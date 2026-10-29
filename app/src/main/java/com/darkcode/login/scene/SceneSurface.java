package com.darkcode.login.scene;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public final class SceneSurface extends GLSurfaceView implements GLSurfaceView.Renderer {
    public interface Listener { void onStats(String text, int entities); }
    private final Handler clock = new Handler(Looper.getMainLooper());
    private long handle, lastStats;
    private int width, height;
    private boolean active = true;
    private volatile boolean closed, paused;
    private volatile int mode, flags = 3;
    private volatile String latestStatus = "Preparando escena…";
    private volatile int entities;
    private Listener listener;
    private final Runnable tick = new Runnable() {
        @Override public void run() {
            if (!active || closed || !isAttachedToWindow()) return;
            requestRender(); clock.postDelayed(this, 33);
        }
    };
    public SceneSurface(Context context) {
        super(context);
        setEGLContextClientVersion(2);
        setEGLConfigChooser(8, 8, 8, 8, 0, 0);
        setRenderer(this);
        setRenderMode(RENDERMODE_WHEN_DIRTY);
        setContentDescription("Escena simulada con tres objetos de colores");
    }
    public void setListener(Listener listener) { this.listener = listener; }
    public void setMode(int mode) { this.mode = mode; }
    public void setPaused(boolean paused) { this.paused = paused; }
    public void setGrid(boolean value) { flags = value ? flags | 1 : flags & ~1; }
    public void setBounds(boolean value) { flags = value ? flags | 2 : flags & ~2; }
    public int entityCount() { return entities; }
    public String latestStatus() { return latestStatus; }
    @Override public void onSurfaceCreated(GL10 gl, EGLConfig config) {
        if (closed) return;
        if (handle == 0) handle = NativeBridge.create();
        if (handle == 0 || !NativeBridge.surfaceCreated(handle)) publish("No se pudo iniciar el dibujo", 0);
    }
    @Override public void onSurfaceChanged(GL10 gl, int width, int height) { this.width = width; this.height = height; }
    @Override public void onDrawFrame(GL10 gl) {
        if (closed || handle == 0) return;
        int count = NativeBridge.draw(handle, width, height, mode, paused, flags);
        if (SystemClock.uptimeMillis() - lastStats >= 500) {
            lastStats = SystemClock.uptimeMillis(); publish(NativeBridge.status(handle), count);
        }
    }
    private void publish(String text, int count) {
        latestStatus = text; entities = count;
        clock.post(() -> { if (!closed && listener != null) listener.onStats(text, count); });
    }
    @Override protected void onAttachedToWindow() { super.onAttachedToWindow(); clock.post(tick); }
    @Override public void onPause() {
        if (closed || !active) return;
        active = false; clock.removeCallbacks(tick);
        queueEvent(() -> { if (handle != 0) NativeBridge.setActive(handle, false); });
        super.onPause();
    }
    @Override public void onResume() {
        if (closed || active) return;
        active = true; super.onResume();
        queueEvent(() -> { if (handle != 0) NativeBridge.setActive(handle, true); });
        clock.post(tick);
    }
    public void release() {
        if (closed) return;
        closed = true; active = false; clock.removeCallbacksAndMessages(null); listener = null;
        queueEvent(() -> { if (handle != 0) { NativeBridge.destroy(handle); handle = 0; } });
    }
    @Override protected void onDetachedFromWindow() { release(); super.onDetachedFromWindow(); }
}
