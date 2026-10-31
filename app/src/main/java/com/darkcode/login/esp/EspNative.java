package com.darkcode.login.esp;

/**
 * Puente mínimo con el módulo ESP nativo.
 *
 * Requisitos en el dispositivo: driver /dev/phmeop cargado (insmod phmeop.ko)
 * y que el proceso de la app tenga root (CAP_SYS_ADMIN); si no, attach()
 * devuelve DC_ESP_RESULT_NO_DRIVER y status() explica el motivo.
 *
 * El hilo de adquisición lo arranca attach(); esta clase nunca hace ioctl.
 * El dibujo (por ejemplo la ventana ImGui) se hace en código nativo con
 * dc_esp_poll(); desde Java basta con encender/apagar y leer el estado.
 */
public final class EspNative {
    static { System.loadLibrary("darkcode_scene"); }

    /** Igual que DcEspResult en esp_api.h. */
    public static final int RESULT_OK = 0;
    public static final int RESULT_ALREADY = 1;
    public static final int RESULT_NO_PID = 2;
    public static final int RESULT_NO_DRIVER = 3;
    public static final int RESULT_NO_WORLD = 4;
    public static final int RESULT_FAILED = 5;

    /** Igual que DcEspState en esp_api.h. */
    public static final int STATE_DETACHED = 0;
    public static final int STATE_STARTING = 1;
    public static final int STATE_NO_PID = 2;
    public static final int STATE_NO_DRIVER = 3;
    public static final int STATE_NO_WORLD = 4;
    public static final int STATE_NO_CAMERA = 5;
    public static final int STATE_LIVE = 6;
    public static final int STATE_ERROR = 7;

    private EspNative() {}

    /** packageName nulo = "com.garena.game.df"; pid &lt;= 0 = buscar por paquete. */
    public static native int attach(String packageName, int pid);
    public static native void detach();
    public static native boolean setEnabled(boolean enabled);
    public static native boolean isEnabled();
    public static native String status();
    public static native int state();
    public static native int targetCount();
}
