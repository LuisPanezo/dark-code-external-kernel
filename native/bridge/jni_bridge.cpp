#include <jni.h>
#include "runtime/frame_runtime.h"
#include "renderer/opengl_es/gl_renderer.h"
#ifdef DARKCODE_TESTING
#include "tests/core_tests.h"
#endif
#include <string>
namespace {
struct Engine {dc::FrameRuntime runtime;dc::GlRenderer renderer;std::string status="Esperando datos";bool ready{};};
Engine* Get(jlong handle) {return reinterpret_cast<Engine*>(handle);}
}
extern "C" JNIEXPORT jlong JNICALL Java_com_darkcode_login_scene_NativeBridge_create(JNIEnv*,jclass) {
    try {auto engine=std::make_unique<Engine>();engine->runtime.Start();return reinterpret_cast<jlong>(engine.release());}
    catch(...) {return 0;}
}
extern "C" JNIEXPORT jboolean JNICALL Java_com_darkcode_login_scene_NativeBridge_surfaceCreated(JNIEnv*,jclass,jlong handle) {
    auto e=Get(handle);if(!e)return false;e->ready=e->renderer.SurfaceCreated();return e->ready;
}
extern "C" JNIEXPORT jint JNICALL Java_com_darkcode_login_scene_NativeBridge_draw(JNIEnv*,jclass,jlong handle,jint width,jint height,jint mode,jboolean paused,jint flags) {
    auto e=Get(handle);if(!e)return 0;
    try {
        e->runtime.SetMode(mode);e->runtime.SetPaused(paused);auto latest=e->runtime.Latest();
        e->status=e->ready?dc::StatusName(latest.status):"No se pudo iniciar OpenGL";
        e->renderer.Draw(latest.frame.get(),width,height,{bool(flags&1),bool(flags&2)});
        return latest.frame?jint(latest.frame->entities.size()):0;
    } catch(...) {e->status="Error de escena";return 0;}
}
extern "C" JNIEXPORT jstring JNICALL Java_com_darkcode_login_scene_NativeBridge_status(JNIEnv* env,jclass,jlong handle) {
    auto e=Get(handle);return env->NewStringUTF(e?e->status.c_str():"Detenido");
}
extern "C" JNIEXPORT void JNICALL Java_com_darkcode_login_scene_NativeBridge_setActive(JNIEnv*,jclass,jlong handle,jboolean active) {
    auto e=Get(handle);if(!e)return;
    try {if(active)e->runtime.Start();else e->runtime.Stop();}catch(...) {e->status="No se pudo iniciar la escena";}
}
extern "C" JNIEXPORT void JNICALL Java_com_darkcode_login_scene_NativeBridge_destroy(JNIEnv*,jclass,jlong handle) {
    auto e=Get(handle);if(!e)return;e->runtime.Stop();e->renderer.Release();delete e;
}
extern "C" JNIEXPORT jstring JNICALL Java_com_darkcode_login_scene_NativeBridge_runCoreTests(JNIEnv* env,jclass) {
    std::string report;
#ifdef DARKCODE_TESTING
    try {report=std::to_string(dc::RunCoreTests())+" native checks passed";}
    catch(const std::exception& e) {report=std::string("FAILED: ")+e.what();}
#else
    report="Test runner unavailable in release";
#endif
    return env->NewStringUTF(report.c_str());
}
