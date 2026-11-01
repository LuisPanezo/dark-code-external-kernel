#include "engine.h"

#include "unreal/reader.h"
#include "unreal/unreal_types.h"

namespace dc::esp {

/*
 * Factory de engines. Aquí se mapea la configuración genérica (EspConfig) a la
 * configuración concreta de cada motor. Para Unity se añade un EngineKind::Unity
 * y una clase en native/esp/unity/ que implemente IEspEngine.
 */
std::unique_ptr<IEspEngine> MakeEngine(EngineKind kind, const EspConfig& config,
                                       std::unique_ptr<IMemoryDriver> driver,
                                       std::unique_ptr<IProcessMap> map) {
    if (kind == EngineKind::Unreal) {
        Options options;
        if (!config.package.empty()) options.package = config.package;
        options.pid = config.pid;
        if (config.max_distance_m > 0.f) options.max_distance_m = config.max_distance_m;
        if (config.min_health > 0.f) options.min_health = config.min_health;
        options.skip_teammates = config.skip_teammates;
        if (config.poll_interval_ms > 0) options.poll_interval_ms = config.poll_interval_ms;
        return std::make_unique<EspReader>(std::move(driver), std::move(map),
                                           std::move(options));
    }
    // EngineKind::Unity: pendiente de implementar.
    return nullptr;
}

} // namespace dc::esp
