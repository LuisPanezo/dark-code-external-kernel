#pragma once
#include <stdint.h>
/*
 * Offsets confirmados para la build actual de Delta Force (libUE4.so).
 *
 * Regla: si un offset cambia con un parche del juego, se toca SOLO aquí.
 * Cada bloque indica la cadena que lo usa y qué se espera encontrar.
 */
namespace dc::esp::off {

/* ═══ UWorld → ULevel → TArray<AActor*> ═══ */
constexpr uintptr_t PERSISTENT_LEVEL = 0xF8;   // UWorld::PersistentLevel
constexpr uintptr_t ACTORS           = 0x98;   // ULevel::Actors { Data, Num, Max }

/* ═══ Actor / Character ═══ */
constexpr uintptr_t ROOT_COMPONENT  = 0x180;   // AActor::RootComponent
constexpr uintptr_t MESH             = 0x3D0;   // ACharacter::Mesh
constexpr uintptr_t CAPSULE          = 0x3E0;   // ACharacter::CapsuleComponent
constexpr uintptr_t CAPSULE_HALF_HEIGHT = 0x160; // UCapsuleComponent::CapsuleHalfHeight (cm, 86 de pie)
constexpr uintptr_t BASE_EYE_HEIGHT = 0x37C;   // ACharacter::BaseEyeHeight (float, cm)

/* ═══ Esqueleto: USkeletalMeshComponent ═══ */
constexpr uintptr_t COMPONENT_TRANSLATION = 0x148; // ComponentToWorld.Translation (FVector)
constexpr uintptr_t COMPONENT_ROTATION    = 0x1D0; // ComponentToWorld.Rotation (FQuat x,y,z,w)
constexpr uintptr_t BONE_TRANSFORMS     = 0xEA8; // ComponentSpaceTransforms ANIMADO (TArray<FTransform>)

/* Huesos principales del esqueleto de Delta Force (77 huesos), en orden:
 * pelvis→columna→cuello→cabeza, brazos y piernas. Índices REALES del juego,
 * no los del maniquí estándar de UE4. */
constexpr int kMainBoneIndex[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
                                  29, 30, 31, 32, 57, 59, 60, 61, 63, 64, 65};
constexpr int kMainBoneCount = 22;

/* ═══ Posición: FVector de 16 bytes (3 floats + trailer de cifrado) ═══ */
constexpr uintptr_t REL_LOCATION    = 0x168;   // USceneComponent::RelativeLocation

/* ═══ Cámara: APlayerCameraManager::CameraCache (FCameraCacheEntry) ═══
 * Verificado en partida (SM-S908U, Android 14): el POV vive en +0x1890, no en
 * +0x3F0. Hay tres copias idénticas de la caché en +0x1890, +0x2D70 y +0x37C0;
 * se usa la primera.
 * Layout de la entrada: Location(12 B) + padding(4) + Rotation(12 B) + FOV. */
constexpr uintptr_t CAM_CACHE       = 0x1880;  // FCameraCacheEntry
constexpr uintptr_t POV_LOCATION    = 0x1890;  // → FVector (12 B, 4 de padding)
constexpr uintptr_t POV_ROTATION    = 0x18A0;  // → FRotator (3 floats)
constexpr uintptr_t POV_FOV         = 0x18AC;  // → float (grados)

/* ═══ Vida: ADFMCharacter → HealthDataComponent → Set → float ═══ */
constexpr uintptr_t HEALTH_COMPONENT = 0x24E8;
constexpr uintptr_t HEALTH_SET       = 0x280;
constexpr uintptr_t HEALTH           = 0x3C;

/* ═══ Equipo, vía corta (AGPCharacterBase::TeamComp) ═══ */
constexpr uintptr_t TEAM_COMP        = 0x10C0;
constexpr uintptr_t TEAM_ID_FAST     = 0x108;
constexpr uintptr_t CAMP_ID          = 0x10C;

/* ═══ Equipo, vía respaldo (APawn::PlayerState) ═══ */
constexpr uintptr_t PLAYER_STATE     = 0x390;
constexpr uintptr_t TEAM_ID_FALLBACK = 0x658;
constexpr uintptr_t PLAYER_NAME      = 0x388;  // verificado: "2GustavFord4"
                                               // (hay copias en 0x470 y 0x480)

/* ═══ Cadena del jugador local ═══ */
constexpr uintptr_t GAME_INSTANCE     = 0x190; // UWorld::OwningGameInstance
constexpr uintptr_t LOCAL_PLAYERS     = 0x38;  // UGameInstance::LocalPlayers
constexpr uintptr_t PLAYER_CONTROLLER = 0x30;  // UPlayer::PlayerController
constexpr uintptr_t ACK_PAWN          = 0x3F0; // APlayerController::AcknowledgedPawn
constexpr uintptr_t CAMERA_MANAGER    = 0x408; // APlayerController::PlayerCameraManager

/* ═══ UObject (solo para diagnóstico por nombre) ═══ */
constexpr uintptr_t OBJ_CLASS    = 0x08;       // UObject::Class (esta build: 0x08)
constexpr uintptr_t GOBJECTS_RVA = 0x205ED838; // FUObjectArray*
constexpr uintptr_t GNAMES_RVA   = 0x205C6A00; // FNamePool*

/* ═══ Sanidad ═══ */
constexpr int   MAX_ACTORS          = 15000;
constexpr float MAX_WORLD_COORD_CM  = 400000.f; // 4 km: más allá es basura
constexpr size_t MAX_NAME_CHARS     = 60;
constexpr uintptr_t MIN_VALID_PTR   = 0x1000000000ULL;

} // namespace dc::esp::off
