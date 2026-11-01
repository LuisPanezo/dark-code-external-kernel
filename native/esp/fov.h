#pragma once
#include <cmath>

namespace dc::esp {

/* Conversión FOV horizontal <-> vertical. Genérica: no depende de Unreal. */
inline float VerticalFovFromHorizontal(float horizontal_radians, float aspect) {
    if (!(aspect > 0.f) || !std::isfinite(aspect) || !(horizontal_radians > 0.f) ||
        horizontal_radians >= 3.14159265f) return 0.f;
    return 2.f * std::atan(std::tan(horizontal_radians * 0.5f) / aspect);
}

inline float HorizontalFovFromVertical(float vertical_radians, float aspect) {
    if (!(aspect > 0.f) || !std::isfinite(aspect) || !(vertical_radians > 0.f) ||
        vertical_radians >= 3.14159265f) return 0.f;
    return 2.f * std::atan(std::tan(vertical_radians * 0.5f) * aspect);
}

} // namespace dc::esp
