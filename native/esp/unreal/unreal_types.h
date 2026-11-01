#pragma once
#include <cstdint>
#include <string>

#include "core/frame_snapshot.h"   // dc::Vec3
#include "target.h"                 // EspTarget (base de Target)

namespace dc::esp {

using Vec3 = dc::Vec3;

/* Rotación de Unreal en grados: pitch (Y), yaw (Z), roll (X). */
struct Rotator { float pitch{}, yaw{}, roll{}; };

/* POV leído de APlayerCameraManager::CameraCache.
 * `fov_horizontal` dice cómo interpretar fov_degrees. */
struct CameraState {
    Vec3    location{};
    Rotator rotation{};
    float   fov_degrees{90.f};
    bool    fov_horizontal{true};
    bool    valid{};
};

/* FVector de esta build: X,Y,Z + trailer de cifrado (16 bytes). */
struct FEncVector {
    float    x{}, y{}, z{};
    uint16_t index{};
    int8_t   encrypted{};
    uint8_t  pad{};
};
static_assert(sizeof(FEncVector) == 16, "FEncVector debe pesar 16 bytes");
static_assert(offsetof(FEncVector, index) == 12, "trailer: index en +0x0C");
static_assert(offsetof(FEncVector, encrypted) == 14, "trailer: bEncrypted en +0x0E");

enum class Codec { Auto, Plain, Trailer };

inline const char* CodecName(Codec codec) {
    switch (codec) {
    case Codec::Plain:   return "plain";
    case Codec::Trailer: return "trailer";
    case Codec::Auto:    break;
    }
    return "auto";
}

/* Configuración específica de Unreal / Delta Force. */
struct Options {
    std::string package{"com.garena.game.df"};
    std::string module{"libUE4.so"};
    pid_t       pid{-1};
    uintptr_t   gworld{0};
    float       hfov_degrees{0.f};
    bool        fov_horizontal{true};
    float       max_distance_m{400.f};
    bool        skip_teammates{true};
    bool        require_character{true};
    float       min_health{0.f};
    int         cache_ttl_ticks{30};
    int         poll_interval_ms{16};
    float       eye_extra_cm{10.f};
    float       default_eye_cm{165.f};
    float       box_width_ratio{0.35f};
    float       aspect{16.f / 9.f};
    Codec       codec{Codec::Auto};
    bool        verbose{false};
    bool        rescan{true};
};

/* Un actor aceptado por el adaptador Unreal: geometría en centímetros (para el
 * tool y para construir el FrameSnapshot canónico). La metadata canónica la
 * hereda de EspTarget. */
struct Target : EspTarget {
    Vec3  feet_cm{};
    float height_cm{170.f};
    float width_cm{40.f};
    Vec3  bones[32]{};   // huesos principales (mundo, cm)
    int   bone_count{0};
};

} // namespace dc::esp
