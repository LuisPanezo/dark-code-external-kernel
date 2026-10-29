#include "projection.h"
#include <algorithm>
namespace dc {
Mat4 ViewProjection(const CameraFrame& c,float aspect) {
    if(!std::isfinite(aspect)||aspect<=0) return {};
    Vec3 f=Unit(c.target-c.position), s=Unit(Cross(f,c.up)), u=Cross(s,f);
    Mat4 v=Mat4::Identity();
    v.m[0]=s.x;v.m[4]=s.y;v.m[8]=s.z;v.m[12]=-Dot(s,c.position);
    v.m[1]=u.x;v.m[5]=u.y;v.m[9]=u.z;v.m[13]=-Dot(u,c.position);
    v.m[2]=-f.x;v.m[6]=-f.y;v.m[10]=-f.z;v.m[14]=Dot(f,c.position);
    Mat4 p;float t=1/std::tan(c.vertical_fov_radians*0.5f);
    p.m[0]=t/aspect;p.m[5]=t;p.m[10]=(c.far_plane+c.near_plane)/(c.near_plane-c.far_plane);
    p.m[11]=-1;p.m[14]=2*c.far_plane*c.near_plane/(c.near_plane-c.far_plane);
    return p*v;
}
static bool Finite4(Vec4 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z)&&std::isfinite(a.w); }
static float Plane(Vec4 p,int plane) {
    switch(plane) { case 0:return p.w+p.x;case 1:return p.w-p.x;case 2:return p.w+p.y;
        case 3:return p.w-p.y;case 4:return p.w+p.z;default:return p.w-p.z; }
}
bool ProjectSegment(const Mat4& m,Vec3 a,Vec3 b,Vec3& oa,Vec3& ob) {
    Vec4 ca=Transform(m,{a.x,a.y,a.z,1}),cb=Transform(m,{b.x,b.y,b.z,1});
    if(!Finite4(ca)||!Finite4(cb)) return false;
    float lo=0,hi=1;
    for(int i=0;i<6;++i) {
        float x=Plane(ca,i),y=Plane(cb,i);
        if(x<0 && y<0) return false;
        if(x<0) lo=std::max(lo,x/(x-y));
        if(y<0) hi=std::min(hi,x/(x-y));
        if(lo>hi) return false;
    }
    auto point=[&](float t,Vec3& o) {
        float w=ca.w+(cb.w-ca.w)*t;
        if(w<=1e-6f) return false;
        o={(ca.x+(cb.x-ca.x)*t)/w,(ca.y+(cb.y-ca.y)*t)/w,(ca.z+(cb.z-ca.z)*t)/w};
        return Finite(o);
    };
    return point(lo,oa)&&point(hi,ob);
}
bool ProjectPoint(const Mat4& m,Vec3 p,Vec3& ndc) { Vec3 other;return ProjectSegment(m,p,p,ndc,other); }
}
