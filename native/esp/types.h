#pragma once
#include <sys/types.h>
#include <stddef.h>
#include <cstdint>
#include <string>

#include "core/frame_snapshot.h"   // dc::kMaxEntities, dc::Vec3

namespace dc::esp {

using Vec3 = dc::Vec3;

/* Rotación de Unreal en grados: pitch (Y), yaw (Z), roll (X). */
struct Rotator { float pitch{}, yaw{}, roll{}; };

/* POV leído de APlayerCameraManager::CameraCache.
 * `fov_horizontal` dice cómo interpretar fov_degrees; el proyector convierte
 * al eje que necesite con la relación de aspecto real de la superficie. */
struct CameraState {
    Vec3    location{};
    Rotator rotation{};
    float   fov_degrees{90.f};
    bool    fov_horizontal{true};
    bool    valid{};
};

/*
 * FVector de esta build: X, Y, Z + trailer de cifrado (16 bytes).
 * Cuando `encrypted` != 0 el valor se reporta igual, marcado, para poder
 * diagnosticar; ver DecodeVector() en ue.h.
 */
struct FEncVector {
    float    x{}, y{}, z{};
    uint16_t index{};
    int8_t   encrypted{};
    uint8_t  pad{};
};
static_assert(sizeof(FEncVector) == 16, "FEncVector debe pesar 16 bytes");
static_assert(offsetof(FEncVector, index) == 12, "trailer: index en +0x0C");
static_assert(offsetof(FEncVector, encrypted) == 14, "trailer: bEncrypted en +0x0E");

struct AddressRange {
    uintptr_t start{}, end{};
    size_t size() const { return end > start ? static_cast<size_t>(end - start) : 0; }
};

enum class Codec { Auto, Plain, Trailer };
enum class State { Detached, Starting, NoPid, NoDriver, NoWorld, NoCamera, Live, Error };

const char* CodecName(Codec codec);
const char* StateName(State state);

struct Options {
    std::string package{"com.garena.game.df"};
    std::string module{"libUE4.so"};
    pid_t       pid{-1};                 // si < 0 se busca por `package`
    uintptr_t   gworld{0};               // 0 = escanear
    float       hfov_degrees{0.f};       // 0 = leer de memoria
    bool        fov_horizontal{true};    // UE entrega FOV horizontal por defecto
    float       max_distance_m{400.f};
    bool        skip_teammates{true};
    bool        require_character{true};  // exige equipo o vida legibles
    float       min_health{0.f};          // 0 = sin filtro; 1 = descarta HP<=0
    int         cache_ttl_ticks{30};
    int         poll_interval_ms{16};
    float       eye_extra_cm{10.f};
    float       default_eye_cm{165.f};
    float       box_width_ratio{0.35f};
    float       aspect{16.f / 9.f};      // relación usada para el vfov del snapshot
    Codec       codec{Codec::Auto};
    bool        verbose{false};
    bool        rescan{true};
};

/* Un actor aceptado, en centímetros de Unreal (eje Z arriba). */
struct Target {
    uintptr_t   actor{};
    Vec3        feet_cm{};
    float       height_cm{170.f};
    float       width_cm{40.f};
    float       distance_m{};
    float       health{-1.f};
    int32_t     team{-1};
    bool        encrypted{};
    bool        teammate{};
    std::string name;
    Vec3        bones[32]{};   // huesos principales (mundo, cm), kMainBoneIndex
    int         bone_count{0}; // cuántos huesos válidos
};

struct Stats {
    uint64_t ticks{};
    uint64_t read_calls{};
    int actors_in_level{};
    int actors_considered{};
    int accepted{};
    int rejected_pointer{};
    int rejected_position{};
    int rejected_distance{};
    int teammates{};
    int encrypted{};
    int plain{};
    int unreadable{};
    int duplicates{};
    int last_errno{};
};

} // namespace dc::esp
