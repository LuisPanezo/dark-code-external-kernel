package com.darkcode.login;
import org.junit.Test;
import static org.junit.Assert.*;

public class LoginPolicyTest {
    @Test public void rejectsMissingAndWhitespaceUsername() {
        assertNotNull(LoginPolicy.validate(null, "1234"));
        assertNotNull(LoginPolicy.validate("  ", "1234"));
    }
    @Test public void acceptsLeadingZeroWithoutNumericConversion() {
        assertNull(LoginPolicy.validate("user", "0123"));
    }
    @Test public void rejectsMalformedPins() {
        for (String p : new String[]{null, "", "123", "12345", "12a4", "１２３４", "123\n", " 123"})
            assertNotNull(LoginPolicy.validate("user", p));
    }
    @Test public void rejectsOversizedUsername() {
        assertNotNull(LoginPolicy.validate("x".repeat(129), "1234"));
    }
}
