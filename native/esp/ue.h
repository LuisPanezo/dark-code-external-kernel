#pragma once
#include <string>

#include "driver.h"
#include "offsets.h"
#include "types.h"

namespace dc::esp {

/* Puntero alineado y dentro del espacio de usuario de 47 bits. */
bool PlausiblePointer(uintptr_t pointer);

/*
 * Base ortonormal de Unreal (mano izquierda: X delante, Y derecha, Z arriba)
 * a partir de un rotador en grados. Es la misma convención que usa
 * APlayerCameraManager, incluido el roll.
 */
void BasisFromRotator(const Rotator& rotation, Vec3& forward, Vec3& right, Vec3& up);

/* UE entrega FOV horizontal en pantallas anchas; estas dos lo convierten. */
float VerticalFovFromHorizontal(float horizontal_radians, float aspect);
float HorizontalFovFromVertical(float vertical_radians, float aspect);

struct PlayerChain {
    uintptr_t game_instance{};
    uintptr_t local_player{};
    uintptr_t controller{};
    uintptr_t pawn{};
    uintptr_t camera_manager{};
    bool      valid{};
};

/* UWorld → GameInstance → LocalPlayers[0] → PlayerController → Pawn/Cámara. */
bool ReadPlayerChain(IMemoryDriver& driver, uintptr_t world, PlayerChain& chain);

bool ReadCamera(IMemoryDriver& driver, const PlayerChain& chain,
                float fov_override_degrees, bool fov_memory_is_horizontal,
                CameraState& camera);

struct DecodeLimits {
    float absolute_cm{off::MAX_WORLD_COORD_CM};
    // Antes 60000 (600 m): con la cámara en carga/espectador o mapas grandes
    // descartaba actores válidos como "ilegibles". El filtro real de distancia
    // lo hace max_distance_m; aquí solo se rechaza basura fuera del mundo.
    float reference_cm{off::MAX_WORLD_COORD_CM};
};

struct DecodeResult {
    Vec3  value{};
    bool  flag{};        // el trailer venía marcado como cifrado
    bool  plausible{};
    Codec path{Codec::Plain};
};

/*
 * El FVector de esta build lleva 3 floats + trailer (index, bEncrypted).
 * `Auto` acepta el valor tal cual si es creíble (el caso habitual incluso
 * con el flag puesto) y, si no, prueba a restaurar los 16 bits bajos de X
 * desde `index`. Si ninguno es creíble, plausible = false y el actor se
 * descarta; el diagnóstico imprime los bits crudos para reversear DecVector.
 */
DecodeResult DecodeVector(const FEncVector& raw, const Vec3* reference_cm,
                          const DecodeLimits& limits, Codec codec);

/* APlayerState::PlayerName (FString UTF-16) → ASCII, '?' fuera de rango. */
bool ReadPlayerName(IMemoryDriver& driver, uintptr_t player_state, std::string& out);

/* Vida: ACharacter → HealthDataComponent → Set → float. */
bool ResolveHealthSet(IMemoryDriver& driver, uintptr_t character, uintptr_t& out);
bool ReadHealthValue(IMemoryDriver& driver, uintptr_t health_set, float& out);

/* Equipo: vía corta (TeamComp) y respaldo (PlayerState). */
bool ResolveTeamComponent(IMemoryDriver& driver, uintptr_t character, uintptr_t& out);
bool ReadTeamValue(IMemoryDriver& driver, uintptr_t team_component, int32_t& out);
bool ReadCampValue(IMemoryDriver& driver, uintptr_t team_component, int32_t& out);
bool ReadTeamFallback(IMemoryDriver& driver, uintptr_t player_state, int32_t& out);

/* ACharacter::BaseEyeHeight, con cordura; si no sirve, devuelve el respaldo. */
float ReadEyeHeight(IMemoryDriver& driver, uintptr_t character, float fallback_cm);

/* Lee los huesos principales del SkeletalMesh y los devuelve en mundo (cm).
 * `out` admite kMainBoneCount elementos; devuelve cuántos se rellenaron. */
int ReadBones(IMemoryDriver& driver, uintptr_t mesh, Vec3* out);

} // namespace dc::esp
