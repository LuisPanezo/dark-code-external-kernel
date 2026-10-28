package com.darkcode.login;

import org.json.JSONObject;
import java.io.ByteArrayOutputStream;
import java.io.InputStream;
import java.io.OutputStream;
import java.net.URL;
import javax.net.ssl.HttpsURLConnection;
import java.nio.charset.StandardCharsets;

/** Calls our HTTPS gateway. Never embeds SaoAuth credentials or stores passwords. */
final class AuthClient {
    static final class AuthException extends Exception {
        AuthException(String message) { super(message); }
    }
    static final class Session {
        final String token, username;
        final long expiresAt;
        Session(String token, String username, long expiresAt) {
            this.token = token; this.username = username; this.expiresAt = expiresAt;
        }
    }
    private final String baseUrl, deviceId;
    AuthClient(String baseUrl, String deviceId) { this.baseUrl = baseUrl; this.deviceId = deviceId; }

    Session login(String username, String password) throws Exception {
        JSONObject body = new JSONObject().put("device_id", deviceId).put("username", username).put("password", password);
        JSONObject data = request("/v1/login", body, null);
        String token = data.getString("session_token");
        long expiry = data.getLong("expires_at");
        if (!token.matches("[a-f0-9]{64}") || expiry <= System.currentTimeMillis())
            throw new AuthException("El servidor devolvió una sesión inválida.");
        return new Session(token, data.getString("username"), expiry);
    }
    void check(Session session) throws Exception {
        if (session.expiresAt <= System.currentTimeMillis()) throw new AuthException("Tu sesión venció. Inicia sesión de nuevo.");
        JSONObject data = request("/v1/session", new JSONObject().put("device_id", deviceId), session.token);
        if (!data.optBoolean("success", false)) throw new AuthException("No se pudo validar la sesión.");
    }
    boolean logout(Session session) throws Exception {
        return request("/v1/logout", new JSONObject().put("device_id", deviceId), session.token)
                .optBoolean("upstream_revoked", false);
    }
    private JSONObject request(String path, JSONObject body, String token) throws Exception {
        if (baseUrl.isEmpty()) throw new AuthException("El servicio de acceso aún no está configurado. No se enviaron tus datos.");
        URL url = new URL(baseUrl + path);
        if (!"https".equals(url.getProtocol())) throw new AuthException("La conexión debe usar HTTPS.");
        HttpsURLConnection connection = (HttpsURLConnection) url.openConnection();
        try {
            connection.setConnectTimeout(10000);
            connection.setReadTimeout(20000);
            connection.setInstanceFollowRedirects(false);
            connection.setUseCaches(false);
            connection.setRequestMethod("POST");
            connection.setRequestProperty("Content-Type", "application/json");
            connection.setRequestProperty("Accept", "application/json");
            if (token != null) connection.setRequestProperty("Authorization", "Bearer " + token);
            connection.setDoOutput(true);
            byte[] bytes = body.toString().getBytes(StandardCharsets.UTF_8);
            connection.setFixedLengthStreamingMode(bytes.length);
            try (OutputStream output = connection.getOutputStream()) { output.write(bytes); }
            finally { java.util.Arrays.fill(bytes, (byte) 0); }
            int code = connection.getResponseCode();
            if (code == 401 || code == 403) throw new AuthException("Acceso rechazado. Comprueba tus datos o vuelve a iniciar sesión.");
            if (code == 429) throw new AuthException("Demasiados intentos. Espera un minuto antes de volver a intentar.");
            if (code != 200) throw new AuthException("El servicio no está disponible. Intenta más tarde.");
            try (InputStream stream = connection.getInputStream(); ByteArrayOutputStream output = new ByteArrayOutputStream()) {
                byte[] buffer = new byte[1024]; int count;
                while ((count = stream.read(buffer)) != -1) {
                    if (output.size() + count > 16384) throw new AuthException("Respuesta del servidor inválida.");
                    output.write(buffer, 0, count);
                }
                return new JSONObject(output.toString(StandardCharsets.UTF_8.name()));
            }
        } finally { connection.disconnect(); }
    }
}
