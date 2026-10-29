#pragma once
#include <array>
#include <cmath>
namespace dc {
struct Vec3 { float x{}, y{}, z{}; };
struct Vec4 { float x{}, y{}, z{}, w{}; };
struct Color { float r{1}, g{1}, b{1}, a{1}; };
inline Vec3 operator+(Vec3 a, Vec3 b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
inline Vec3 operator-(Vec3 a, Vec3 b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
inline Vec3 operator*(Vec3 a, float s) { return {a.x*s,a.y*s,a.z*s}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline float Length(Vec3 a) { return std::sqrt(Dot(a,a)); }
inline Vec3 Unit(Vec3 a) { float n=Length(a); return n>1e-6f ? a*(1/n) : Vec3{}; }
inline bool Finite(Vec3 a) { return std::isfinite(a.x)&&std::isfinite(a.y)&&std::isfinite(a.z); }
// Column-major, column vectors. Canonical right-handed world: +X right, +Y up, -Z forward; meters.
struct Mat4 {
    std::array<float,16> m{};
    static Mat4 Identity() { Mat4 a; a.m[0]=a.m[5]=a.m[10]=a.m[15]=1; return a; }
};
inline Vec4 Transform(const Mat4& a, Vec4 v) {
    const auto& m=a.m;
    return {m[0]*v.x+m[4]*v.y+m[8]*v.z+m[12]*v.w,
        m[1]*v.x+m[5]*v.y+m[9]*v.z+m[13]*v.w,
        m[2]*v.x+m[6]*v.y+m[10]*v.z+m[14]*v.w,
        m[3]*v.x+m[7]*v.y+m[11]*v.z+m[15]*v.w};
}
inline Mat4 operator*(const Mat4& a,const Mat4& b) {
    Mat4 c;
    for(int col=0;col<4;++col) for(int row=0;row<4;++row)
        for(int k=0;k<4;++k) c.m[col*4+row]+=a.m[k*4+row]*b.m[col*4+k];
    return c;
}
}
