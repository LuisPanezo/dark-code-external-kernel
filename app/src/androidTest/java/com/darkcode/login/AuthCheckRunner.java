package com.darkcode.login;

import android.app.Instrumentation;
import android.content.Context;
import android.os.Bundle;
import java.io.File;
import java.nio.charset.StandardCharsets;
import java.nio.file.Files;
import org.json.JSONObject;

/** Opt-in live auth test, installed only in the separate test APK. No UI or root operations. */
public final class AuthCheckRunner extends Instrumentation {
    @Override public void onCreate(Bundle arguments) { super.onCreate(arguments); start(); }
    @Override public void onStart() {
        Bundle result = new Bundle();
        File credentials = new File(getTargetContext().getFilesDir(), "auth-test.json");
        String stage = "read_test_credentials";
        AuthClient client = null;
        AuthClient.Session session = null;
        try {
            byte[] bytes = Files.readAllBytes(credentials.toPath());
            Files.delete(credentials.toPath());
            JSONObject input;
            try { input = new JSONObject(new String(bytes, StandardCharsets.UTF_8)); }
            finally { java.util.Arrays.fill(bytes, (byte)0); }
            String device = getTargetContext().getSharedPreferences("installation", Context.MODE_PRIVATE).getString("id", null);
            if (device == null) throw new IllegalStateException("Installation ID absent");
            client = new AuthClient(BuildConfig.AUTH_BASE_URL, device);
            stage = "incorrect_password";
            boolean wrongPasswordRejected = false;
            try { client.login(input.getString("username"), java.util.UUID.randomUUID().toString()); }
            catch (AuthClient.AuthException expected) {
                wrongPasswordRejected = expected.getMessage().startsWith("Acceso rechazado.");
            }
            if (!wrongPasswordRejected) throw new AssertionError("Incorrect password did not produce the expected rejection");
            stage = "login";
            session = client.login(input.getString("username"), input.getString("password"));
            input.remove("password");
            if (session.subscriptionExpiresAt <= System.currentTimeMillis()) throw new AssertionError("Missing subscription expiry");
            stage = "session_check";
            client.check(session);
            stage = "logout";
            if (!client.logout(session)) throw new AssertionError("Logout not confirmed");
            stage = "revoked_session";
            boolean rejected = false;
            try { client.check(session); } catch (AuthClient.AuthException expected) { rejected = true; }
            if (!rejected) throw new AssertionError("Revoked session accepted");
            session = null;
            result.putString("result", "PASS: incorrect password rejected; Android login, subscription expiry, session check, logout and revoked token");
            finish(-1, result);
        } catch (Exception | AssertionError e) {
            result.putString("stage", stage);
            result.putString("result", e instanceof AuthClient.AuthException ? e.getMessage() : e.getClass().getSimpleName());
            finish(0, result);
        } finally {
            credentials.delete();
            if (session != null && client != null) try { client.logout(session); } catch (Exception ignored) { }
        }
    }
}
