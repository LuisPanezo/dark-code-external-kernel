package com.darkcode.login;

import android.app.Activity;
import android.os.Bundle;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.RippleDrawable;
import android.content.res.ColorStateList;
import android.text.InputFilter;
import android.text.InputType;
import android.text.method.PasswordTransformationMethod;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.widget.*;

public final class LoginActivity extends Activity {
    private static final int CREAM = 0xFFF5F2EC, PANEL = 0xFF191E18,
            FIELD = 0xFF11150F, ACCENT = 0xFFE8DDC5, MUTED = 0xFFBFC2B7;
    private EditText username, pin;
    private TextView status;
    private Button reveal;
    private boolean pinVisible;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        if (!BuildConfig.DEBUG) getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        ScrollView scroll = new ScrollView(this);
        scroll.setBackgroundColor(CREAM);
        scroll.setFillViewport(true);
        scroll.setClipToPadding(false);
        scroll.setOnApplyWindowInsetsListener((v, insets) -> {
            v.setPadding(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                    insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
            return insets;
        });
        LinearLayout outer = column();
        outer.setGravity(Gravity.CENTER);
        outer.setPadding(dp(24), dp(28), dp(24), dp(28));
        scroll.addView(outer, new ScrollView.LayoutParams(-1, -1));
        LinearLayout content = column();
        int width = Math.min(getResources().getDisplayMetrics().widthPixels - dp(48), dp(420));
        outer.addView(content, new LinearLayout.LayoutParams(width, -2));

        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.phoenix);
        logo.setScaleType(ImageView.ScaleType.FIT_CENTER);
        logo.setImportantForAccessibility(View.IMPORTANT_FOR_ACCESSIBILITY_NO);
        content.addView(logo, new LinearLayout.LayoutParams(-1, dp(176)));
        TextView brand = text("DARK CODE", 22, FIELD, true);
        brand.setGravity(Gravity.CENTER);
        brand.setLetterSpacing(.13f);
        content.addView(brand);
        LinearLayout divider = new LinearLayout(this);
        divider.setGravity(Gravity.CENTER_VERTICAL);
        View left = line(), right = line();
        divider.addView(left, new LinearLayout.LayoutParams(0, dp(1), 1));
        TextView edition = text("EXTERNAL", 11, PANEL, true);
        edition.setLetterSpacing(.25f);
        edition.setPadding(dp(14), 0, dp(14), 0);
        divider.addView(edition);
        divider.addView(right, new LinearLayout.LayoutParams(0, dp(1), 1));
        LinearLayout.LayoutParams dividerParams = new LinearLayout.LayoutParams(-1, dp(42));
        dividerParams.bottomMargin = dp(12);
        content.addView(divider, dividerParams);

        LinearLayout card = column();
        card.setPadding(dp(24), dp(26), dp(24), dp(26));
        card.setBackground(shape(PANEL, 28, 0xFF454A40));
        card.setElevation(dp(7));
        content.addView(card, new LinearLayout.LayoutParams(-1, -2));
        TextView title = text("Bienvenido de nuevo", 23, CREAM, true);
        card.addView(title);
        addGap(card, 24);
        username = input("Usuario", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_NORMAL);
        username.setAutofillHints(View.AUTOFILL_HINT_USERNAME);
        username.setFilters(new InputFilter[]{new InputFilter.LengthFilter(128)});
        username.setImeOptions(EditorInfo.IME_ACTION_NEXT);
        card.addView(username, new LinearLayout.LayoutParams(-1, dp(62)));
        addGap(card, 16);
        LinearLayout pinRow = new LinearLayout(this);
        pinRow.setGravity(Gravity.CENTER_VERTICAL);
        pinRow.setBackground(shape(FIELD, 16, 0xFF3C4337));
        pin = input("PIN de 4 dígitos", InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_VARIATION_PASSWORD);
        pin.setBackgroundColor(Color.TRANSPARENT);
        pin.setFilters(new InputFilter[]{new InputFilter.LengthFilter(4)});
        pin.setTransformationMethod(PasswordTransformationMethod.getInstance());
        pin.setAutofillHints(View.AUTOFILL_HINT_PASSWORD);
        pin.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING);
        pin.setSaveEnabled(false);
        pin.setOnEditorActionListener((v, action, event) -> {
            if (action == EditorInfo.IME_ACTION_DONE) { submit(); return true; }
            return false;
        });
        pinRow.addView(pin, new LinearLayout.LayoutParams(0, dp(62), 1));
        reveal = new Button(this);
        reveal.setText(R.string.show_pin);
        reveal.setTextColor(MUTED);
        reveal.setTextSize(12);
        reveal.setAllCaps(false);
        reveal.setPadding(0, 0, 0, 0);
        reveal.setMinWidth(0);
        reveal.setMinimumWidth(0);
        reveal.setBackground(ripple(FIELD, 16));
        reveal.setContentDescription("Mostrar PIN");
        reveal.setOnClickListener(v -> setPinVisible(!pinVisible));
        pinRow.addView(reveal, new LinearLayout.LayoutParams(dp(60), dp(56)));
        card.addView(pinRow);
        addGap(card, 18);
        Button submit = new Button(this);
        submit.setText(R.string.sign_in);
        submit.setTextColor(FIELD);
        submit.setTextSize(18);
        submit.setAllCaps(false);
        submit.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        submit.setBackground(ripple(ACCENT, 17));
        submit.setOnClickListener(v -> submit());
        card.addView(submit, new LinearLayout.LayoutParams(-1, dp(64)));
        status = text("", 14, 0xFFFFD7A3, false);
        status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        status.setPadding(0, dp(16), 0, 0);
        status.setVisibility(View.GONE);
        card.addView(status);
        outer.setFocusableInTouchMode(true);
        setContentView(scroll);
    }

    private void submit() {
        String error = LoginPolicy.validate(username.getText().toString(), pin.getText().toString());
        if (error != null) { showStatus(error); return; }
        // Fail closed: no local success route or hardcoded credentials.
        pin.getText().clear();
        setPinVisible(false);
        ((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE))
                .hideSoftInputFromWindow(pin.getWindowToken(), 0);
        showStatus("El servicio de acceso aún no está configurado. No se enviaron tus datos.");
    }

    private void showStatus(String message) {
        status.setText(message);
        status.setVisibility(View.VISIBLE);
    }

    private void setPinVisible(boolean visible) {
        pinVisible = visible;
        pin.setTransformationMethod(visible ? null : PasswordTransformationMethod.getInstance());
        pin.setSelection(pin.length());
        reveal.setText(visible ? "Ocultar" : "Ver");
        reveal.setContentDescription(visible ? "Ocultar PIN" : "Mostrar PIN");
    }

    @Override protected void onStop() {
        super.onStop();
        pin.getText().clear();
        setPinVisible(false);
    }
    private EditText input(String hint, int type) {
        EditText v = new EditText(this);
        v.setHint(hint);
        v.setContentDescription(hint);
        v.setInputType(type);
        v.setSingleLine(true);
        v.setTextSize(17);
        v.setTextColor(CREAM);
        v.setHintTextColor(MUTED);
        v.setPadding(dp(18), 0, dp(12), 0);
        v.setBackground(shape(FIELD, 16, 0xFF3C4337));
        v.setFilterTouchesWhenObscured(true);
        v.setSaveEnabled(false);
        return v;
    }
    private LinearLayout column() {
        LinearLayout v = new LinearLayout(this);
        v.setOrientation(LinearLayout.VERTICAL);
        return v;
    }
    private TextView text(String s, float size, int color, boolean bold) {
        TextView v = new TextView(this);
        v.setText(s); v.setTextSize(size); v.setTextColor(color);
        if (bold) v.setTypeface(Typeface.DEFAULT, Typeface.BOLD);
        return v;
    }
    private View line() { View v = new View(this); v.setBackgroundColor(0xFFD7D1C5); return v; }
    private void addGap(LinearLayout parent, int height) { parent.addView(new View(this), new LinearLayout.LayoutParams(1, dp(height))); }
    private GradientDrawable shape(int fill, int radius, int stroke) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(fill); d.setCornerRadius(dp(radius)); d.setStroke(dp(1), stroke); return d;
    }
    private RippleDrawable ripple(int fill, int radius) {
        return new RippleDrawable(ColorStateList.valueOf(0x33888888), shape(fill, radius, fill), null);
    }
    private int dp(int value) { return Math.round(value * getResources().getDisplayMetrics().density); }
}
