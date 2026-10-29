package com.darkcode.login.scene;

/** All engine handles are owned by the GLSurfaceView render thread. */
public final class NativeBridge {
    static { System.loadLibrary("darkcode_scene"); }
    private NativeBridge() {}
    public static native long create();
    public static native boolean surfaceCreated(long handle);
    public static native int draw(long handle, int width, int height, int mode, boolean paused, int flags);
    public static native String status(long handle);
    public static native void setActive(long handle, boolean active);
    public static native void destroy(long handle);
    public static native String runCoreTests();
}
