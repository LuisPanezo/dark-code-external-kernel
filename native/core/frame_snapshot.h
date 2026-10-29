#pragma once
#include "types.h"
#include <cstdint>
#include <string>
#include <vector>
namespace dc {
constexpr std::size_t kMaxEntities=512;
struct CameraFrame {
    Vec3 position{0,2.8f,7}, target{0,0.3f,0}, up{0,1,0};
    float vertical_fov_radians{0.959931f}, near_plane{0.1f}, far_plane{100};
};
struct Entity {
    uint64_t id{};
    Vec3 position{}, half_extents{0.5f,0.5f,0.5f};
    Color color{};
    std::string label;
};
struct FrameSnapshot {
    uint64_t sequence{}, timestamp_ns{};
    CameraFrame camera;
    std::vector<Entity> entities;
};
uint64_t MonotonicNs();
bool Validate(const FrameSnapshot& frame);
bool IsFresh(const FrameSnapshot& frame,uint64_t now,uint64_t maximum_age=250000000);
}
