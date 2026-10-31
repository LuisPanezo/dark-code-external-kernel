#include "mock_source.h"
#include "adapters/normalize.h"
namespace dc {
FrameSnapshot SampleScene(float seconds,Coordinates coordinates) {
    TelemetryFrame input;
    input.entities={
        {1,{-1.7f,.5f,std::sin(seconds)*.7f},{.45f,.5f,.45f},{.85f,.77f,.58f,1},"Ámbar"},
        {2,{0,.65f+std::sin(seconds*1.5f)*.18f,-.5f},{.5f,.65f,.5f},{.4f,.8f,.72f,1},"Jade"},
        {3,{1.7f,.4f,std::cos(seconds)*.8f},{.4f,.4f,.4f},{.62f,.66f,.98f,1},"Índigo"}};
    input.camera.position=FromCanonical(input.camera.position,coordinates);
    input.camera.target=FromCanonical(input.camera.target,coordinates);
    input.camera.up=Unit(FromCanonical(input.camera.up,coordinates));
    if(coordinates==Coordinates::UnrealWorldCentimeters) {input.camera.near_plane*=100;input.camera.far_plane*=100;}
    for(auto& e:input.entities) {
        e.position=FromCanonical(e.position,coordinates);
        Vec3 s=FromCanonical(e.half_extents,coordinates);
        e.half_extents={std::abs(s.x),std::abs(s.y),std::abs(s.z)};
    }
    input.timestamp_ns=MonotonicNs();FrameSnapshot out;
    Normalize(input,coordinates,out);
    return out;
}
Status MockSource::Start() {start_=MonotonicNs();sequence_=0;running_=true;return Status::Ok;}
Status MockSource::ReadLatest(FrameSnapshot& out) {
    if(!running_) return Status::Stopped;
    out=SampleScene(float(MonotonicNs()-start_)/1e9f,coordinates_);out.sequence=++sequence_;
    return Status::Ok;
}
}
