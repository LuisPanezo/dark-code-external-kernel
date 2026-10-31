#include "esp_tests.h"

#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

#include "api/esp_service.h"
#include "core/frame_snapshot.h"
#include "offsets.h"
#include "projector.h"
#include "reader.h"
#include "scanner.h"
#include "tests/mock_driver.h"
#include "ue.h"

namespace dc::esp {
namespace {

void Need(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

bool Near(float a, float b, float epsilon = 1e-4f) {
    return std::fabs(a - b) < epsilon;
}

/* Direcciones del escenario simulado: todas alineadas y en espacio de usuario. */
constexpr uintptr_t kDataStart   = 0x700000000000ULL;
constexpr uintptr_t kDataEnd     = 0x700000010000ULL;
constexpr uintptr_t kGlobal      = 0x700000001000ULL;
constexpr uintptr_t kWorld       = 0x710000000000ULL;
constexpr uintptr_t kLevel       = 0x720000000000ULL;
constexpr uintptr_t kInstance    = 0x730000000000ULL;
constexpr uintptr_t kActorArray  = 0x740000000000ULL;
constexpr uintptr_t kPawn        = 0x750000000000ULL;
constexpr uintptr_t kEnemy       = 0x750000000008ULL;
constexpr uintptr_t kGhost       = 0x750000000010ULL;   // sin RootComponent
constexpr uintptr_t kLocalArray  = 0x760000000000ULL;
constexpr uintptr_t kLocalPlayer = 0x770000000000ULL;
constexpr uintptr_t kController  = 0x780000000000ULL;
constexpr uintptr_t kCamera      = 0x790000000000ULL;
constexpr uintptr_t kPawnTeam    = 0x7A0000000000ULL;
constexpr uintptr_t kRoot        = 0x7B0000000000ULL;
constexpr uintptr_t kEnemyTeam   = 0x7C0000000000ULL;
constexpr uintptr_t kHealthComp  = 0x7D0000000000ULL;
constexpr uintptr_t kHealthSet   = 0x7E0000000000ULL;
constexpr uintptr_t kState       = 0x7F0000000000ULL;
constexpr uintptr_t kNameData    = 0x7F0000001000ULL;

/*
 * Escenario mínimo con los offsets reales:
 *   GWorld → PersistentLevel (3 actores) + GameInstance → LocalPlayer → PC
 *   PC → Pawn (equipo 2) y PlayerCameraManager (POV en 60 cm de alto)
 *   Enemigo (equipo 1) con RootComponent, vida 100 y nombre "Ana"
 *   Tercer actor sin RootComponent para comprobar el descarte.
 */
void BuildScene(MockDriver& driver, Vec3 enemy_cm) {
    driver.register_module("libUE4.so", 0x1000000000ULL, AddressRange{kDataStart, kDataEnd});

    driver.put_ptr(kGlobal, kWorld);
    driver.put_ptr(kWorld + off::PERSISTENT_LEVEL, kLevel);
    driver.put_ptr(kWorld + off::GAME_INSTANCE, kInstance);

    const uintptr_t actors[3] = {kPawn, kEnemy, kGhost};
    driver.put_ptr(kLevel + off::ACTORS, kActorArray + 0x10);
    driver.put_i32(kLevel + off::ACTORS + 8, 3);
    driver.put_i32(kLevel + off::ACTORS + 12, 4);
    driver.write_bytes(kActorArray + 0x10, actors, sizeof(actors));

    driver.put_ptr(kInstance + off::LOCAL_PLAYERS, kLocalArray + 0x10);
    driver.put_i32(kInstance + off::LOCAL_PLAYERS + 8, 1);
    driver.put_i32(kInstance + off::LOCAL_PLAYERS + 12, 1);
    driver.put_ptr(kLocalArray + 0x10, kLocalPlayer);

    driver.put_ptr(kLocalPlayer + off::PLAYER_CONTROLLER, kController);
    driver.put_ptr(kController + off::ACK_PAWN, kPawn);
    driver.put_ptr(kController + off::CAMERA_MANAGER, kCamera);

    driver.put(kCamera + off::POV_LOCATION, Vec3{0.f, 0.f, 60.f});
    driver.put(kCamera + off::POV_ROTATION, Rotator{0.f, 0.f, 0.f});
    driver.put_float(kCamera + off::POV_FOV, 90.f);

    driver.put_ptr(kPawn + off::TEAM_COMP, kPawnTeam);
    driver.put_i32(kPawnTeam + off::TEAM_ID_FAST, 2);

    driver.put_ptr(kEnemy + off::ROOT_COMPONENT, kRoot);
    const FEncVector position{enemy_cm.x, enemy_cm.y, enemy_cm.z, 0, 0, 0};
    driver.put(kRoot + off::REL_LOCATION, position);

    driver.put_ptr(kEnemy + off::TEAM_COMP, kEnemyTeam);
    driver.put_i32(kEnemyTeam + off::TEAM_ID_FAST, 1);

    driver.put_ptr(kEnemy + off::HEALTH_COMPONENT, kHealthComp);
    driver.put_ptr(kHealthComp + off::HEALTH_SET, kHealthSet);
    driver.put_float(kHealthSet + off::HEALTH, 100.f);

    driver.put_ptr(kEnemy + off::PLAYER_STATE, kState);
    driver.put_fstring(kState + off::PLAYER_NAME, kNameData, 4);
    const char16_t name[4] = {u'A', u'n', u'a', 0};
    driver.write_bytes(kNameData, name, sizeof(name));

    driver.put_float(kEnemy + off::BASE_EYE_HEIGHT, 60.f);
}

Options SceneOptions() {
    Options options;
    options.pid = 4242;
    options.gworld = kGlobal;
    options.module = "libUE4.so";
    options.poll_interval_ms = 5;
    return options;
}

} // namespace

int RunEspTests() {
    int passed = 0;

    // ── Base del rotador ──
    {
        Vec3 forward{}, right{}, up{};
        BasisFromRotator(Rotator{0.f, 0.f, 0.f}, forward, right, up);
        Need(Near(forward.x, 1.f) && Near(forward.z, 0.f), "forward de identidad");
        Need(Near(right.y, 1.f) && Near(up.z, 1.f), "ejes de identidad");
        ++passed;

        BasisFromRotator(Rotator{0.f, 90.f, 0.f}, forward, right, up);
        Need(Near(forward.x, 0.f) && Near(forward.y, 1.f) && Near(forward.z, 0.f), "yaw 90");
        ++passed;

        BasisFromRotator(Rotator{90.f, 0.f, 0.f}, forward, right, up);
        Need(Near(forward.z, 1.f) && Near(Length(forward), 1.f, 1e-3f), "pitch 90");
        ++passed;
    }

    // ── FOV horizontal ↔ vertical ──
    {
        const float horizontal = 1.5707963f;   // 90°
        const float aspect = 16.f / 9.f;
        const float vertical = VerticalFovFromHorizontal(horizontal, aspect);
        Need(vertical > 0.f && vertical < horizontal, "vfov fuera de rango");
        Need(Near(HorizontalFovFromVertical(vertical, aspect), horizontal, 1e-3f), "fov ida y vuelta");
        ++passed;
    }

    // ── Decodificación del FVector ──
    {
        const DecodeLimits limits;

        const FEncVector plain_raw{100.f, 200.f, 300.f, 0, 0, 0};
        const DecodeResult plain = DecodeVector(plain_raw, nullptr, limits, Codec::Auto);
        Need(plain.plausible && plain.path == Codec::Plain, "vector plano rechazado");
        Need(Near(plain.value.x, 100.f), "valor plano alterado");
        ++passed;

        const FEncVector broken{std::numeric_limits<float>::quiet_NaN(), 0.f, 0.f, 0, 1, 0};
        Need(!DecodeVector(broken, nullptr, limits, Codec::Auto).plausible, "NaN aceptado");
        ++passed;

        const Vec3 origin{0.f, 0.f, 0.f};
        const FEncVector distant{100000.f, 0.f, 0.f, 0, 0, 0};
        Need(!DecodeVector(distant, &origin, limits, Codec::Auto).plausible,
             "posición lejos de la referencia aceptada");
        ++passed;

        // index guarda los 16 bits bajos que se quitaron de X.
        const float exact = 12345.5f;
        uint32_t bits = 0;
        std::memcpy(&bits, &exact, sizeof(bits));
        uint32_t truncated_bits = bits & 0xFFFF0000u;
        float truncated = 0.f;
        std::memcpy(&truncated, &truncated_bits, sizeof(truncated));
        FEncVector packed{truncated, 200.f, 300.f, static_cast<uint16_t>(bits & 0xFFFFu), 1, 0};
        const DecodeResult restored = DecodeVector(packed, nullptr, limits, Codec::Trailer);
        Need(restored.plausible && Near(restored.value.x, exact, 0.01f), "restauración del trailer");
        ++passed;
    }

    // ── Escaneo de GWorld ──
    {
        MockDriver driver;
        BuildScene(driver, Vec3{300.f, 60.f, 0.f});
        const std::vector<AddressRange> ranges{{kDataStart, kDataEnd}};
        const WorldScan scan = ScanForWorld(driver, ranges);
        Need(scan.variable == kGlobal, "no encontró la variable GWorld");
        Need(scan.world == kWorld, "GWorld con valor inesperado");
        Need(scan.strong, "no confirmó la cadena del jugador local");
        ++passed;
    }

    // ── Lector completo ──
    {
        auto owned = std::make_unique<MockDriver>();
        MockDriver* mock = owned.get();
        BuildScene(*mock, Vec3{300.f, 60.f, 0.f});

        EspReader reader(std::move(owned), MakeLinuxProcessMap(), SceneOptions());
        Need(reader.Start() == dc::Status::Ok, "Start falló");

        dc::FrameSnapshot frame;
        mock->reads = 0;
        Need(reader.ReadLatest(frame) == dc::Status::Ok, "ReadLatest falló");
        const size_t cold_reads = mock->reads;

        Need(dc::Validate(frame), "snapshot inválido");
        Need(reader.Targets().size() == 1 && frame.entities.size() == 1, "nº de objetivos");
        ++passed;

        const Target& target = reader.Targets().front();
        Need(target.team == 1, "equipo del enemigo");
        Need(Near(target.health, 100.f, 0.01f), "vida del enemigo");
        Need(target.name == "Ana", "nombre del enemigo");
        Need(std::fabs(target.distance_m - 3.1177f) < 0.06f, "distancia del enemigo");
        Need(reader.Statistics().rejected_pointer >= 1, "actor sin RootComponent aceptado");
        ++passed;

        const dc::Entity& entity = frame.entities.front();
        Need(Near(entity.position.x, 0.6f, 1e-3f) &&
             Near(entity.position.y, 0.35f, 1e-3f) &&
             Near(entity.position.z, -3.f, 1e-3f), "posición canónica");
        Need(Near(entity.half_extents.y, 0.35f, 1e-3f), "semialtura canónica");
        ++passed;

        mock->reads = 0;
        Need(reader.ReadLatest(frame) == dc::Status::Ok, "segundo tick falló");
        Need(mock->reads < cold_reads, "la caché de actores no redujo lecturas");
        ++passed;
    }

    // ── Proyección a pantalla ──
    {
        dc::CameraFrame camera;
        camera.position = {0.f, 1.f, 0.f};
        camera.target = {0.f, 1.f, -10.f};
        camera.up = {0.f, 1.f, 0.f};
        camera.vertical_fov_radians = 1.f;
        camera.near_plane = 0.1f;
        camera.far_plane = 200.f;

        const dc::Vec3 half{0.1575f, 0.35f, 0.1575f};
        const ScreenBox close_box =
            ProjectBox(camera, 0.f, {0.f, 1.35f, -5.f}, half, 1920.f, 1080.f);
        Need(close_box.visible, "caja delante no visible");
        Need(close_box.h > 10.f && close_box.w > 0.f, "caja sin tamaño");
        Need(std::fabs(close_box.x + close_box.w * 0.5f - 960.f) < 2.f, "caja descentrada");
        ++passed;

        const ScreenBox far_box =
            ProjectBox(camera, 0.f, {0.f, 1.35f, -20.f}, half, 1920.f, 1080.f);
        Need(far_box.visible && far_box.h < close_box.h, "la caja lejana no se encoge");
        ++passed;

        const ScreenBox behind =
            ProjectBox(camera, 0.f, {0.f, 1.35f, 5.f}, half, 1920.f, 1080.f);
        Need(!behind.visible, "caja detrás de la cámara visible");
        ++passed;
    }

    // ── Servicio con hilo (lo que consume el overlay) ──
    {
        auto owned = std::make_unique<MockDriver>();
        BuildScene(*owned, Vec3{300.f, 60.f, 0.f});

        EspService& service = EspService::Instance();
        Need(service.Attach(SceneOptions(), std::move(owned)) == DC_ESP_RESULT_OK, "Attach falló");

        std::shared_ptr<const EspFrame> frame;
        for (int attempt = 0; attempt < 200 && !frame; ++attempt) {
            frame = service.Latest();
            if (!frame) std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        Need(frame != nullptr, "el servicio no publicó ningún frame");
        Need(!frame->targets.empty(), "frame sin objetivos");

        DcEspBox boxes[8]{};
        const int count = service.Poll(1920.f, 1080.f, boxes, 8);
        Need(count >= 1, "Poll no devolvió cajas");
        Need(boxes[0].w > 0.f && boxes[0].h > 0.f, "caja degenerada");

        service.SetEnabled(false);
        Need(service.Poll(1920.f, 1080.f, boxes, 8) == 0, "Poll con el ESP apagado");
        service.SetEnabled(true);

        DcEspView view{};
        Need(service.CopyView(&view) == 1 && view.count == 1, "CopyView");
        ++passed;

        service.Detach();
        Need(!service.Attached(), "Detach no limpió el estado");
        ++passed;
    }

    return passed;
}

} // namespace dc::esp
