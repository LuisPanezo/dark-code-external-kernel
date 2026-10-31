#include "driver.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if defined(__linux__)
#  include <cstdlib>
#  include <dirent.h>
#endif

namespace dc::esp {

#if defined(__linux__)

/* ¿El proceso tiene ese módulo mapeado? Evita confundir un cmdline parecido
 * (por ejemplo un shell con el nombre del paquete) con el juego real. */
static bool MapsContain(pid_t pid, const char* module) {
    if (!module || !*module) return true;
    char path[64];
    std::snprintf(path, sizeof(path), "/proc/%d/maps", static_cast<int>(pid));
    FILE* file = std::fopen(path, "r");
    if (!file) return false;
    char line[1024];
    bool found = false;
    while (!found && std::fgets(line, sizeof(line), file)) {
        if (std::strstr(line, module)) found = true;
    }
    std::fclose(file);
    return found;
}

#endif

pid_t FindProcessId(const char* needle, const char* module) {
#if defined(__linux__)
    if (!needle || !*needle) return -1;
    DIR* directory = ::opendir("/proc");
    if (!directory) return -1;
    pid_t first_match = -1;
    pid_t found = -1;
    struct dirent* entry = nullptr;
    while ((entry = ::readdir(directory)) != nullptr) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9') continue;
        char path[64];
        std::snprintf(path, sizeof(path), "/proc/%s/cmdline", entry->d_name);
        FILE* file = std::fopen(path, "r");
        if (!file) continue;
        char buffer[256]{};
        size_t read_bytes = std::fread(buffer, 1, sizeof(buffer) - 1, file);
        std::fclose(file);
        // cmdline separa argumentos con NUL; el paquete es el primer argumento.
        if (!read_bytes || !std::strstr(buffer, needle)) continue;
        const pid_t pid = static_cast<pid_t>(std::atoi(entry->d_name));
        if (!module || !*module || MapsContain(pid, module)) {
            found = pid;
            break;
        }
        if (first_match < 0) first_match = pid;
    }
    ::closedir(directory);
    return found >= 0 ? found : first_match;
#else
    (void)needle;
    (void)module;
    return -1;
#endif
}

namespace {

#if defined(__linux__)

class LinuxProcessMap final : public IProcessMap {
public:
    bool writable_module_ranges(pid_t pid, const char* module,
                                std::vector<AddressRange>& out) override {
        out.clear();
        if (pid <= 0 || !module || !*module) return false;
        char path[64];
        std::snprintf(path, sizeof(path), "/proc/%d/maps", static_cast<int>(pid));
        FILE* file = std::fopen(path, "r");
        if (!file) return false;
        char line[1024];
        while (std::fgets(line, sizeof(line), file)) {
            unsigned long start = 0, end = 0;
            char permissions[8]{};
            if (std::sscanf(line, "%lx-%lx %7s", &start, &end, permissions) != 3) continue;
            if (permissions[0] != 'r' || permissions[1] != 'w' || end <= start) continue;
            // La ruta del mapping es lo primero que empieza por '/' en la línea.
            const char* mapped = std::strchr(line, '/');
            if (!mapped || !std::strstr(mapped, module)) continue;
            AddressRange range{static_cast<uintptr_t>(start), static_cast<uintptr_t>(end)};
            if (!out.empty() && out.back().end == range.start) out.back().end = range.end;
            else out.push_back(range);
        }
        std::fclose(file);
        return !out.empty();
    }
};

#else

class LinuxProcessMap final : public IProcessMap {
public:
    bool writable_module_ranges(pid_t, const char*, std::vector<AddressRange>& out) override {
        out.clear();
        return false;
    }
};

#endif

} // namespace

std::unique_ptr<IProcessMap> MakeLinuxProcessMap() {
    return std::make_unique<LinuxProcessMap>();
}

} // namespace dc::esp
