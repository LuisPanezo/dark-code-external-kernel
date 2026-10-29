#include "frame_snapshot.h"
#include <chrono>
#include <unordered_set>
namespace dc {
uint64_t MonotonicNs() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
bool IsFresh(const FrameSnapshot& f,uint64_t now,uint64_t age) {
    return f.timestamp_ns>0 && f.timestamp_ns<=now && now-f.timestamp_ns<=age;
}
bool Validate(const FrameSnapshot& f) {
    const auto& c=f.camera;
    if(!Finite(c.position)||!Finite(c.target)||!Finite(c.up)||
       Length(c.target-c.position)<1e-4f||Length(Cross(c.target-c.position,c.up))<1e-4f||
       !std::isfinite(c.vertical_fov_radians)||c.vertical_fov_radians<0.05f||c.vertical_fov_radians>3.0f||
       !std::isfinite(c.near_plane)||!std::isfinite(c.far_plane)||c.near_plane<=0||c.far_plane<=c.near_plane||
       f.entities.size()>kMaxEntities) return false;
    std::unordered_set<uint64_t> ids;
    for(const auto& e:f.entities) {
        if(!e.id||!ids.insert(e.id).second||!Finite(e.position)||!Finite(e.half_extents)||
            e.half_extents.x<=0||e.half_extents.y<=0||e.half_extents.z<=0||e.label.size()>128) return false;
        for(float value:{e.color.r,e.color.g,e.color.b,e.color.a})
            if(!std::isfinite(value)||value<0||value>1) return false;
    } return true;
}
}
