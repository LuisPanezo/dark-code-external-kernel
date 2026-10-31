package com.darkcode.login;

/** Process-local account state. No credentials are written to disk or service intents. */
public final class AccountState {
    static volatile AuthClient.Session session;
    public static volatile boolean rootGranted;
    private AccountState() { }
    public static boolean authenticated() {
        AuthClient.Session current = session;
        return current != null && current.expiresAt > System.currentTimeMillis();
    }
    public static boolean canOpenPanel() { return authenticated() && rootGranted; }
    public static String username() { return authenticated() ? session.username : "Sin sesión"; }
    public static String plan() { return authenticated() ? session.plan : "Inicia sesión"; }
    public static String remaining() {
        if (!authenticated() || session.subscriptionExpiresAt <= 0) return "Días restantes: sin datos";
        long remaining = session.subscriptionExpiresAt - System.currentTimeMillis();
        return remaining <= 0 ? "Suscripción vencida" :
                ((remaining + 86_399_999L) / 86_400_000L) + " días restantes";
    }
    static void clear() { session = null; rootGranted = false; }
}
