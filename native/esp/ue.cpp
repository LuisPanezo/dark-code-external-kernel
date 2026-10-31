#include "ue.h"

#include <cmath>
#include <cstring>

namespace dc::esp {
namespace {

constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.f;

bool FiniteF(float value) { return std::isfinite(value); }

bool VectorPlausible(const Vec3& value, const Vec3* reference, const DecodeLimits& limits) {
    if (!Finite(value)) return false;
    // Entidades "invisibles": su RootComponent lee la posición default (0,0,0)
    // o casi (ej. 0.3,-0.1,0). Un jugador real siempre está a metros del origen.
    const float magnitude_sq = value.x * value.x + value.y * value.y + value.z * value.z;
    if (magnitude_sq < 100.f * 100.f) return false;   // < 1 m del origen del mapa
    if (std::fabs(value.x) > limits.absolute_cm ||
        std::fabs(value.y) > limits.absolute_cm ||
        std::fabs(value.z) > limits.absolute_cm) return false;
    if (reference) {
        float dx = value.x - reference->x;
        float dy = value.y - reference->y;
        float dz = value.z - reference->z;
        if (dx * dx + dy * dy + dz * dz > limits.reference_cm * limits.reference_cm) return false;
    }
    return true;
}

/* Hipótesis documentada: `index` guarda los 16 bits bajos que se quitaron de X. */
Vec3 RestoreTrailer(const FEncVector& raw) {
    Vec3 value{raw.x, raw.y, raw.z};
    uint32_t bits = 0;
    std::memcpy(&bits, &raw.x, sizeof(bits));
    bits = (bits & 0xFFFF0000u) | static_cast<uint32_t>(raw.index);
    std::memcpy(&value.x, &bits, sizeof(bits));
    return value;
}

} // namespace

bool PlausiblePointer(uintptr_t pointer) {
    return pointer >= off::MIN_VALID_PTR && pointer < 0x0000800000000000ULL && (pointer & 7) == 0;
}

void BasisFromRotator(const Rotator& rotation, Vec3& forward, Vec3& right, Vec3& up) {
    float pitch = rotation.pitch * kDegreesToRadians;
    float yaw = rotation.yaw * kDegreesToRadians;
    float roll = rotation.roll * kDegreesToRadians;
    float cp = std::cos(pitch), sp = std::sin(pitch);
    float cy = std::cos(yaw), sy = std::sin(yaw);
    float cr = std::cos(roll), sr = std::sin(roll);

    forward = {cp * cy, cp * sy, sp};
    right = {cy * sp * sr - sy * cr, sy * sp * sr + cy * cr, -cp * sr};
    up = {cy * cr * sp + sy * sr, sy * cr * sp - cy * sr, cr * cp};
}

float VerticalFovFromHorizontal(float horizontal_radians, float aspect) {
    if (!(aspect > 0.f) || !FiniteF(aspect) || !(horizontal_radians > 0.f) ||
        horizontal_radians >= 3.14159265f) return 0.f;
    return 2.f * std::atan(std::tan(horizontal_radians * 0.5f) / aspect);
}

float HorizontalFovFromVertical(float vertical_radians, float aspect) {
    if (!(aspect > 0.f) || !FiniteF(aspect) || !(vertical_radians > 0.f) ||
        vertical_radians >= 3.14159265f) return 0.f;
    return 2.f * std::atan(std::tan(vertical_radians * 0.5f) * aspect);
}

bool ReadPlayerChain(IMemoryDriver& driver, uintptr_t world, PlayerChain& chain) {
    chain = PlayerChain{};
    if (!PlausiblePointer(world)) return false;
    chain.game_instance = ReadPtr(driver, world + off::GAME_INSTANCE);
    if (!PlausiblePointer(chain.game_instance)) return false;
    uintptr_t local_players = ReadPtr(driver, chain.game_instance + off::LOCAL_PLAYERS);
    if (!PlausiblePointer(local_players)) return false;
    chain.local_player = ReadPtr(driver, local_players);
    if (!PlausiblePointer(chain.local_player)) return false;
    chain.controller = ReadPtr(driver, chain.local_player + off::PLAYER_CONTROLLER);
    if (!PlausiblePointer(chain.controller)) return false;
    chain.pawn = ReadPtr(driver, chain.controller + off::ACK_PAWN);
    chain.camera_manager = ReadPtr(driver, chain.controller + off::CAMERA_MANAGER);
    chain.valid = true;
    return true;
}

bool ReadCamera(IMemoryDriver& driver, const PlayerChain& chain,
                float fov_override_degrees, bool fov_memory_is_horizontal,
                CameraState& camera) {
    camera = CameraState{};
    if (!PlausiblePointer(chain.camera_manager)) return false;

    Vec3 location{};
    Rotator rotation{};
    float fov = 0.f;
    if (!Read(driver, chain.camera_manager + off::POV_LOCATION, location)) return false;
    if (!Read(driver, chain.camera_manager + off::POV_ROTATION, rotation)) return false;
    if (!Read(driver, chain.camera_manager + off::POV_FOV, fov)) return false;

    if (fov_override_degrees > 0.f) fov = fov_override_degrees;
    if (!Finite(location) || !FiniteF(fov) || !FiniteF(rotation.pitch) ||
        !FiniteF(rotation.yaw) || !FiniteF(rotation.roll)) return false;
    if (fov < 20.f || fov > 170.f) return false;
    if (location.x == 0.f && location.y == 0.f && location.z == 0.f) return false;

    Vec3 forward{}, right{}, up{};
    BasisFromRotator(rotation, forward, right, up);
    if (!Finite(forward) || !Finite(up) || Length(forward) < 0.5f) return false;

    camera.location = location;
    camera.rotation = rotation;
    camera.fov_degrees = fov;
    camera.fov_horizontal = fov_memory_is_horizontal;
    camera.valid = true;
    return true;
}

DecodeResult DecodeVector(const FEncVector& raw, const Vec3* reference_cm,
                          const DecodeLimits& limits, Codec codec) {
    DecodeResult result;
    result.flag = raw.encrypted != 0;
    Vec3 plain{raw.x, raw.y, raw.z};

    // El trailer solo tiene sentido si venía marcado.
    if (codec == Codec::Trailer && !result.flag) codec = Codec::Plain;

    switch (codec) {
    case Codec::Plain:
        result.value = plain;
        result.path = Codec::Plain;
        break;
    case Codec::Trailer:
        result.value = RestoreTrailer(raw);
        result.path = Codec::Trailer;
        break;
    case Codec::Auto:
    default:
        if (VectorPlausible(plain, reference_cm, limits)) {
            result.value = plain;
            result.path = Codec::Plain;
        } else {
            result.value = RestoreTrailer(raw);
            result.path = Codec::Trailer;
        }
        break;
    }
    result.plausible = VectorPlausible(result.value, reference_cm, limits);
    return result;
}

bool ReadPlayerName(IMemoryDriver& driver, uintptr_t player_state, std::string& out) {
    out.clear();
    if (!PlausiblePointer(player_state)) return false;

    struct FString { uintptr_t data; int32_t num; int32_t max; };
    FString text{};
    if (!Read(driver, player_state + off::PLAYER_NAME, text)) return false;
    if (!PlausiblePointer(text.data)) return false;
    if (text.num <= 0 || text.num > static_cast<int32_t>(off::MAX_NAME_CHARS)) return false;

    char16_t characters[off::MAX_NAME_CHARS + 1]{};
    size_t want = static_cast<size_t>(text.num) * sizeof(char16_t);
    if (driver.read(text.data, characters, want) != want) return false;

    out.reserve(static_cast<size_t>(text.num));
    for (int32_t i = 0; i < text.num; ++i) {
        char16_t value = characters[i];
        if (value == 0) break;
        out.push_back(value < 128 ? static_cast<char>(value) : '?');
    }
    return !out.empty();
}

bool ResolveHealthSet(IMemoryDriver& driver, uintptr_t character, uintptr_t& out) {
    out = 0;
    if (!PlausiblePointer(character)) return false;
    uintptr_t component = ReadPtr(driver, character + off::HEALTH_COMPONENT);
    if (!PlausiblePointer(component)) return false;
    uintptr_t set = ReadPtr(driver, component + off::HEALTH_SET);
    if (!PlausiblePointer(set)) return false;
    out = set;
    return true;
}

bool ReadHealthValue(IMemoryDriver& driver, uintptr_t health_set, float& out) {
    out = -1.f;
    if (!PlausiblePointer(health_set)) return false;
    float value = 0.f;
    if (!Read(driver, health_set + off::HEALTH, value)) return false;
    if (!(value >= 0.f) || !(value <= 100000.f)) return false;
    out = value;
    return true;
}

bool ResolveTeamComponent(IMemoryDriver& driver, uintptr_t character, uintptr_t& out) {
    out = 0;
    if (!PlausiblePointer(character)) return false;
    uintptr_t component = ReadPtr(driver, character + off::TEAM_COMP);
    if (!PlausiblePointer(component)) return false;
    out = component;
    return true;
}

bool ReadTeamValue(IMemoryDriver& driver, uintptr_t team_component, int32_t& out) {
    out = -1;
    if (!PlausiblePointer(team_component)) return false;
    int32_t value = -1;
    if (!Read(driver, team_component + off::TEAM_ID_FAST, value)) return false;
    if (value < 0 || value > 32) return false;
    out = value;
    return true;
}

/* Equipo/campo real (más amplio que la escuadra): TeamComp + CAMP_ID. */
bool ReadCampValue(IMemoryDriver& driver, uintptr_t team_component, int32_t& out) {
    out = -1;
    if (!PlausiblePointer(team_component)) return false;
    int32_t value = -1;
    if (!Read(driver, team_component + off::CAMP_ID, value)) return false;
    if (value < 0 || value > 32) return false;
    out = value;
    return true;
}

bool ReadTeamFallback(IMemoryDriver& driver, uintptr_t player_state, int32_t& out) {
    out = -1;
    if (!PlausiblePointer(player_state)) return false;
    int8_t value = -1;
    if (!Read(driver, player_state + off::TEAM_ID_FALLBACK, value)) return false;
    if (value < 0 || value > 32) return false;
    out = value;
    return true;
}

float ReadEyeHeight(IMemoryDriver& driver, uintptr_t character, float fallback_cm) {
    if (!PlausiblePointer(character)) return fallback_cm;
    float value = 0.f;
    if (!Read(driver, character + off::BASE_EYE_HEIGHT, value)) return fallback_cm;
    if (!(value >= 30.f) || !(value <= 300.f)) return fallback_cm;
    return value;
}

/* Aplica un cuaternión unitario a un vector: v' = q * v * q^-1. */
static Vec3 RotateByQuat(const Vec3& v, float qx, float qy, float qz, float qw) {
    const float tx = 2.f * (qy * v.z - qz * v.y);
    const float ty = 2.f * (qz * v.x - qx * v.z);
    const float tz = 2.f * (qx * v.y - qy * v.x);
    return Vec3{
        v.x + qw * tx + (qy * tz - qz * ty),
        v.y + qw * ty + (qz * tx - qx * tz),
        v.z + qw * tz + (qx * ty - qy * tx),
    };
}

/* Lee los huesos principales (kMainBoneIndex) en mundo, en cm. */
int ReadBones(IMemoryDriver& driver, uintptr_t mesh, Vec3* out) {
    if (!PlausiblePointer(mesh)) return 0;

    // ComponentToWorld: Rotation(4) + Translation(3) + Scale(3) = 10 floats.
    float ctw[10] = {0};
    if (!Read(driver, mesh + off::COMPONENT_TO_WORLD, ctw)) return 0;
    const float qx = ctw[0], qy = ctw[1], qz = ctw[2], qw = ctw[3];
    const float tx = ctw[4], ty = ctw[5], tz = ctw[6];

    // TArray<FTransform> en mesh+0x760: data (ptr), num (i32), max (i32).
    const uintptr_t data = ReadPtr(driver, mesh + off::BONE_TRANSFORMS);
    int32_t num = 0;
    if (!Read(driver, mesh + off::BONE_TRANSFORMS + 8, num)) return 0;
    if (!PlausiblePointer(data) || num <= 0 || num > 400) return 0;

    // Leer todo el array de una vez (stride 48, trans en +16 = floats[4..6]).
    std::vector<unsigned char> buf(static_cast<size_t>(num) * 48);
    if (driver.read(data, buf.data(), buf.size()) != buf.size()) return 0;

    int count = 0;
    for (int i = 0; i < off::kMainBoneCount; ++i) {
        const int bone = off::kMainBoneIndex[i];
        if (bone >= num) continue;
        const float* f = reinterpret_cast<const float*>(
            buf.data() + static_cast<size_t>(bone) * 48);
        const Vec3 local{f[4], f[5], f[6]};
        const Vec3 world = RotateByQuat(local, qx, qy, qz, qw);
        out[count] = Vec3{world.x + tx, world.y + ty, world.z + tz};
        ++count;
    }
    return count;
}

} // namespace dc::esp
