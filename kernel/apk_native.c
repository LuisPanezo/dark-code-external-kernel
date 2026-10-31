/*
 * phmeop — cliente nativo de referencia (JNI) para la APK.
 *
 * Este archivo es SOLO una referencia para mostrar cómo la APK debe abrir
 * /dev/phmeop y usar los 3 ioctl desde código nativo. No está conectado a
 * la app principal de este repositorio.
 *
 * Requisitos:
 *   - La APK debe ejecutarse como root (su), o open() devuelve -EPERM.
 *   - Un solo cliente a la vez (un segundo open() devuelve -EBUSY).
 *   - Structs y números de ioctl idénticos a phmeop.h, byte a byte.
 *
 * Compilar con el NDK. Ajusta el nombre del paquete Java a tu APK.
 */

#include <jni.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "phmeop.h"

static int g_fd = -1;

/* --- open/close --- */

JNIEXPORT jint JNICALL
Java_com_example_phmeop_PhmeopNative_open(JNIEnv *env, jobject thiz)
{
    if (g_fd >= 0)
        return g_fd;
    g_fd = open("/dev/phmeop", O_RDWR);
    return g_fd; /* -1 en error; ver errno (-EPERM, -EBUSY, ...) */
}

JNIEXPORT void JNICALL
Java_com_example_phmeop_PhmeopNative_close(JNIEnv *env, jobject thiz)
{
    if (g_fd >= 0) {
        close(g_fd);
        g_fd = -1;
    }
}

/* --- read/write de memoria --- */

JNIEXPORT jint JNICALL
Java_com_example_phmeop_PhmeopNative_readMem(JNIEnv *env, jobject thiz,
                                             jint pid, jlong addr,
                                             jbyteArray buffer, jint size)
{
    struct CopyMemory cm;
    jbyte *buf;
    long ret;

    if (g_fd < 0)
        return -EBADF;

    buf = (*env)->GetByteArrayElements(env, buffer, NULL);
    if (!buf)
        return -ENOMEM;

    cm.pid    = (pid_t)pid;
    cm.addr   = (uintptr_t)addr;
    cm.buffer = buf;
    cm.size   = (size_t)size;

    ret = ioctl(g_fd, OP_READ_MEM, &cm);

    (*env)->ReleaseByteArrayElements(env, buffer, buf, 0);
    return (jint)ret; /* bytes leídos, o negativo = -errno */
}

JNIEXPORT jint JNICALL
Java_com_example_phmeop_PhmeopNative_writeMem(JNIEnv *env, jobject thiz,
                                              jint pid, jlong addr,
                                              jbyteArray buffer, jint size)
{
    struct CopyMemory cm;
    jbyte *buf;
    long ret;

    if (g_fd < 0)
        return -EBADF;

    buf = (*env)->GetByteArrayElements(env, buffer, NULL);
    if (!buf)
        return -ENOMEM;

    cm.pid    = (pid_t)pid;
    cm.addr   = (uintptr_t)addr;
    cm.buffer = buf;
    cm.size   = (size_t)size;

    ret = ioctl(g_fd, OP_WRITE_MEM, &cm);

    (*env)->ReleaseByteArrayElements(env, buffer, buf, JNI_ABORT);
    return (jint)ret; /* bytes escritos, o negativo = -errno */
}

/* --- base de un módulo --- */

JNIEXPORT jlong JNICALL
Java_com_example_phmeop_PhmeopNative_moduleBase(JNIEnv *env, jobject thiz,
                                                jint pid, jstring name)
{
    struct ModuleBase mb;
    const char *nameStr;
    long ret;

    if (g_fd < 0)
        return -EBADF;

    nameStr = (*env)->GetStringUTFChars(env, name, NULL);
    if (!nameStr)
        return -ENOMEM;

    mb.pid  = (pid_t)pid;
    mb.name = (char *)nameStr;
    mb.base = 0;

    ret = ioctl(g_fd, OP_MODULE_BASE, &mb);

    (*env)->ReleaseStringUTFChars(env, name, nameStr);

    if (ret < 0)
        return (jlong)ret;   /* negativo = -errno */
    return (jlong)mb.base;   /* dirección base */
}
