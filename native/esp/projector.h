#pragma once
#include <cstddef>

#include "core/frame_snapshot.h"
#include "types.h"

namespace dc::esp {

struct RectOptions {
    float width_ratio{0.45f};   // respaldo de ancho/alto si el AABB degenera
    float margin_px{64.f};      // margen para seguir dibujando al salir de pantalla
    float min_height_px{4.f};   // más pequeño que esto no se dibuja
};

/* Rectángulo listo para dibujar (píxeles, origen arriba-izquierda, como ImGui). */
struct ScreenBox {
    float x{}, y{}, w{}, h{};
    bool  visible{};
};

/*
 * Proyecta un AABB canónico con la MISMA matriz que usa el motor de escena
 * (projection/projection.cpp), así el overlay y la escena 3D nunca discrepan.
 * `horizontal_fov_radians` manda sobre el FOV vertical del snapshot porque es
 * el que reporta el juego; si es <= 0 se usa el del snapshot tal cual.
 */
ScreenBox ProjectBox(const dc::CameraFrame& camera, float horizontal_fov_radians,
                     dc::Vec3 center, dc::Vec3 half_extents,
                     float screen_w, float screen_h, const RectOptions& options = {});

ScreenBox ProjectEntity(const dc::FrameSnapshot& frame, float horizontal_fov_radians,
                        size_t index, float screen_w, float screen_h,
                        const RectOptions& options = {});

/* El alto en pantalla es la referencia: el ancho sale del AABB o del respaldo. */
ScreenBox ProjectFeetHead(const dc::CameraFrame& camera, float horizontal_fov_radians,
                          dc::Vec3 feet, dc::Vec3 head, float world_ratio,
                          float screen_w, float screen_h, const RectOptions& options = {});

} // namespace dc::esp
