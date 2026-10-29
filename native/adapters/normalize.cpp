#include "normalize.h"
namespace dc {
Vec3 ToCanonical(Vec3 p,Coordinates c) {
    if(c==Coordinates::UnityWorldMeters) return {p.x,p.y,-p.z};
    if(c==Coordinates::UnrealWorldCentimeters) return {p.y*.01f,p.z*.01f,-p.x*.01f};
    return p;
}
Vec3 FromCanonical(Vec3 p,Coordinates c) {
    if(c==Coordinates::UnityWorldMeters) return {p.x,p.y,-p.z};
    if(c==Coordinates::UnrealWorldCentimeters) return {-p.z*100,p.x*100,p.y*100};
    return p;
}
Status Normalize(const TelemetryFrame& in,Coordinates coord,FrameSnapshot& out) {
    if(in.entities.size()>kMaxEntities) return Status::InvalidFrame;
    FrameSnapshot result;
    result.sequence=in.sequence;result.timestamp_ns=in.timestamp_ns;result.camera=in.camera;
    auto& c=result.camera;
    c.position=ToCanonical(c.position,coord);c.target=ToCanonical(c.target,coord);
    c.up=Unit(ToCanonical(c.up,coord));
    if(coord==Coordinates::UnrealWorldCentimeters) { c.near_plane*=.01f;c.far_plane*=.01f; }
    result.entities=in.entities;
    for(auto& e:result.entities) {
        e.position=ToCanonical(e.position,coord);
        Vec3 size=ToCanonical(e.half_extents,coord);
        // Input extents are positive lengths along each input world axis.
        if(e.half_extents.x<=0||e.half_extents.y<=0||e.half_extents.z<=0) return Status::InvalidFrame;
        e.half_extents={std::abs(size.x),std::abs(size.y),std::abs(size.z)};
    }
    if(!Validate(result)) return Status::InvalidFrame;
    out=std::move(result);return Status::Ok;
}
}
