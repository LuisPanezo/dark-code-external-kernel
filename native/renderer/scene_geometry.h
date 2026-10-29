#pragma once
#include "core/frame_snapshot.h"
namespace dc {
struct Vertex {float x,y,r,g,b,a;};
struct DrawOptions {bool grid{true},bounds{true};};
std::vector<Vertex> BuildScene(const FrameSnapshot& frame,float aspect,DrawOptions options);
}
