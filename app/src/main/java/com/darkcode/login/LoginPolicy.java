package com.darkcode.login;

/** Local input checks only. Authorization must be performed by the server. */
public final class LoginPolicy {
    private LoginPolicy() {}
    public static String validate(String username, String password) {
        if (username == null || username.trim().isEmpty()) return "Escribe tu usuario.";
        if (username.trim().length() > 128) return "El usuario es demasiado largo.";
        if (password == null || password.isEmpty()) return "Escribe tu contraseña.";
        if (password.length() > 1024) return "La contraseña es demasiado larga.";
        return null;
    }
}
