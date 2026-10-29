#pragma once
#include "renderer/scene_geometry.h"
#include <GLES2/gl2.h>
namespace dc {
class GlRenderer {
    GLuint program_{},buffer_{};
public:
    bool SurfaceCreated();
    void Draw(const FrameSnapshot* frame,int width,int height,DrawOptions options);
    void Release();
};
}
