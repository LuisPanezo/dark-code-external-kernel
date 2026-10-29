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
import android.os.Handler;
import android.os.Looper;
import java.util.UUID;
import java.util.concurrent.*;
import java.util.function.Consumer;

public final class LoginActivity extends Activity {
    private static final int CREAM = 0xFFF5F2EC, PANEL = 0xFF191E18,
            FIELD = 0xFF11150F, ACCENT = 0xFFE8DDC5, MUTED = 0xFFBFC2B7;
    private EditText username, password;
    private TextView status;
    private Button reveal;
    private boolean passwordVisible;
    private Button signIn, verify, signOut;
    private LinearLayout loginCard, accountCard;
    private TextView accountStatus;
    private AuthClient auth;
    private AuthClient.Session session;
    private final ExecutorService executor = Executors.newSingleThreadExecutor();
    private final Handler handler = new Handler(Looper.getMainLooper());
    private Future<?> pending;
    private int requestGeneration;
    private boolean busy;
    private final Runnable expire = () -> {
        session = null;
        accountCard.setVisibility(View.GONE);
        loginCard.setVisibility(View.VISIBLE);
        showStatus("Tu sesión venció. Inicia sesión de nuevo.");
    };

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        android.content.SharedPreferences prefs = getSharedPreferences("installation", MODE_PRIVATE);
        String deviceId = prefs.getString("id", null);
        if (deviceId == null) {
            deviceId = UUID.randomUUID().toString();
            prefs.edit().putString("id", deviceId).apply();
        }
        auth = new AuthClient(BuildConfig.AUTH_BASE_URL, deviceId);
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
        loginCard = card;
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
        LinearLayout passwordRow = new LinearLayout(this);
        passwordRow.setGravity(Gravity.CENTER_VERTICAL);
        passwordRow.setBackground(shape(FIELD, 16, 0xFF3C4337));
        password = input("Contraseña", InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_PASSWORD);
        password.setBackgroundColor(Color.TRANSPARENT);
        password.setFilters(new InputFilter[]{new InputFilter.LengthFilter(1024)});
        password.setTransformationMethod(PasswordTransformationMethod.getInstance());
        password.setAutofillHints(View.AUTOFILL_HINT_PASSWORD);
        password.setImeOptions(EditorInfo.IME_ACTION_DONE | EditorInfo.IME_FLAG_NO_PERSONALIZED_LEARNING);
        password.setSaveEnabled(false);
        password.setOnEditorActionListener((v, action, event) -> {
            if (action == EditorInfo.IME_ACTION_DONE) { submit(); return true; }
            return false;
        });
        passwordRow.addView(password, new LinearLayout.LayoutParams(0, dp(62), 1));
        reveal = new Button(this);
        reveal.setText(R.string.show_password);
        reveal.setTextColor(MUTED);
        reveal.setTextSize(12);
        reveal.setAllCaps(false);
        reveal.setPadding(0, 0, 0, 0);
        reveal.setMinWidth(0);
        reveal.setMinimumWidth(0);
        reveal.setBackground(ripple(FIELD, 16));
        reveal.setContentDescription("Mostrar contraseña");
        reveal.setOnClickListener(v -> setPasswordVisible(!passwordVisible));
        passwordRow.addView(reveal, new LinearLayout.LayoutParams(dp(60), dp(56)));
        card.addView(passwordRow);
        addGap(card, 18);
        Button submit = new Button(this);
        signIn = submit;
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
        accountCard = column();
        accountCard.setPadding(dp(24), dp(26), dp(24), dp(26));
        accountCard.setBackground(shape(PANEL, 28, 0xFF454A40));
        accountCard.setVisibility(View.GONE);
        accountCard.addView(text("Tu cuenta", 23, CREAM, true));
        accountStatus = text("", 16, MUTED, false);
        accountStatus.setPadding(0, dp(20), 0, dp(20));
        accountStatus.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
        accountCard.addView(accountStatus);
        verify = new Button(this);
        verify.setText("Verificar sesión");
        verify.setAllCaps(false);
        verify.setOnClickListener(v -> verifySession());
        accountCard.addView(verify, new LinearLayout.LayoutParams(-1, dp(56)));
        signOut = new Button(this);
        signOut.setText("Cerrar sesión");
        signOut.setAllCaps(false);
        signOut.setOnClickListener(v -> logout());
        accountCard.addView(signOut, new LinearLayout.LayoutParams(-1, dp(56)));
        content.addView(accountCard, new LinearLayout.LayoutParams(-1, -2));
        Button demo = com.darkcode.login.ui.Palette.button(this, "Explorar escena de prueba", false);
        demo.setOnClickListener(v -> startActivity(new android.content.Intent(this, com.darkcode.login.ui.SceneActivity.class)));
        LinearLayout.LayoutParams demoParams = new LinearLayout.LayoutParams(-1, dp(52));
        demoParams.topMargin = dp(20);
        content.addView(demo, demoParams);
        outer.setFocusableInTouchMode(true);
        setContentView(scroll);
    }

    private void submit() {
        if (busy) return;
        String error = LoginPolicy.validate(username.getText().toString(), password.getText().toString());
        if (error != null) { showStatus(error); return; }
        String userValue = username.getText().toString().trim();
        String passwordValue = password.getText().toString();
        password.getText().clear();
        setPasswordVisible(false);
        ((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE))
                .hideSoftInputFromWindow(password.getWindowToken(), 0);
        showStatus("Comprobando acceso…");
        runTask(() -> auth.login(userValue, passwordValue), result -> {
            session = result;
            loginCard.setVisibility(View.GONE);
            accountCard.setVisibility(View.VISIBLE);
            accountStatus.setText("Sesión validada para " + result.username + ".");
            handler.removeCallbacks(expire);
            handler.postDelayed(expire, Math.max(0, result.expiresAt - System.currentTimeMillis()));
        }, this::showStatus);
    }

    private void verifySession() {
        if (busy || session == null) return;
        AuthClient.Session current = session;
        accountStatus.setText("Verificando sesión…");
        runTask(() -> { auth.check(current); return true; }, ok ->
                accountStatus.setText("Sesión validada para " + current.username + "."), message -> {
            session = null;
            handler.removeCallbacks(expire);
            accountCard.setVisibility(View.GONE);
            loginCard.setVisibility(View.VISIBLE);
            showStatus(message);
        });
    }

    private void logout() {
        if (busy || session == null) return;
        AuthClient.Session current = session;
        session = null;
        handler.removeCallbacks(expire);
        accountCard.setVisibility(View.GONE);
        loginCard.setVisibility(View.VISIBLE);
        showStatus("Cerrando sesión…");
        runTask(() -> auth.logout(current), revoked -> showStatus(revoked ? "Sesión cerrada." :
                "Sesión cerrada aquí y en nuestro servidor. SaoAuth no confirmó el cierre."),
                message -> showStatus("Sesión eliminada del teléfono. No se confirmó el cierre remoto; vencerá en un máximo de 15 minutos."));
    }

    private <T> void runTask(Callable<T> task, Consumer<T> success, Consumer<String> failure) {
        setBusy(true);
        int generation = ++requestGeneration;
        pending = executor.submit(() -> {
            try {
                T result = task.call();
                handler.post(() -> {
                    if (generation != requestGeneration || isFinishing() || isDestroyed()) return;
                    setBusy(false); success.accept(result);
                });
            } catch (Exception e) {
                String message = e instanceof AuthClient.AuthException ? e.getMessage() :
                        "No se pudo conectar. Comprueba tu conexión e inténtalo de nuevo.";
                handler.post(() -> {
                    if (generation != requestGeneration || isFinishing() || isDestroyed()) return;
                    setBusy(false); failure.accept(message);
                });
            }
        });
    }
    private void setBusy(boolean value) {
        busy = value;
        signIn.setEnabled(!value); verify.setEnabled(!value); signOut.setEnabled(!value);
        username.setEnabled(!value); password.setEnabled(!value);
        signIn.setText(value ? "Conectando…" : getString(R.string.sign_in));
    }

    private void showStatus(String message) {
        status.setText(message);
        status.setVisibility(View.VISIBLE);
    }

    private void setPasswordVisible(boolean visible) {
        passwordVisible = visible;
        password.setTransformationMethod(visible ? null : PasswordTransformationMethod.getInstance());
        password.setSelection(password.length());
        reveal.setText(visible ? "Ocultar" : "Ver");
        reveal.setContentDescription(visible ? "Ocultar contraseña" : "Mostrar contraseña");
    }

    @Override protected void onStop() {
        super.onStop();
        ++requestGeneration;
        if (pending != null) pending.cancel(true);
        setBusy(false);
        password.getText().clear();
        setPasswordVisible(false);
    }
    @Override protected void onStart() {
        super.onStart();
        if (session != null) verifySession();
    }
    @Override protected void onDestroy() {
        ++requestGeneration;
        executor.shutdownNow();
        handler.removeCallbacksAndMessages(null);
        session = null;
        super.onDestroy();
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
