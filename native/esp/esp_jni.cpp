#include <jni.h>

#include <string>

#include "api/esp_api.h"

namespace {

std::string FromJava(JNIEnv* env, jstring value) {
    if (!value) return {};
    const char* chars = env->GetStringUTFChars(value, nullptr);
    if (!chars) return {};
    std::string copy(chars);
    env->ReleaseStringUTFChars(value, chars);
    return copy;
}

} // namespace

extern "C" JNIEXPORT jint JNICALL
Java_com_darkcode_login_esp_EspNative_attach(JNIEnv* env, jclass, jstring package, jint pid) {
    const std::string name = FromJava(env, package);
    return dc_esp_attach(name.empty() ? nullptr : name.c_str(), static_cast<int>(pid));
}

extern "C" JNIEXPORT void JNICALL
Java_com_darkcode_login_esp_EspNative_detach(JNIEnv*, jclass) {
    dc_esp_detach();
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_darkcode_login_esp_EspNative_setEnabled(JNIEnv*, jclass, jboolean enabled) {
    return dc_esp_set_enabled(enabled ? 1 : 0) ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jboolean JNICALL
Java_com_darkcode_login_esp_EspNative_isEnabled(JNIEnv*, jclass) {
    return dc_esp_enabled() ? JNI_TRUE : JNI_FALSE;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_darkcode_login_esp_EspNative_status(JNIEnv* env, jclass) {
    return env->NewStringUTF(dc_esp_status());
}

extern "C" JNIEXPORT jint JNICALL
Java_com_darkcode_login_esp_EspNative_state(JNIEnv*, jclass) {
    return dc_esp_state();
}

extern "C" JNIEXPORT jint JNICALL
Java_com_darkcode_login_esp_EspNative_targetCount(JNIEnv*, jclass) {
    return dc_esp_target_count();
}
