package com.darkcode.login;

/** Local input checks only. Authorization must be performed by the server. */
public final class LoginPolicy {
    private LoginPolicy() {}
    public static String validate(String username, String pin) {
        if (username == null || username.trim().isEmpty()) return "Escribe tu usuario.";
        if (username.trim().length() > 128) return "El usuario es demasiado largo.";
        if (pin == null || !pin.matches("[0-9]{4}")) return "Escribe tu PIN de 4 dígitos.";
        return null;
    }
}
