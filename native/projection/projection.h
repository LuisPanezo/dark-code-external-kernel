#pragma once
#include "core/frame_snapshot.h"
namespace dc {
Mat4 ViewProjection(const CameraFrame& camera,float aspect);
// Homogeneous clipping to OpenGL's [-w,+w] frustum. Outputs NDC coordinates.
bool ProjectSegment(const Mat4& matrix,Vec3 a,Vec3 b,Vec3& out_a,Vec3& out_b);
bool ProjectPoint(const Mat4& matrix,Vec3 point,Vec3& ndc);
}
