package com.darkcode.login;
import org.junit.Test;
import static org.junit.Assert.*;

public class LoginPolicyTest {
    @Test public void rejectsMissingAndWhitespaceUsername() {
        assertNotNull(LoginPolicy.validate(null, "1234"));
        assertNotNull(LoginPolicy.validate("  ", "1234"));
    }
    @Test public void acceptsPasswordsWithoutApplyingRegistrationRules() {
        assertNull(LoginPolicy.validate("user", "Correct horse 🔐 42!"));
        assertNull(LoginPolicy.validate("user", " spaces remain "));
        assertNull(LoginPolicy.validate("user", "short"));
    }
    @Test public void rejectsMissingOrOversizedPasswords() {
        for (String p : new String[]{null, "", "x".repeat(1025)})
            assertNotNull(LoginPolicy.validate("user", p));
    }
    @Test public void rejectsOversizedUsername() {
        assertNotNull(LoginPolicy.validate("x".repeat(129), "1234"));
    }
}
