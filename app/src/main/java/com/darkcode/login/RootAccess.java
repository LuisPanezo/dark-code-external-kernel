package com.darkcode.login;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.util.concurrent.TimeUnit;

/** Requests consent from the installed superuser manager; only checks the UID. */
final class RootAccess {
    static final class Result {
        final boolean granted;
        final String message;
        Result(boolean granted, String message) { this.granted = granted; this.message = message; }
    }
    static Result request() throws InterruptedException {
        Process process = null;
        try {
            process = new ProcessBuilder("su", "-c", "id -u")
                    .redirectErrorStream(true).start();
            process.getOutputStream().close();
            if (!process.waitFor(60, TimeUnit.SECONDS)) {
                return new Result(false, "La solicitud root agotó el tiempo de espera.");
            }
            if (process.exitValue() != 0) return new Result(false, "Acceso root denegado o no disponible.");
            try (BufferedReader reader = new BufferedReader(
                    new InputStreamReader(process.getInputStream()))) {
                boolean granted = "0".equals(reader.readLine());
                return new Result(granted, granted ? "Acceso root concedido." : "No se concedió acceso root.");
            }
        } catch (IOException e) {
            return new Result(false, "Root no disponible. Se necesita un dispositivo rooteado con gestor de superusuario.");
        } finally {
            if (process != null) {
                process.destroy();
                if (process.isAlive()) process.destroyForcibly();
                try { process.getInputStream().close(); } catch (IOException ignored) { }
                try { process.getErrorStream().close(); } catch (IOException ignored) { }
            }
        }
    }
}
