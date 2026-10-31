#include "api/esp_service.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

#include "projector.h"

namespace dc::esp {
namespace {

static_assert(DC_ESP_MAX_ENTITIES == static_cast<int>(dc::kMaxEntities),
              "DC_ESP_MAX_ENTITIES debe coincidir con dc::kMaxEntities");

constexpr float kMinAspect = 0.3f;
constexpr float kMaxAspect = 4.f;

} // namespace

EspService& EspService::Instance() {
    static EspService instance;
    return instance;
}

EspService::~EspService() { Detach(); }

int EspService::Attach(const Options& options, std::unique_ptr<IMemoryDriver> driver,
                       std::unique_ptr<IProcessMap> map) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (attached_) return DC_ESP_RESULT_ALREADY;
    }

    auto owned_driver = driver ? std::move(driver) : MakePhmeopDriver();
    auto owned_map = map ? std::move(map) : MakeLinuxProcessMap();
    auto reader = std::make_unique<EspReader>(std::move(owned_driver), std::move(owned_map),
                                              options);

    std::lock_guard<std::mutex> lock(mutex_);
    if (attached_) return DC_ESP_RESULT_ALREADY;

    options_ = options;
    reader_ = std::move(reader);
    attached_ = true;
    running_ = true;
    enabled_.store(true);
    state_ = State::Starting;
    status_ = "Iniciando…";
    error_.clear();

    try {
        worker_ = std::thread(&EspService::Loop, this);
    } catch (...) {
        reader_.reset();
        attached_ = false;
        running_ = false;
        state_ = State::Error;
        error_ = "no se pudo crear el hilo del ESP";
        return DC_ESP_RESULT_FAILED;
    }
    return DC_ESP_RESULT_OK;
}

void EspService::Detach() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!attached_ && !worker_.joinable()) return;
        running_ = false;
    }
    wake_.notify_all();
    if (worker_.joinable()) worker_.join();

    std::lock_guard<std::mutex> lock(mutex_);
    reader_.reset();
    latest_.reset();
    attached_ = false;
    state_ = State::Detached;
    status_ = "Desconectado";
    error_.clear();
}

bool EspService::SetEnabled(bool enabled) {
    enabled_.store(enabled);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!enabled && state_ != State::Detached) status_ = "Desactivado";
    }
    wake_.notify_all();
    return enabled;
}

bool EspService::Enabled() const { return enabled_.load(); }

bool EspService::Attached() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return attached_;
}

State EspService::CurrentState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

std::string EspService::Status() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

std::string EspService::LastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_;
}

std::shared_ptr<const EspFrame> EspService::Latest() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return latest_;
}

int EspService::TargetCount() const {
    std::shared_ptr<const EspFrame> frame = Latest();
    return frame ? static_cast<int>(frame->targets.size()) : 0;
}

void EspService::SetAspect(float aspect) {
    if (!(aspect > kMinAspect) || !(aspect < kMaxAspect)) return;
    std::lock_guard<std::mutex> lock(mutex_);
    aspect_ = aspect;
}

void EspService::Publish(std::shared_ptr<const EspFrame> frame) {
    std::lock_guard<std::mutex> lock(mutex_);
    latest_ = std::move(frame);
    state_ = State::Live;
}

void EspService::PublishStatus(State state, const std::string& text) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
    status_ = text;
}

void EspService::Loop() {
    using clock = std::chrono::steady_clock;

    auto next_attempt = clock::now();
    bool started = false;
    dc::FrameSnapshot frame;

    while (true) {
        int interval_ms = 16;
        float aspect = 16.f / 9.f;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) break;
            interval_ms = std::max(options_.poll_interval_ms, 1);
            aspect = aspect_;
        }

        if (!enabled_.load()) {
            std::unique_lock<std::mutex> lock(mutex_);
            wake_.wait_for(lock, std::chrono::milliseconds(100),
                           [this] { return !running_ || enabled_.load(); });
            continue;
        }

        const auto now = clock::now();
        if (!started && now >= next_attempt) {
            const dc::Status status = reader_->Start();
            started = status == dc::Status::Ok;
            if (!started) next_attempt = now + std::chrono::milliseconds(2000);
            PublishStatus(reader_->CurrentState(), reader_->StatusText());
        }

        if (started) {
            reader_->SetAspect(aspect);
            const dc::Status status = reader_->ReadLatest(frame);
            if (status == dc::Status::Ok) {
                auto published = std::make_shared<EspFrame>();
                published->snapshot = frame;
                published->targets = reader_->Targets();
                published->horizontal_fov_radians = reader_->HorizontalFovRadians();
                published->tick = reader_->Statistics().ticks;
                Publish(std::move(published));
            } else if (status == dc::Status::Disconnected) {
                started = false;
                next_attempt = clock::now() + std::chrono::milliseconds(1500);
            }
            PublishStatus(reader_->CurrentState(), reader_->StatusText());
        }

        std::unique_lock<std::mutex> lock(mutex_);
        wake_.wait_for(lock, std::chrono::milliseconds(interval_ms),
                       [this] { return !running_; });
    }

    if (reader_) reader_->Stop();
}

int EspService::Poll(float width, float height, DcEspBox* out, int capacity) {
    if (!out || capacity <= 0 || !(width > 1.f) || !(height > 1.f)) return 0;
    if (!enabled_.load()) return 0;

    std::shared_ptr<const EspFrame> frame;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        frame = latest_;
        const float aspect = width / height;
        if (std::isfinite(aspect) && aspect > kMinAspect && aspect < kMaxAspect) aspect_ = aspect;
    }
    if (!frame) return 0;
    // Un frame viejo deja cajas fantasma (menú, muerte, cambio de mapa).
    if (!dc::IsFresh(frame->snapshot, dc::MonotonicNs(), 500000000)) return 0;

    const size_t count = std::min(frame->snapshot.entities.size(), frame->targets.size());
    int written = 0;
    for (size_t index = 0; index < count && written < capacity; ++index) {
        const ScreenBox box = ProjectEntity(frame->snapshot, frame->horizontal_fov_radians, index,
                                            width, height);
        if (!box.visible) continue;

        const Target& target = frame->targets[index];
        DcEspBox& item = out[written];
        item = DcEspBox{};
        item.actor = target.actor;
        item.x = box.x;
        item.y = box.y;
        item.w = box.w;
        item.h = box.h;
        item.distance_m = target.distance_m;
        item.health = target.health;
        item.team = target.team;
        if (target.encrypted) item.flags |= DC_ESP_FLAG_ENCRYPTED;
        if (target.teammate) item.flags |= DC_ESP_FLAG_TEAMMATE;
        if (target.health < 0.f) item.flags |= DC_ESP_FLAG_NO_HEALTH;
        else if (target.health <= 0.5f) item.flags |= DC_ESP_FLAG_DEAD;
        std::snprintf(item.name, sizeof(item.name), "%s", target.name.c_str());
        ++written;
    }
    return written;
}

int EspService::CopyView(DcEspView* view) const {
    if (!view) return 0;
    std::shared_ptr<const EspFrame> frame = Latest();
    *view = DcEspView{};
    if (!frame) return 0;

    const dc::CameraFrame& camera = frame->snapshot.camera;
    view->camera_position[0] = camera.position.x;
    view->camera_position[1] = camera.position.y;
    view->camera_position[2] = camera.position.z;
    view->camera_target[0] = camera.target.x;
    view->camera_target[1] = camera.target.y;
    view->camera_target[2] = camera.target.z;
    view->camera_up[0] = camera.up.x;
    view->camera_up[1] = camera.up.y;
    view->camera_up[2] = camera.up.z;
    view->vertical_fov_radians = camera.vertical_fov_radians;
    view->near_plane = camera.near_plane;
    view->far_plane = camera.far_plane;
    view->horizontal_fov_radians = frame->horizontal_fov_radians;

    const size_t count = std::min({frame->snapshot.entities.size(), frame->targets.size(),
                                   static_cast<size_t>(DC_ESP_MAX_ENTITIES)});
    for (size_t index = 0; index < count; ++index) {
        const dc::Entity& entity = frame->snapshot.entities[index];
        DcEspEntityView& item = view->entities[index];
        item.actor = frame->targets[index].actor;
        item.center[0] = entity.position.x;
        item.center[1] = entity.position.y;
        item.center[2] = entity.position.z;
        item.half_extents[0] = entity.half_extents.x;
        item.half_extents[1] = entity.half_extents.y;
        item.half_extents[2] = entity.half_extents.z;
        item.rgba[0] = entity.color.r;
        item.rgba[1] = entity.color.g;
        item.rgba[2] = entity.color.b;
        item.rgba[3] = entity.color.a;
    }
    view->count = static_cast<int32_t>(count);
    return static_cast<int>(count);
}

} // namespace dc::esp
