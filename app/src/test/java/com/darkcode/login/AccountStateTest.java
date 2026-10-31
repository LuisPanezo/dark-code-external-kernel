package com.darkcode.login;

import org.junit.After;
import org.junit.Test;
import static org.junit.Assert.*;

public class AccountStateTest {
    @After public void reset() { AccountState.clear(); }
    @Test public void panelRequiresSessionAndRootConsent() {
        AccountState.clear();
        AccountState.rootGranted = true;
        assertFalse(AccountState.canOpenPanel());
        AccountState.session = new AuthClient.Session("test", "ana", System.currentTimeMillis() + 60_000);
        AccountState.rootGranted = false;
        assertFalse(AccountState.canOpenPanel());
        AccountState.rootGranted = true;
        assertTrue(AccountState.canOpenPanel());
        AccountState.clear();
        assertFalse(AccountState.canOpenPanel());
    }
    @Test public void expiredSessionCannotOpenPanel() {
        AccountState.session = new AuthClient.Session("test", "ana", System.currentTimeMillis() - 1);
        AccountState.rootGranted = true;
        assertFalse(AccountState.canOpenPanel());
    }
    @Test public void sessionExpiryIsNotSubscriptionExpiry() {
        AccountState.session = new AuthClient.Session("test", "ana", System.currentTimeMillis() + 60_000);
        assertEquals("Días restantes: sin datos", AccountState.remaining());
        AccountState.session.subscriptionExpiresAt = System.currentTimeMillis() + 2 * 86_400_000L;
        assertEquals("2 días restantes", AccountState.remaining());
        AccountState.session.subscriptionExpiresAt = System.currentTimeMillis() - 1;
        assertEquals("Suscripción vencida", AccountState.remaining());
    }
}
