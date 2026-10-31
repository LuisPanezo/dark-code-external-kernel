#pragma once
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "driver.h"
#include "scanner.h"
#include "sources/frame_source.h"
#include "types.h"
#include "ue.h"

namespace dc::esp {

/*
 * Fuente de frames que lee Delta Force desde el proceso del juego.
 *
 *   UWorld → PersistentLevel → Actores → {posición, vida, equipo, nombre}
 *   APlayerCameraManager::CameraCache → POV
 *
 * Publica un dc::FrameSnapshot canónico (metros) usando el adaptador Unreal
 * que ya existe (adapters/normalize). El snapshot sale listo para el motor
 * de escena y para el proyector de pantalla.
 *
 * Sólo el hilo de adquisición llama Start/ReadLatest/Stop (contrato de
 * IFrameSource). No hace falta root aquí: el driver ya lo exigió al abrir.
 */
class EspReader final : public dc::IFrameSource {
public:
    EspReader(std::unique_ptr<IMemoryDriver> driver, std::unique_ptr<IProcessMap> map, Options options);
    ~EspReader() override;

    dc::Status Start() override;
    dc::Status ReadLatest(dc::FrameSnapshot& output) override;
    void Stop() noexcept override;

    /* Última lista aceptada, en centímetros de Unreal. */
    const std::vector<Target>& Targets() const { return targets_; }
    const Stats&  Statistics() const { return stats_; }
    const CameraState& Camera() const { return camera_; }
    const std::string& StatusText() const { return status_; }
    State   CurrentState() const { return state_; }
    pid_t   ProcessId() const { return pid_; }
    uintptr_t World() const { return world_; }
    uintptr_t WorldVariable() const { return world_variable_; }
    ModuleSpan Module() const { return ModuleSpan{module_start_, module_end_}; }
    float   HorizontalFovRadians() const { return horizontal_fov_radians_; }
    const Options& Settings() const { return options_; }
    int32_t SelfTeam() const { return self_team_; }
    uintptr_t SelfPawn() const { return self_team_actor_; }
    void    SetAspect(float aspect);
    void    SetOptions(const Options& options) { options_ = options; }

private:
    struct CachedActor {
        uintptr_t   root{};
        uintptr_t   health_set{};
        uintptr_t   team_component{};
        uintptr_t   player_state{};
        uintptr_t   mesh{};
        uintptr_t   capsule{};
        int32_t     team{-1};
        float       eye_cm{0.f};
        float       half_height_cm{0.f};
        std::string name;
        int         ttl{0};
        uint64_t    last_tick{0};
    };

    bool   OpenDriver();
    bool   LocateRanges();
    bool   ScanWorld();
    bool   EnsureWorld();
    void   RefreshActor(uintptr_t actor, CachedActor& cached);
    int32_t TeamOf(uintptr_t actor, CachedActor& cached);
    bool   IsModuleObject(uintptr_t ptr) const;
    void   PruneCache();

    std::unique_ptr<IMemoryDriver> driver_;
    std::unique_ptr<IProcessMap>   process_map_;
    Options     options_;
    pid_t       pid_{-1};
    uintptr_t   world_variable_{};
    uintptr_t   world_{};
    uintptr_t   module_start_{};
    uintptr_t   module_end_{};
    uint64_t    last_scan_ns_{};
    bool        running_{};

    std::vector<AddressRange> ranges_;
    std::vector<Target>       targets_;
    std::unordered_map<uintptr_t, CachedActor> cache_;
    std::unordered_set<uintptr_t> seen_;
    uint64_t    tick_{};
    int32_t     self_team_{-1};
    uintptr_t   self_team_actor_{};
    int         self_team_ttl_{};

    CameraState camera_;
    float       horizontal_fov_radians_{0.f};
    Stats       stats_;
    State       state_{State::Detached};
    std::string status_{"Desconectado"};
};

} // namespace dc::esp
