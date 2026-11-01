#pragma once
#include <memory>
#include <string>
#include <vector>

#include "core/frame_snapshot.h"
#include "core/status.h"
#include "driver.h"
#include "target.h"
#include "types.h"   // State

namespace dc::esp {

/* Configuración genérica de un lector (sin nada específico de motor). */
struct EspConfig {
    std::string package;            // paquete del proceso del juego
    pid_t       pid{-1};            // <= 0: buscar por paquete
    float       max_distance_m{0.f};// <= 0: sin límite
    float       min_health{0.f};    // 0: sin filtro
    bool        skip_teammates{true};
    int         poll_interval_ms{0};
};

/* Tipo de motor: la "costura" para añadir Unity sin tocar el núcleo. */
enum class EngineKind { Unreal, Unity };

/*
 * Interfaz que implementa cada adaptador de motor. El EspService solo conoce
 * esta interfaz; nunca a un motor concreto. Para añadir Unity solo se escribe
 * una clase que la implemente y se registra en MakeEngine().
 */
class IEspEngine {
public:
    virtual ~IEspEngine() = default;

    virtual dc::Status Start() = 0;
    virtual dc::Status ReadLatest(dc::FrameSnapshot& snapshot,
                                  std::vector<EspTarget>& targets,
                                  float& horizontal_fov_radians) = 0;
    virtual void Stop() noexcept = 0;
    virtual void SetAspect(float aspect) = 0;
    virtual State CurrentState() const = 0;
    virtual std::string StatusText() const = 0;
};

/*
 * Factory agnóstica. Devuelve el lector del motor indicado. La implementación
 * de Unreal vive en unreal/engine_factory.cpp; la de Unity se añadirá en
 * unity/engine_factory.cpp y se conecta aquí.
 */
std::unique_ptr<IEspEngine> MakeEngine(EngineKind kind, const EspConfig& config,
                                       std::unique_ptr<IMemoryDriver> driver = {},
                                       std::unique_ptr<IProcessMap> map = {});

} // namespace dc::esp
