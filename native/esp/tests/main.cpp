#include <cstdio>
#include <exception>

#include "esp/tests/esp_tests.h"

int main() {
    try {
        const int passed = dc::esp::RunEspTests();
        std::printf("%d ESP checks passed\n", passed);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAILED: %s\n", error.what());
        return 1;
    }
}
