package com.darkcode.login.floating;

import android.content.Context;
import android.graphics.*;
import android.view.MotionEvent;
import android.view.View;
import java.util.function.IntConsumer;

/** Hue/saturation wheel with a separate brightness control. */
final class ColorWheelView extends View {
    private final float[] hsv = new float[3];
    private final Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final IntConsumer changed;
    ColorWheelView(Context context, int color, IntConsumer changed) {
        super(context);
        Color.colorToHSV(color, hsv);
        this.changed = changed;
        setContentDescription("Seleccionar tono y saturación del color");
    }
    float brightness() { return hsv[2]; }
    int color() { return Color.HSVToColor(hsv); }
    void setBrightness(float value) { hsv[2] = value; invalidate(); changed.accept(color()); }
    @Override protected void onMeasure(int widthSpec, int heightSpec) {
        int preferred = Math.round(148 * getResources().getDisplayMetrics().density);
        int width = resolveSize(preferred, widthSpec);
        setMeasuredDimension(width, resolveSize(width, heightSpec));
    }
    @Override protected void onDraw(Canvas canvas) {
        float cx = getWidth() / 2f, cy = getHeight() / 2f;
        float radius = Math.max(1, Math.min(cx, cy) - 8 * getResources().getDisplayMetrics().density);
        paint.setStyle(Paint.Style.FILL);
        paint.setColor(Color.WHITE);
        paint.setShader(new SweepGradient(cx, cy, new int[]{Color.RED, Color.YELLOW,
                Color.GREEN, Color.CYAN, Color.BLUE, Color.MAGENTA, Color.RED}, null));
        canvas.drawCircle(cx, cy, radius, paint);
        paint.setShader(new RadialGradient(cx, cy, radius, Color.WHITE, Color.TRANSPARENT, Shader.TileMode.CLAMP));
        canvas.drawCircle(cx, cy, radius, paint);
        paint.setShader(null);
        // Keep the palette visible at zero brightness; the swatch shows the result.
        double angle = Math.toRadians(hsv[0]);
        float x = cx + (float)Math.cos(angle) * hsv[1] * radius;
        float y = cy + (float)Math.sin(angle) * hsv[1] * radius;
        radius = 6 * getResources().getDisplayMetrics().density;
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeWidth(4); paint.setColor(Color.BLACK);
        canvas.drawCircle(x, y, radius, paint);
        paint.setStrokeWidth(2); paint.setColor(Color.WHITE); canvas.drawCircle(x, y, radius, paint);
    }
    @Override public boolean onTouchEvent(MotionEvent event) {
        int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_CANCEL) {
            getParent().requestDisallowInterceptTouchEvent(false); return true;
        }
        if (action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_MOVE && action != MotionEvent.ACTION_UP) return false;
        getParent().requestDisallowInterceptTouchEvent(action != MotionEvent.ACTION_UP);
        float dx = event.getX() - getWidth() / 2f, dy = event.getY() - getHeight() / 2f;
        float radius = Math.max(1, Math.min(getWidth(), getHeight()) / 2f - 8 * getResources().getDisplayMetrics().density);
        hsv[0] = ((float)Math.toDegrees(Math.atan2(dy, dx)) + 360) % 360;
        hsv[1] = Math.min(1, (float)Math.hypot(dx, dy) / radius);
        invalidate(); changed.accept(color());
        if (action == MotionEvent.ACTION_UP) performClick();
        return true;
    }
    @Override public boolean performClick() { super.performClick(); return true; }
}
