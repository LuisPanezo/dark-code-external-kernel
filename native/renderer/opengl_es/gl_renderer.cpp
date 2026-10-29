#include "gl_renderer.h"
#include <cstddef>
namespace dc {
static GLuint Shader(GLenum type,const char* code) {
    GLuint shader=glCreateShader(type);glShaderSource(shader,1,&code,nullptr);glCompileShader(shader);
    GLint ok;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) {glDeleteShader(shader);return 0;}return shader;
}
bool GlRenderer::SurfaceCreated() {
    // Called after EGL recreation: old names belong to a lost context, do not delete them here.
    program_=buffer_=0;
    GLuint vertex=Shader(GL_VERTEX_SHADER,"attribute vec2 position;attribute vec4 color;varying vec4 tint;void main(){gl_Position=vec4(position,0.,1.);tint=color;}");
    GLuint fragment=Shader(GL_FRAGMENT_SHADER,"precision mediump float;varying vec4 tint;void main(){gl_FragColor=tint;}");
    if(!vertex||!fragment) {if(vertex)glDeleteShader(vertex);if(fragment)glDeleteShader(fragment);return false;}
    program_=glCreateProgram();glAttachShader(program_,vertex);glAttachShader(program_,fragment);
    glBindAttribLocation(program_,0,"position");glBindAttribLocation(program_,1,"color");glLinkProgram(program_);
    glDeleteShader(vertex);glDeleteShader(fragment);
    GLint ok;glGetProgramiv(program_,GL_LINK_STATUS,&ok);
    if(!ok) {glDeleteProgram(program_);program_=0;return false;}
    glGenBuffers(1,&buffer_);return buffer_!=0;
}
void GlRenderer::Draw(const FrameSnapshot* frame,int width,int height,DrawOptions options) {
    if(width<=0||height<=0)return;
    glViewport(0,0,width,height);glClearColor(.055f,.071f,.062f,1);glClear(GL_COLOR_BUFFER_BIT);
    if(!frame||!program_)return;
    auto vertices=BuildScene(*frame,float(width)/height,options);if(vertices.empty())return;
    glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
    glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(Vertex)),vertices.data(),GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);
    glVertexAttribPointer(1,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,r)));
    glLineWidth(2);glDrawArrays(GL_LINES,0,GLsizei(vertices.size()));
    glDisableVertexAttribArray(0);glDisableVertexAttribArray(1);glBindBuffer(GL_ARRAY_BUFFER,0);
}
void GlRenderer::Release() {if(buffer_)glDeleteBuffers(1,&buffer_);if(program_)glDeleteProgram(program_);program_=buffer_=0;}
}
