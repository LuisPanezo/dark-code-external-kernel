#include "scene_geometry.h"
#include "projection/projection.h"
namespace dc {
std::vector<Vertex> BuildScene(const FrameSnapshot& frame,float aspect,DrawOptions options) {
    std::vector<Vertex> vertices;
    if(!Validate(frame)||!std::isfinite(aspect)||aspect<=0) return vertices;
    Mat4 matrix=ViewProjection(frame.camera,aspect);
    auto line=[&](Vec3 a,Vec3 b,Color c) {
        Vec3 x,y;if(!ProjectSegment(matrix,a,b,x,y)) return;
        vertices.push_back({x.x,x.y,c.r,c.g,c.b,c.a});vertices.push_back({y.x,y.y,c.r,c.g,c.b,c.a});
    };
    if(options.grid) for(int i=-5;i<=5;++i) {
        Color color=i==0?Color{.36f,.42f,.35f,.9f}:Color{.19f,.23f,.20f,.8f};
        line({float(i),0,-5},{float(i),0,5},color);line({-5,0,float(i)},{5,0,float(i)},color);
    }
    if(options.bounds) for(const auto& e:frame.entities) {
        Vec3 p[8];
        for(int i=0;i<8;++i) p[i]=e.position+Vec3{(i&1?1.f:-1.f)*e.half_extents.x,
            (i&2?1.f:-1.f)*e.half_extents.y,(i&4?1.f:-1.f)*e.half_extents.z};
        for(int i=0;i<8;++i) for(int bit=1;bit<=4;bit*=2) if(!(i&bit)) line(p[i],p[i|bit],e.color);
    }
    return vertices;
}
}
