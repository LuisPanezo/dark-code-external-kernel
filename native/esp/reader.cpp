#include "reader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "abi.h"
#include "adapters/normalize.h"
#include "core/frame_snapshot.h"
#include "core/status.h"
#include "elf.h"
#include "offsets.h"

namespace dc::esp {
namespace {

constexpr uint64_t kRescanNs = 2000000000ULL;   // 2 s entre reescaneos
constexpr float    kMinHeightCm = 40.f;    // mínimo (tumbado); el de pie es mayor
constexpr float    kMaxHeightCm = 220.f;   // y máximo (humano/bot alto)
constexpr float    kDegreesToRadians = 3.14159265358979323846f / 180.f;

struct ActorsArray { uintptr_t data; uint32_t num; uint32_t max; };

dc::Color ColorFor(bool teammate, float health) {
    if (teammate) return {0.35f, 0.85f, 0.55f, 1.f};
    if (health >= 0.f && health <= 25.f) return {1.f, 0.45f, 0.25f, 1.f};
    return {1.f, 0.38f, 0.38f, 1.f};
}

std::string Joined(const char* prefix, size_t value) {
    char buffer[96];
    std::snprintf(buffer, sizeof(buffer), "%s%zu", prefix, value);
    return buffer;
}

} // namespace

EspReader::EspReader(std::unique_ptr<IMemoryDriver> driver, std::unique_ptr<IProcessMap> map,
                     Options options)
    : driver_(std::move(driver)), process_map_(std::move(map)), options_(std::move(options)) {}

EspReader::~EspReader() { Stop(); }

dc::Status EspReader::Start() {
    Stop();
    state_ = State::Starting;
    status_ = "Conectando…";

    pid_ = options_.pid > 0 ? options_.pid
                            : FindProcessId(options_.package.c_str(), options_.module.c_str());
    if (pid_ <= 0) {
        state_ = State::NoPid;
        status_ = "Juego no encontrado: " + options_.package;
        return dc::Status::NoData;
    }
    driver_->set_pid(pid_);

    if (!OpenDriver()) return dc::Status::Disconnected;
    if (!LocateRanges()) return dc::Status::NoData;
    if (!ScanWorld()) return dc::Status::NoData;

    running_ = true;
    return dc::Status::Ok;
}

void EspReader::Stop() noexcept {
    running_ = false;
    driver_->close();
    targets_.clear();
    cache_.clear();
    seen_.clear();
    ranges_.clear();
    world_ = 0;
    world_variable_ = 0;
    camera_ = CameraState{};
    horizontal_fov_radians_ = 0.f;
    self_team_ = -1;
    self_team_actor_ = 0;
    self_team_ttl_ = 0;
    pid_ = -1;
    state_ = State::Detached;
    status_ = "Desconectado";
}

void EspReader::SetAspect(float aspect) {
    if (aspect > 0.1f && aspect < 10.f) options_.aspect = aspect;
}

bool EspReader::OpenDriver() {
    if (driver_->is_open()) return true;
    std::string error;
    if (!driver_->open(&error)) {
        state_ = State::NoDriver;
        status_ = "Driver " DC_ESP_DEVICE ": " + error;
        return false;
    }
    return true;
}

bool EspReader::LocateRanges() {
    ranges_.clear();
    module_start_ = 0;
    module_end_ = 0;

    // 1) Program headers del ELF: cubre .data y el .bss anónimo de Android.
    const uintptr_t base = driver_->module_base(options_.module.c_str());
    if (base) {
        const ModuleSegments segments = ReadModuleSegments(*driver_, base);
        if (segments.valid) {
            ranges_ = segments.writable;
            module_start_ = segments.base;
            module_end_ = segments.end;
        }
    }
    // 2) /proc/<pid>/maps (sólo si el ELF no dio nada).
    if (ranges_.empty() && process_map_ &&
        !process_map_->writable_module_ranges(pid_, options_.module.c_str(), ranges_)) {
        ranges_.clear();
    }
    // 3) ioctl OP_MODULE_DATA_RANGE (driver nuevo).
    if (ranges_.empty()) {
        AddressRange range{};
        if (driver_->module_data_range(options_.module.c_str(), range)) ranges_.push_back(range);
    }
    if (ranges_.empty()) {
        state_ = State::NoWorld;
        status_ = "Sin datos RW de " + options_.module +
                  " (¿módulo cargado? ¿/proc restringido?)";
        return false;
    }
    return true;
}

bool EspReader::ScanWorld() {
    last_scan_ns_ = dc::MonotonicNs();

    if (options_.gworld) {
        world_variable_ = options_.gworld;
        world_ = ReadPtr(*driver_, world_variable_);
        if (!world_ || WorldScore(*driver_, world_, ModuleSpan{module_start_, module_end_}) < 2) {
            world_ = 0;
            state_ = State::NoWorld;
            status_ = "GWorld forzado no válido en 0x" + std::to_string(world_variable_);
            return false;
        }
        state_ = State::Starting;
        status_ = "GWorld forzado";
        return true;
    }

    ScanOptions scan;
    scan.module_start = module_start_;
    scan.module_end = module_end_;
    WorldScan found = ScanForWorld(*driver_, ranges_, scan);
    stats_.last_errno = driver_->last_errno();
    if (!found.variable) {
        world_variable_ = 0;
        world_ = 0;
        state_ = State::NoWorld;
        status_ = Joined("GWorld no encontrado (escaneados ", found.bytes_scanned) + " B)";
        return false;
    }
    world_variable_ = found.variable;
    world_ = found.world;
    state_ = State::Starting;
    status_ = (found.strong ? "GWorld confirmado (" : "GWorld candidato (") +
              Joined("", found.bytes_scanned) + " B, score " + std::to_string(found.score) + ")";
    return true;
}

bool EspReader::EnsureWorld() {
    if (world_variable_) {
        uintptr_t world = ReadPtr(*driver_, world_variable_);
        if (world && WorldScore(*driver_, world, ModuleSpan{module_start_, module_end_}) >= 2) {
            world_ = world;
            return true;
        }
    }
    if (!options_.rescan) return false;
    if (dc::MonotonicNs() - last_scan_ns_ < kRescanNs) return false;
    if (ranges_.empty() && !LocateRanges()) return false;
    return ScanWorld();
}

void EspReader::RefreshActor(uintptr_t actor, CachedActor& cached) {
    cached.root = ReadPtr(*driver_, actor + off::ROOT_COMPONENT);
    cached.mesh = ReadPtr(*driver_, actor + off::MESH);

    cached.health_set = 0;
    ResolveHealthSet(*driver_, actor, cached.health_set);

    cached.team_component = 0;
    ResolveTeamComponent(*driver_, actor, cached.team_component);

    cached.player_state = ReadPtr(*driver_, actor + off::PLAYER_STATE);
    cached.team = -1;
    // Equipo/campo real (TeamComp + 0x10C): agrupa escuadras aliadas.
    // Sin TeamComp (bot/actor sin estado) queda en -1 (sin equipo).
    if (cached.team_component) {
        ReadCampValue(*driver_, cached.team_component, cached.team);
    }

    cached.eye_cm = ReadEyeHeight(*driver_, actor, options_.default_eye_cm);

    // Cápsula → media-altura: refleja agachado/tumbado (86 cm de pie).
    cached.capsule = ReadPtr(*driver_, actor + off::CAPSULE);
    cached.half_height_cm = 0.f;
    if (PlausiblePointer(cached.capsule)) {
        float half = 0.f;
        if (Read(*driver_, cached.capsule + off::CAPSULE_HALF_HEIGHT, half) &&
            half >= 20.f && half <= 120.f) {
            cached.half_height_cm = half;
        }
    }

    cached.name.clear();
    if (cached.player_state) ReadPlayerName(*driver_, cached.player_state, cached.name);
}

/* UObject real = puntero creíble con vtable dentro del módulo cargado. */
bool EspReader::IsModuleObject(uintptr_t ptr) const {
    if (!PlausiblePointer(ptr)) return false;
    const uintptr_t vtable = ReadPtr(*driver_, ptr);
    return vtable >= module_start_ && vtable < module_end_;
}

int32_t EspReader::TeamOf(uintptr_t actor, CachedActor& cached) {
    (void)actor;
    if (cached.team < 0 && cached.team_component) {
        ReadCampValue(*driver_, cached.team_component, cached.team);
    }
    return cached.team;
}

void EspReader::PruneCache() {
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second.last_tick == tick_) ++it;
        else it = cache_.erase(it);
    }
}

dc::Status EspReader::ReadLatest(dc::FrameSnapshot& output) {
    output = dc::FrameSnapshot{};
    ++tick_;

    targets_.clear();
    seen_.clear();
    seen_roots_.clear();
    stats_.ticks = tick_;
    stats_.actors_in_level = 0;
    stats_.actors_considered = 0;
    stats_.accepted = 0;
    stats_.rejected_pointer = 0;
    stats_.rejected_position = 0;
    stats_.rejected_distance = 0;
    stats_.teammates = 0;
    stats_.encrypted = 0;
    stats_.plain = 0;
    stats_.unreadable = 0;
    stats_.duplicates = 0;

    if (!driver_->is_open()) {
        state_ = State::NoDriver;
        status_ = "Driver cerrado";
        return dc::Status::Disconnected;
    }
    if (!EnsureWorld()) {
        state_ = State::NoWorld;
        status_ = "GWorld no disponible (¿menú principal o carga?)";
        return dc::Status::NoData;
    }

    PlayerChain chain;
    if (!ReadPlayerChain(*driver_, world_, chain)) {
        state_ = State::NoCamera;
        status_ = "Cadena del jugador local incompleta";
        return dc::Status::NoData;
    }
    if (!ReadCamera(*driver_, chain, options_.hfov_degrees, options_.fov_horizontal, camera_)) {
        state_ = State::NoCamera;
        status_ = "Cámara no disponible (¿carga, muerte o espectador?)";
        stats_.last_errno = driver_->last_errno();
        return dc::Status::NoData;
    }

    const float fov_radians = camera_.fov_degrees * kDegreesToRadians;
    const float vertical_fov = camera_.fov_horizontal
        ? VerticalFovFromHorizontal(fov_radians, options_.aspect)
        : fov_radians;
    horizontal_fov_radians_ = camera_.fov_horizontal
        ? fov_radians
        : HorizontalFovFromVertical(fov_radians, options_.aspect);
    if (!(vertical_fov > 0.05f) || vertical_fov > 3.f) {
        state_ = State::NoCamera;
        status_ = "FOV fuera de rango";
        return dc::Status::NoData;
    }

    const uintptr_t level = ReadPtr(*driver_, world_ + off::PERSISTENT_LEVEL);
    if (!PlausiblePointer(level)) {
        state_ = State::NoWorld;
        status_ = "Nivel no disponible";
        return dc::Status::NoData;
    }
    ActorsArray actors{};
    if (!Read(*driver_, level + off::ACTORS, actors)) return dc::Status::NoData;
    if (!PlausiblePointer(actors.data) || actors.num == 0 ||
        actors.num > static_cast<uint32_t>(off::MAX_ACTORS)) {
        state_ = State::NoWorld;
        status_ = "Tabla de actores no disponible";
        return dc::Status::NoData;
    }

    stats_.actors_in_level = static_cast<int>(actors.num);
    std::vector<uintptr_t> list(static_cast<size_t>(actors.num));
    const size_t got = driver_->read(actors.data, list.data(), list.size() * sizeof(uintptr_t));
    const size_t count = got / sizeof(uintptr_t);
    if (count == 0) return dc::Status::NoData;

    if (self_team_actor_ != chain.pawn || --self_team_ttl_ <= 0) {
        self_team_actor_ = chain.pawn;
        self_team_ = -1;
        // Equipo/campo local (TeamComp + 0x10C): más amplio que la escuadra.
        uintptr_t component = 0;
        if (ResolveTeamComponent(*driver_, chain.pawn, component)) {
            ReadCampValue(*driver_, component, self_team_);
        }
        if (self_team_ < 0) {
            const uintptr_t state = ReadPtr(*driver_, chain.pawn + off::PLAYER_STATE);
            if (state) ReadTeamFallback(*driver_, state, self_team_);
        }
        self_team_ttl_ = std::max(options_.cache_ttl_ticks, 1);
    }

    const DecodeLimits limits;
    for (size_t index = 0; index < count; ++index) {
        const uintptr_t actor = list[index];
        if (!PlausiblePointer(actor) || actor == chain.pawn) continue;
        if (!seen_.insert(actor).second) continue;
        ++stats_.actors_considered;

        CachedActor& cached = cache_[actor];
        cached.last_tick = tick_;
        if (cached.ttl-- <= 0) {
            RefreshActor(actor, cached);
            cached.ttl = std::max(options_.cache_ttl_ticks, 1);
        }
        if (!PlausiblePointer(cached.root)) {
            ++stats_.rejected_pointer;
            continue;
        }

        // Dedup por RootComponent: varios actores pueden compartir el mismo
        // root (alias/componentes) y duplicarían la caja sobre el personaje.
        if (!seen_roots_.insert(cached.root).second) {
            ++stats_.duplicates;
            continue;
        }

        // Rechazo temprano de props: si no tiene mesh/name/vida/equipo, no
        // malgastamos la lectura de posición (cientos de decorados). Usa solo
        // campos cacheados, así que no cuesta ioctl extra y sube la fluidez.
        if (options_.require_character) {
            const bool likely = IsModuleObject(cached.mesh) || !cached.name.empty() ||
                                cached.health_set != 0 ||
                                (cached.team >= 0 && cached.team <= 16);
            if (!likely) {
                ++stats_.rejected_position;
                continue;
            }
        }

        FEncVector raw{};
        if (!Read(*driver_, cached.root + off::REL_LOCATION, raw)) {
            cached.ttl = 0;
            ++stats_.rejected_position;
            continue;
        }

        const DecodeResult decoded = DecodeVector(raw, &camera_.location, limits, options_.codec);
        if (decoded.flag) ++stats_.encrypted;
        if (!decoded.plausible) {
            cached.ttl = 0;
            ++stats_.unreadable;
            continue;
        }
        if (decoded.path == Codec::Plain) ++stats_.plain;

        const float distance_m = Length(decoded.value - camera_.location) * 0.01f;
        if (options_.max_distance_m > 0.f && distance_m > options_.max_distance_m) {
            ++stats_.rejected_distance;
            continue;
        }

        int32_t team = TeamOf(actor, cached);
        const bool teammate = self_team_ >= 0 && team >= 0 && team == self_team_;
        if (teammate) {
            ++stats_.teammates;
            if (options_.skip_teammates) continue;
        }

        float health = -1.f;
        if (cached.health_set) ReadHealthValue(*driver_, cached.health_set, health);

        // Cadáver (vida exactamente 0): ya eliminado, no dibujar la caja. Los
        // bots tienen -1 (sin vida legible) y se mantienen como objetivos.
        if (health == 0.f) {
            ++stats_.rejected_position;
            continue;
        }

        // Personaje REAL: Mesh válido (ACharacter), nombre (jugador humano),
        // vida real (bot, >=1) o equipo sano (1..16). Los props no cumplen nada.
        if (options_.require_character) {
            const bool character = IsModuleObject(cached.mesh) || !cached.name.empty() ||
                                   health >= 1.f || (team >= 0 && team <= 16);
            if (!character) {
                ++stats_.rejected_position;
                continue;
            }
        }
        if (options_.min_health > 0.f && !(health >= options_.min_health)) {
            ++stats_.rejected_position;
            continue;
        }

        // Altura por cápsula (agachado/tumbado); respaldo: eye_cm.
        // +8 cm de margen de cabeza: la cápsula termina en el cuello.
        float height_cm;
        if (cached.half_height_cm > 0.f) {
            height_cm = cached.half_height_cm * 2.f + 8.f;
        } else {
            const float eye_cm = cached.eye_cm > 0.f ? cached.eye_cm : options_.default_eye_cm;
            height_cm = eye_cm + options_.eye_extra_cm;
        }
        height_cm = std::min(std::max(height_cm, kMinHeightCm), kMaxHeightCm);

        Target target;
        target.actor = actor;
        target.feet_cm = decoded.value;
        target.height_cm = height_cm;
        target.width_cm = height_cm * options_.box_width_ratio;
        target.distance_m = distance_m;
        target.health = health;
        target.team = team;
        target.encrypted = decoded.flag;
        target.teammate = teammate;
        target.name = cached.name;

        // Esqueleto: solo personajes cercanos (los lejanos son puntos diminutos).
        if (distance_m < 120.f && cached.mesh) {
            target.bone_count = ReadBones(*driver_, cached.mesh, cached.capsule,
                                          decoded.value, target.bones);
        }

        targets_.push_back(std::move(target));
    }
    PruneCache();

    std::sort(targets_.begin(), targets_.end(),
              [](const Target& a, const Target& b) { return a.distance_m < b.distance_m; });
    if (targets_.size() > dc::kMaxEntities) targets_.resize(dc::kMaxEntities);
    stats_.accepted = static_cast<int>(targets_.size());

    dc::TelemetryFrame input;
    input.sequence = tick_;
    input.timestamp_ns = dc::MonotonicNs();
    input.camera.position = camera_.location;

    Vec3 forward{}, right{}, up{};
    BasisFromRotator(camera_.rotation, forward, right, up);
    input.camera.target = camera_.location + forward * 1000.f;
    input.camera.up = Unit(up);
    input.camera.vertical_fov_radians = vertical_fov;
    input.camera.near_plane = 1.f;    // 1 cm: no descarta enemigos pegados
    input.camera.far_plane =
        std::max(30000.f, (options_.max_distance_m + 50.f) * 100.f);

    input.entities.reserve(targets_.size());
    for (const Target& target : targets_) {
        dc::Entity entity;
        entity.id = target.actor;
        entity.position = {target.feet_cm.x, target.feet_cm.y,
                           target.feet_cm.z + target.height_cm * 0.5f};
        entity.half_extents = {target.width_cm * 0.5f, target.width_cm * 0.5f,
                               target.height_cm * 0.5f};
        entity.color = ColorFor(target.teammate, target.health);
        entity.label = target.name.empty() ? "Objetivo" : target.name;
        input.entities.push_back(std::move(entity));
    }

    if (dc::Normalize(input, dc::Coordinates::UnrealWorldCentimeters, output) != dc::Status::Ok) {
        state_ = State::Error;
        status_ = "Snapshot inválido";
        targets_.clear();
        return dc::Status::InvalidFrame;
    }

    state_ = State::Live;
    status_ = Joined("En vivo · ", targets_.size()) + " objetivos";
    stats_.last_errno = driver_->last_errno();
    return dc::Status::Ok;
}

} // namespace dc::esp
