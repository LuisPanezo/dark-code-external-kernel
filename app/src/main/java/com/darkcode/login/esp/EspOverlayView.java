package com.darkcode.login.esp;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Typeface;
import android.view.View;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;

/**
 * Vista transparente que dibuja el ESP leyendo `esp_df pipe` (root).
 * No hace ioctl ni toca memoria del juego: solo parsea el flujo y pinta.
 *
 * Formato del pipe por frame:
 *   B <count> <x,y,w,h,mate,team,hp,dist,nombre> <...> ...
 */
public final class EspOverlayView extends View {
    private final Paint darkPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint linePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint boxDarkPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint boxPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint textPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Object lock = new Object();
    private final List<Box> boxes = new ArrayList<>();

    private Thread reader;
    private volatile boolean running;
    private int screenWidth = 1;
    private int screenHeight = 1;

    private static final class Box {
        float x, y, w, h;
        boolean mate;
        int team;
        float hp;
        float dist;
        String name = "";
    }

    public EspOverlayView(Context context) {
        super(context);
        setBackgroundColor(Color.TRANSPARENT);
        darkPaint.setStyle(Paint.Style.STROKE);
        darkPaint.setStrokeCap(Paint.Cap.ROUND);
        linePaint.setStyle(Paint.Style.STROKE);
        linePaint.setStrokeCap(Paint.Cap.ROUND);
        boxDarkPaint.setStyle(Paint.Style.STROKE);
        boxDarkPaint.setStrokeJoin(Paint.Join.ROUND);
        boxPaint.setStyle(Paint.Style.STROKE);
        boxPaint.setStrokeJoin(Paint.Join.ROUND);
        textPaint.setTypeface(Typeface.DEFAULT_BOLD);
        textPaint.setTextAlign(Paint.Align.CENTER);
        fillPaint.setStyle(Paint.Style.FILL);
    }

    /* Aliado verde · bot naranja · enemigo rojo. */
    private static int colorOf(Box box) {
        if (box.mate) return 0xFF00E878;
        if (box.name.isEmpty()) return 0xFFFFA500;
        return 0xFFFF4D4D;
    }

    /* Tamaño REAL del display: se usa para proyectar y para el ancla. */
    public void setScreenSize(int width, int height) {
        if (width <= 1 || height <= 1) return;
        final boolean changed = width != screenWidth || height != screenHeight;
        screenWidth = width;
        screenHeight = height;
        if (changed && running) {
            stop();
            start();
        }
    }

    /* Origen de la snapline desde las preferencias del panel (op2_position):
     * 0 = Arriba, 1 = Abajo, 2 = Centro. */
    private float anchorY() {
        int position = 2;
        try {
            position = getContext().getSharedPreferences("panel_options", Context.MODE_PRIVATE)
                    .getInt("op2_position", 2);
        } catch (RuntimeException ignored) {
        }
        if (position == 0) return 0f;
        if (position == 1) return screenHeight;
        return screenHeight * 0.5f;
    }

    public void start() {
        if (reader != null) return;
        running = true;
        reader = new Thread(this::readLoop, "esp-pipe");
        reader.start();
    }

    public void stop() {
        running = false;
        if (reader != null) {
            reader.interrupt();
            reader = null;
        }
        synchronized (lock) {
            boxes.clear();
        }
        postInvalidate();
    }

    /* Extrae el esp_df empaquetado en assets, lo copia a /data/local/tmp como
     * root y devuelve su ruta. Si algo falla, usa el binario ya existente
     * (push manual). Así la APK es autosuficiente: no depende de un push. */
    private String espDfPath() {
        try {
            final File local = new File(getContext().getCacheDir(), "esp_df");
            if (!local.exists() || local.length() < 1024) {
                try (InputStream in = getContext().getAssets().open("esp_df");
                     FileOutputStream out = new FileOutputStream(local)) {
                    final byte[] buf = new byte[65536];
                    int n;
                    while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
                    out.flush();
                }
            }
            final Process p = new ProcessBuilder("su", "-c",
                    "cp " + local.getAbsolutePath() + " /data/local/tmp/esp_df && " +
                    "chmod 755 /data/local/tmp/esp_df")
                    .redirectErrorStream(true).start();
            p.getOutputStream().close();
            p.waitFor();
        } catch (Exception ignored) {
        }
        return "/data/local/tmp/esp_df";
    }

    /* Vigilante: si el proceso muere, se relanza al segundo. Mata antes
     * cualquier esp_df huérfano que pueda tener el driver ocupado (EBUSY). */
    private void readLoop() {
        while (running) {
            Process process = null;
            try {
                killStale();
                final String screen = screenWidth + "x" + screenHeight;
                final String bin = espDfPath();
                process = new ProcessBuilder("su", "-c",
                        bin + " pipe --screen " + screen)
                        .redirectErrorStream(true).start();
                process.getOutputStream().close();
                BufferedReader in = new BufferedReader(
                        new InputStreamReader(process.getInputStream()));
                String line;
                while (running && (line = in.readLine()) != null) {
                    parse(line);
                }
            } catch (IOException ignored) {
            } finally {
                if (process != null) process.destroy();
            }
            if (running) {
                try {
                    Thread.sleep(1000);
                } catch (InterruptedException ignored) {
                    break;
                }
            }
        }
    }

    private void killStale() {
        try {
            Process p = new ProcessBuilder("su", "-c", "pkill -f 'esp_df pipe'")
                    .redirectErrorStream(true).start();
            p.getOutputStream().close();
            p.waitFor();
        } catch (Exception ignored) {
        }
    }

    private void parse(String line) {
        if (!line.startsWith("B ")) return;
        // Formato: "B <box1> <box2> ... <count>". El último token es el contador.
        String[] tokens = line.substring(2).trim().split("\\s+");
        synchronized (lock) {
            boxes.clear();
            for (int i = 0; i < tokens.length - 1; i++) {
                String[] f = tokens[i].split(",");
                if (f.length < 8) continue;
                try {
                    Box box = new Box();
                    box.x = Float.parseFloat(f[0]);
                    box.y = Float.parseFloat(f[1]);
                    box.w = Float.parseFloat(f[2]);
                    box.h = Float.parseFloat(f[3]);
                    box.mate = "1".equals(f[4]);
                    box.team = Integer.parseInt(f[5]);
                    box.hp = Float.parseFloat(f[6]);
                    box.dist = Float.parseFloat(f[7]);
                    box.name = f.length > 8 ? f[8] : "";
                    boxes.add(box);
                } catch (NumberFormatException ignored) {
                }
            }
        }
        postInvalidate();
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        if (screenWidth <= 1 && w > 1 && h > 1) {
            setScreenSize(w, h);
        }
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (screenWidth <= 1 || screenHeight <= 1) return;
        final float anchorX = screenWidth * 0.5f;
        final float anchorY = anchorY();
        synchronized (lock) {
            for (Box box : boxes) {
                drawLine(canvas, box, anchorX, anchorY);
                drawBox(canvas, box);
                drawText(canvas, box);
            }
        }
    }

    /* Caja con borde oscuro alrededor del objetivo, color según tipo. */
    private void drawBox(Canvas canvas, Box box) {
        final int color = colorOf(box);
        final float left = box.x;
        final float top = box.y;
        final float right = box.x + box.w;
        final float bottom = box.y + box.h;

        boxDarkPaint.setColor(0xFF000000);
        boxDarkPaint.setAlpha(150);
        boxDarkPaint.setStrokeWidth(3f);
        canvas.drawRect(left, top, right, bottom, boxDarkPaint);

        boxPaint.setColor(color);
        boxPaint.setAlpha(235);
        boxPaint.setStrokeWidth(1.5f);
        canvas.drawRect(left, top, right, bottom, boxPaint);

        // Barra de vida (borde izquierdo), verde→amarillo→rojo según HP.
        if (box.hp >= 0f) {
            final float frac = Math.max(0f, Math.min(1f, box.hp / 100f));
            final int hpColor = frac > 0.5f ? 0xFF00E878
                    : (frac > 0.25f ? 0xFFFFC400 : 0xFFFF4D4D);
            final float barW = Math.max(2.5f, box.w * 0.15f);
            fillPaint.setColor(0xFF000000);
            fillPaint.setAlpha(170);
            canvas.drawRect(left - barW - 2f, top, left - 1f, bottom, fillPaint);
            fillPaint.setColor(hpColor);
            fillPaint.setAlpha(230);
            canvas.drawRect(left - barW, bottom - box.h * frac, left - 1f, bottom, fillPaint);
        }
    }

    /* Línea con degradado + borde oscuro, color según tipo. */
    private void drawLine(Canvas canvas, Box box, float ax, float ay) {
        final int color = colorOf(box);
        final float ox = box.x + box.w * 0.5f;
        final float oy = box.y + box.h * 0.5f;
        final int segments = 12;
        for (int i = 0; i < segments; i++) {
            final float f0 = i / (float) segments;
            final float f1 = (i + 1) / (float) segments;
            final float x0 = ox + (ax - ox) * f0;
            final float y0 = oy + (ay - oy) * f0;
            final float x1 = ox + (ax - ox) * f1;
            final float y1 = oy + (ay - oy) * f1;

            darkPaint.setColor(0xFF000000);
            darkPaint.setAlpha(120);
            darkPaint.setStrokeWidth(2f);
            canvas.drawLine(x0, y0, x1, y1, darkPaint);

            linePaint.setColor(color);
            linePaint.setAlpha(100 + Math.round(155f * f1));
            linePaint.setStrokeWidth(1.2f);
            canvas.drawLine(x0, y0, x1, y1, linePaint);
        }
    }

    /* Nombre arriba, "distancia + vida" abajo. */
    private void drawText(Canvas canvas, Box box) {
        final int color = colorOf(box);
        final float cx = box.x + box.w * 0.5f;
        final float size = Math.max(9f, Math.min(18f, box.h * 0.28f));

        if (!box.name.isEmpty()) {
            drawText(canvas, box.name, cx, box.y - size * 0.4f, color, size);
        }
        StringBuilder info = new StringBuilder();
        info.append(String.format(Locale.US, "%.0fm", box.dist));
        if (box.hp > 0f) {
            info.append("  ").append((int) box.hp).append("hp");
        }
        drawText(canvas, info.toString(), cx, box.y + box.h + size * 1.1f, color, size);
    }

    private void drawText(Canvas canvas, String text, float cx, float y,
                          int color, float size) {
        textPaint.setTextSize(size);
        textPaint.setStrokeWidth(size * 0.12f);
        textPaint.setStyle(Paint.Style.STROKE);
        textPaint.setColor(0xFF000000);
        canvas.drawText(text, cx, y, textPaint);
        textPaint.setStyle(Paint.Style.FILL);
        textPaint.setColor(color);
        canvas.drawText(text, cx, y, textPaint);
    }
}
