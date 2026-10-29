package com.darkcode.login.ui;
import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.widget.Button;
import android.widget.TextView;
public final class Palette {
    public static final int CREAM=0xFFF5F2EC, DARK=0xFF191E18, FIELD=0xFF11150F,
        GOLD=0xFFE8DDC5, MUTED=0xFFBFC2B7, BORDER=0xFF3C4337;
    private Palette() {}
    public static int dp(Context c,int v) {return Math.round(v*c.getResources().getDisplayMetrics().density);}
    public static GradientDrawable background(Context c,int color,int radius) {
        GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(dp(c,radius));
        d.setStroke(dp(c,1),BORDER);return d;
    }
    public static TextView text(Context c,String text,int size,int color,boolean bold) {
        TextView t=new TextView(c);t.setText(text);t.setTextSize(size);t.setTextColor(color);
        if(bold)t.setTypeface(Typeface.DEFAULT,Typeface.BOLD);return t;
    }
    public static Button button(Context c,String text,boolean primary) {
        Button b=new Button(c);b.setText(text);b.setAllCaps(false);b.setTextSize(14);
        b.setTextColor(primary?FIELD:CREAM);b.setMinWidth(0);b.setMinimumWidth(0);
        b.setPadding(dp(c,8),0,dp(c,8),0);
        b.setBackground(new RippleDrawable(ColorStateList.valueOf(0x33888888),
            background(c,primary?GOLD:FIELD,12),null));return b;
    }
}
