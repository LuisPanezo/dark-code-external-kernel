#include "api/esp_api.h"

#include <string>

#include "api/esp_service.h"

namespace {

/* thread_local: el puntero devuelto sigue válido hasta la próxima llamada
 * en el mismo hilo, sin carreras con el hilo de adquisición. */
std::string& Scratch() {
    static thread_local std::string text;
    return text;
}

} // namespace

extern "C" {

int dc_esp_attach_config(const DcEspConfig* config) {
    dc::esp::EspConfig options;
    if (config) {
        if (config->package && *config->package) options.package = config->package;
        if (config->pid > 0) options.pid = static_cast<pid_t>(config->pid);
        if (config->max_distance_m > 0) {
            options.max_distance_m = static_cast<float>(config->max_distance_m);
        }
        if (config->min_health > 0) options.min_health = static_cast<float>(config->min_health);
        options.skip_teammates = config->skip_teammates != 0;
        if (config->poll_interval_ms > 0) options.poll_interval_ms = config->poll_interval_ms;
    }
    return dc::esp::EspService::Instance().Attach(options, dc::esp::EngineKind::Unreal);
}

int dc_esp_attach(const char* package_or_null, int pid) {
    DcEspConfig config{};
    config.package = package_or_null;
    config.pid = pid;
    config.skip_teammates = 1;
    return dc_esp_attach_config(&config);
}

void dc_esp_detach(void) { dc::esp::EspService::Instance().Detach(); }

int dc_esp_attached(void) { return dc::esp::EspService::Instance().Attached() ? 1 : 0; }

int dc_esp_set_enabled(int enabled) {
    return dc::esp::EspService::Instance().SetEnabled(enabled != 0) ? 1 : 0;
}

int dc_esp_enabled(void) { return dc::esp::EspService::Instance().Enabled() ? 1 : 0; }

int dc_esp_state(void) {
    return static_cast<int>(dc::esp::EspService::Instance().CurrentState());
}

const char* dc_esp_status(void) {
    Scratch() = dc::esp::EspService::Instance().Status();
    return Scratch().c_str();
}

const char* dc_esp_last_error(void) {
    Scratch() = dc::esp::EspService::Instance().LastError();
    return Scratch().c_str();
}

int dc_esp_target_count(void) { return dc::esp::EspService::Instance().TargetCount(); }

int dc_esp_poll(float screen_width, float screen_height, DcEspBox* boxes, int capacity) {
    return dc::esp::EspService::Instance().Poll(screen_width, screen_height, boxes, capacity);
}

int dc_esp_view(DcEspView* view) { return dc::esp::EspService::Instance().CopyView(view); }

} // extern "C"
