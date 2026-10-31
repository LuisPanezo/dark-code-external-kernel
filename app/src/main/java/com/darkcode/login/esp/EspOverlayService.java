package com.darkcode.login.esp;

import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.PendingIntent;
import android.app.Service;
import android.content.Intent;
import android.content.pm.ServiceInfo;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.os.Build;
import android.os.IBinder;
import android.provider.Settings;
import android.util.DisplayMetrics;
import android.view.Gravity;
import android.view.WindowManager;

import com.darkcode.login.LoginActivity;

/**
 * Overlay a pantalla completa, transparente y que no captura toques.
 *
 * Clave para que las líneas caigan SOBRE los enemigos: proyectar con el tamaño
 * REAL de la pantalla (no el del layout, que recorta la zona del notch). Por eso
 * la ventana se crea con las dimensiones exactas del display.
 *
 * Probar desde adb:
 *   adb shell "su -c 'am start-foreground-service -n com.darkcode.login/.esp.EspOverlayService'"
 *   adb shell "su -c 'am stopservice -n com.darkcode.login/.esp.EspOverlayService'"
 */
public final class EspOverlayService extends Service {
    public static volatile boolean running;
    private WindowManager windows;
    private WindowManager.LayoutParams layout;
    private EspOverlayView view;

    @Override public IBinder onBind(Intent intent) { return null; }

    @Override public void onCreate() {
        super.onCreate();
        windows = (WindowManager) getSystemService(WINDOW_SERVICE);
    }

    @Override public int onStartCommand(Intent intent, int flags, int startId) {
        if (!Settings.canDrawOverlays(this)) {
            stopSelf();
            return START_NOT_STICKY;
        }
        showNotification();
        if (view == null) createWindow();
        running = true;
        // Pegajoso: si el sistema mata el servicio, lo vuelve a arrancar.
        return START_STICKY;
    }

    /* Tamaño real del display (incluye la zona del notch/cutout). */
    private void displaySize(int[] out) {
        if (Build.VERSION.SDK_INT >= 30) {
            Rect bounds = windows.getCurrentWindowMetrics().getBounds();
            out[0] = bounds.width();
            out[1] = bounds.height();
        } else {
            DisplayMetrics metrics = new DisplayMetrics();
            windows.getDefaultDisplay().getRealMetrics(metrics);
            out[0] = metrics.widthPixels;
            out[1] = metrics.heightPixels;
        }
    }

    private void showNotification() {
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(
                new NotificationChannel("esp", "ESP", NotificationManager.IMPORTANCE_LOW));
        PendingIntent open = PendingIntent.getActivity(this, 0,
                new Intent(this, LoginActivity.class),
                PendingIntent.FLAG_UPDATE_CURRENT | PendingIntent.FLAG_IMMUTABLE);
        Notification notification = new Notification.Builder(this, "esp")
                .setSmallIcon(android.R.drawable.ic_menu_view)
                .setContentTitle("Dark Code · ESP")
                .setContentText("Overlay de líneas activo")
                .setContentIntent(open)
                .setOngoing(true)
                .setOnlyAlertOnce(true)
                .build();
        if (Build.VERSION.SDK_INT >= 34) {
            startForeground(11, notification, ServiceInfo.FOREGROUND_SERVICE_TYPE_SPECIAL_USE);
        } else {
            startForeground(11, notification);
        }
    }

    private void createWindow() {
        int[] size = new int[2];
        displaySize(size);

        view = new EspOverlayView(this);
        view.setScreenSize(size[0], size[1]);

        layout = new WindowManager.LayoutParams(
                size[0], size[1],
                WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,
                WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE
                        | WindowManager.LayoutParams.FLAG_NOT_TOUCHABLE
                        | WindowManager.LayoutParams.FLAG_LAYOUT_NO_LIMITS,
                PixelFormat.TRANSLUCENT);
        layout.gravity = Gravity.TOP | Gravity.LEFT;
        layout.x = 0;
        layout.y = 0;
        if (Build.VERSION.SDK_INT >= 28) {
            layout.layoutInDisplayCutoutMode =
                    WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
        }
        windows.addView(view, layout);
        view.start();
    }

    /* Al girar la pantalla, re-lee el tamaño real y re-proyecta. */
    @Override public void onConfigurationChanged(android.content.res.Configuration config) {
        super.onConfigurationChanged(config);
        if (view == null || layout == null) return;
        final int[] size = new int[2];
        displaySize(size);
        layout.width = size[0];
        layout.height = size[1];
        try {
            windows.updateViewLayout(view, layout);
        } catch (RuntimeException ignored) {
        }
        view.setScreenSize(size[0], size[1]);
    }

    @Override public void onDestroy() {
        running = false;
        if (view != null) view.stop();
        if (view != null && layout != null) {
            try {
                windows.removeViewImmediate(view);
            } catch (Exception ignored) {
            }
        }
        view = null;
        stopForeground(STOP_FOREGROUND_REMOVE);
        super.onDestroy();
    }
}
