#include "projector.h"

#include <algorithm>
#include <cmath>

#include "projection/projection.h"
#include "fov.h"

namespace dc::esp {
namespace {

dc::CameraFrame WithAspect(const dc::CameraFrame& camera, float horizontal_fov_radians,
                            float aspect) {
    dc::CameraFrame copy = camera;
    if (horizontal_fov_radians > 0.f) {
        const float vertical = VerticalFovFromHorizontal(horizontal_fov_radians, aspect);
        if (vertical > 0.02f && vertical < 3.05f) copy.vertical_fov_radians = vertical;
    }
    return copy;
}

bool Usable(float width, float height) {
    return std::isfinite(width) && std::isfinite(height) && width > 1.f && height > 1.f;
}

} // namespace

ScreenBox ProjectFeetHead(const dc::CameraFrame& camera, float horizontal_fov_radians,
                          dc::Vec3 feet, dc::Vec3 head, float world_ratio,
                          float screen_w, float screen_h, const RectOptions& options) {
    ScreenBox box;
    if (!Usable(screen_w, screen_h) || !dc::Finite(feet) || !dc::Finite(head)) return box;

    const float aspect = screen_w / screen_h;
    const dc::Mat4 matrix = dc::ViewProjection(WithAspect(camera, horizontal_fov_radians, aspect),
                                                aspect);
    dc::Vec3 feet_ndc{}, head_ndc{};
    // ProjectSegment recorta contra el plano cercano: un enemigo pegado a la
    // cámara (pies detrás del near plane) ya no se descarta entero. ProjectPoint
    // lo tiraba y por eso "cerca no se marcaba".
    if (!dc::ProjectSegment(matrix, feet, head, feet_ndc, head_ndc)) return box;

    const float feet_y = (0.5f - feet_ndc.y * 0.5f) * screen_h;
    const float head_y = (0.5f - head_ndc.y * 0.5f) * screen_h;
    const float center_x = (0.5f + (feet_ndc.x + head_ndc.x) * 0.25f) * screen_w;

    box.h = feet_y - head_y;
    box.y = head_y;
    if (!std::isfinite(box.h) || box.h <= options.min_height_px) return box;

    box.w = box.h * world_ratio;
    if (!(box.w > 0.5f) || !std::isfinite(box.w)) box.w = box.h * options.width_ratio;
    box.x = center_x - box.w * 0.5f;
    if (box.h > screen_h * 8.f) box.h = screen_h * 8.f;   // evita rects absurdos de cerca

    const float margin = options.margin_px;
    box.visible = box.x + box.w > -margin && box.x < screen_w + margin &&
                  box.y + box.h > -margin && box.y < screen_h + margin;
    return box;
}

ScreenBox ProjectBox(const dc::CameraFrame& camera, float horizontal_fov_radians,
                     dc::Vec3 center, dc::Vec3 half_extents,
                     float screen_w, float screen_h, const RectOptions& options) {
    if (!dc::Finite(center) || !dc::Finite(half_extents)) return ScreenBox{};
    if (!(half_extents.y > 0.f)) return ScreenBox{};
    const dc::Vec3 feet{center.x, center.y - half_extents.y, center.z};
    const dc::Vec3 head{center.x, center.y + half_extents.y, center.z};
    const float world_ratio = half_extents.y > 0.f
        ? std::max(half_extents.x, half_extents.z) / half_extents.y
        : options.width_ratio;
    ScreenBox box = ProjectFeetHead(camera, horizontal_fov_radians, feet, head, world_ratio,
                                    screen_w, screen_h, options);
    if (!box.visible) {
        // Enemigo pegado a la cámara: pies/cabeza caen dentro del near plane y el
        // segmento se descarta. Respaldo: proyecta el CENTRO (sigue en frente) y
        // dibuja una caja de tamaño mínimo para no perder el objetivo.
        const float aspect = screen_w / screen_h;
        const dc::Mat4 matrix = dc::ViewProjection(
            WithAspect(camera, horizontal_fov_radians, aspect), aspect);
        dc::Vec3 ndc{};
        if (dc::ProjectPoint(matrix, center, ndc)) {
            const float cx = (0.5f + ndc.x * 0.5f) * screen_w;
            const float cy = (0.5f - ndc.y * 0.5f) * screen_h;
            box.h = std::max(screen_h * 0.18f, options.min_height_px);
            box.w = box.h * world_ratio;
            box.x = cx - box.w * 0.5f;
            box.y = cy - box.h * 0.5f;
            box.visible = true;
        }
    }
    return box;
}

ScreenBox ProjectEntity(const dc::FrameSnapshot& frame, float horizontal_fov_radians,
                        size_t index, float screen_w, float screen_h,
                        const RectOptions& options) {
    if (index >= frame.entities.size()) return ScreenBox{};
    const dc::Entity& entity = frame.entities[index];
    return ProjectBox(frame.camera, horizontal_fov_radians, entity.position,
                      entity.half_extents, screen_w, screen_h, options);
}

} // namespace dc::esp
