/*
 * esp_df — herramienta de diagnóstico del módulo ESP.
 *
 *   esp_df probe                  comprueba driver, PID y GWorld y sale
 *   esp_df test [--ticks N]       N ticks con diagnóstico (por defecto 300)
 *   esp_df loop                   bucle continuo (Ctrl+C para salir)
 *   esp_df dump                   1 tick con cada objetivo y los bits crudos
 *
 * En el dispositivo, como root:
 *   adb shell "su -c /data/local/tmp/esp_df test"
 *
 * Sólo lee memoria del juego (OP_READ_MEM y OP_MODULE_*): no escribe nada.
 */
#include <algorithm>
#include <cctype>
#include <cmath>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "core/status.h"
#include "driver.h"
#include "offsets.h"
#include "projection/projection.h"
#include "projector.h"
#include "reader.h"
#include "ue.h"

using namespace dc::esp;

namespace {

volatile std::sig_atomic_t g_running = 1;

void OnSignal(int) { g_running = 0; }

float g_screen_width = 2400.f;
float g_screen_height = 1080.f;
int g_max_boxes = 12;
int g_ticks = 300;
std::string g_mode = "test";
bool g_raw = false;
bool g_quiet = false;
std::string g_out = "/data/local/tmp/esp.ppm";
int g_anchor = 2;   // ancla de las líneas: 0 arriba, 1 centro, 2 abajo

uintptr_t ParseHex(const char* text) {
    if (!text || !*text) return 0;
    return static_cast<uintptr_t>(std::strtoull(text, nullptr, 16));
}

float ParseFloat(const char* text, float fallback) {
    if (!text || !*text) return fallback;
    char* end = nullptr;
    const float value = std::strtof(text, &end);
    return (end && *end == '\0') ? value : fallback;
}

int ParseInt(const char* text, int fallback) {
    if (!text || !*text) return fallback;
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    return (end && *end == '\0') ? static_cast<int>(value) : fallback;
}

void ParseScreen(const char* text) {
    if (!text) return;
    float width = 0.f;
    float height = 0.f;
    if (std::sscanf(text, "%fx%f", &width, &height) == 2 && width > 1.f && height > 1.f) {
        g_screen_width = width;
        g_screen_height = height;
    }
}

void Usage() {
    std::printf(
        "uso: esp_df [probe|test|loop|dump|chain|camera|names|snapshot] [opciones]\n"
        "  --package <nombre>   paquete del juego (def. com.garena.game.df)\n"
        "  --module <nombre>    módulo UE (def. libUE4.so)\n"
        "  --pid <n>            PID explícito (si no, se busca por paquete)\n"
        "  --gworld <hex>       dirección de la variable GWorld (evita el escaneo)\n"
        "  --ticks <n>          ticks del modo test (def. 300)\n"
        "  --interval <ms>      espera entre ticks (def. 500 en test, 16 en loop)\n"
        "  --screen <WxH>       resolución del overlay (def. 2400x1080)\n"
        "  --fov <grados>       fuerza el FOV horizontal del juego\n"
        "  --fov-vertical       el FOV leído es vertical\n"
        "  --aspect <f>         relación de aspecto del snapshot (def. 16/9)\n"
        "  --max-dist <m>       distancia máxima a dibujar (def. 250)\n"
        "  --boxes <n>          cajas a imprimir por tick (def. 12)\n"
        "  --min-hp <hp>        descarta objetivos con vida menor (1 = saltar HP<=0)\n"
        "  --codec <modo>       plain | trailer | auto (def. auto)\n"
        "  --all                incluye aliados y actores sin equipo/vida\n"
        "  --raw                imprime los bits crudos del FVector\n"
        "  --quiet              sólo CAM/BOX/STATS\n");
}

std::string Hex(uintptr_t value) {
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "0x%llx",
                  static_cast<unsigned long long>(value));
    return buffer;
}

/* Deja el nombre sin comas/espacios para que no rompa el formato del pipe. */
std::string SanitizeName(const std::string& name) {
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        out.push_back(std::isalnum(u) ? c : '_');
    }
    return out;
}

/* ── Rasterizador mínimo: dibuja lo mismo que el overlay, sin ImGui ── */
struct Image {
    int width{};
    int height{};
    std::vector<unsigned char> rgb;

    Image(int w, int h) : width(w), height(h), rgb(size_t(w) * size_t(h) * 3, 0) {}

    void clear(unsigned char r, unsigned char g, unsigned char b) {
        for (size_t index = 0; index + 2 < rgb.size(); index += 3) {
            rgb[index] = r;
            rgb[index + 1] = g;
            rgb[index + 2] = b;
        }
    }

    void blend(int x, int y, int r, int g, int b, float alpha) {
        if (x < 0 || y < 0 || x >= width || y >= height) return;
        unsigned char* pixel = &rgb[(size_t(y) * size_t(width) + size_t(x)) * 3];
        pixel[0] = static_cast<unsigned char>(r * alpha + pixel[0] * (1.0f - alpha));
        pixel[1] = static_cast<unsigned char>(g * alpha + pixel[1] * (1.0f - alpha));
        pixel[2] = static_cast<unsigned char>(b * alpha + pixel[2] * (1.0f - alpha));
    }

    void line(float x0, float y0, float x1, float y1, int r, int g, int b, float alpha) {
        const float dx = x1 - x0;
        const float dy = y1 - y0;
        const int steps = static_cast<int>(std::max(std::fabs(dx), std::fabs(dy)));
        const int count = steps > 0 ? steps : 1;
        for (int step = 0; step <= count; ++step) {
            const float t = static_cast<float>(step) / count;
            blend(static_cast<int>(std::lround(x0 + dx * t)),
                  static_cast<int>(std::lround(y0 + dy * t)), r, g, b, alpha);
        }
    }

    bool save(const char* path) const {
        FILE* file = std::fopen(path, "wb");
        if (!file) return false;
        std::fprintf(file, "P6\n%d %d\n255\n", width, height);
        const size_t written = std::fwrite(rgb.data(), 1, rgb.size(), file);
        std::fclose(file);
        return written == rgb.size();
    }
};

void RenderSnapshot(const std::vector<ScreenBox>& boxes, const std::vector<Target>& targets,
                    int width, int height, int anchor, const char* path) {
    Image image(width, height);
    image.clear(22, 24, 28);

    // Referencia: centro vertical y horizontal.
    image.line(width * 0.5f, 0.f, width * 0.5f, float(height), 80, 84, 92, 0.35f);

    int drawn = 0;
    for (size_t index = 0; index < boxes.size() && index < targets.size(); ++index) {
        const ScreenBox& box = boxes[index];
        if (!box.visible) continue;
        const bool mate = targets[index].teammate;
        const int r = mate ? 110 : 255;
        const int g = mate ? 235 : 112;
        const int b = mate ? 160 : 112;
        ++drawn;

        // Caja fina (lo que el overlay dibuja con "Solo lineas" apagado).
        image.line(box.x, box.y, box.x + box.w, box.y, r, g, b, 0.85f);
        image.line(box.x, box.y + box.h, box.x + box.w, box.y + box.h, r, g, b, 0.85f);
        image.line(box.x, box.y, box.x, box.y + box.h, r, g, b, 0.85f);
        image.line(box.x + box.w, box.y, box.x + box.w, box.y + box.h, r, g, b, 0.85f);

        // Snapline VERDE con borde oscuro, igual que overlay_imgui.cpp.
        const float origin_x = box.x + box.w * 0.5f;
        const float origin_y = box.y + box.h * 0.5f;
        float anchor_y = float(height);
        if (anchor == 0) anchor_y = 0.f;
        else if (anchor == 1) anchor_y = height * 0.5f;
        const float anchor_x = width * 0.5f;

        constexpr int kSegments = 12;
        for (int segment = 0; segment < kSegments; ++segment) {
            const float from = static_cast<float>(segment) / kSegments;
            const float to = static_cast<float>(segment + 1) / kSegments;
            const float ax = origin_x + (anchor_x - origin_x) * from;
            const float ay = origin_y + (anchor_y - origin_y) * from;
            const float bx = origin_x + (anchor_x - origin_x) * to;
            const float by = origin_y + (anchor_y - origin_y) * to;
            const float alpha = (120.0f + 135.0f * to) / 255.0f;
            // borde oscuro (más grueso)
            image.line(ax, ay, bx, by, 0, 0, 0, 0.55f);
            // núcleo verde
            image.line(ax, ay, bx, by, 0, 255, 120, alpha);
        }
    }

    if (image.save(path)) {
        std::printf("[+] imagen %dx%d con %d objetivos: %s\n", width, height, drawn, path);
    } else {
        std::printf("[-] no se pudo escribir %s\n", path);
    }
}

/* Busca el FString del nombre dentro del PlayerState: puntero plausible,
 * num entre 2 y 60, max >= num y texto UTF-16 imprimible. */
void DumpNames(IMemoryDriver& driver, uintptr_t world) {
    PlayerChain chain;
    if (!ReadPlayerChain(driver, world, chain)) {
        std::printf("names: cadena del jugador incompleta\n");
        return;
    }
    struct Text { uintptr_t data; int32_t num; int32_t max; };

    int found = 0;
    const uintptr_t actor = chain.pawn;
    if (PlausiblePointer(actor)) {
        const uintptr_t state = ReadPtr(driver, actor + off::PLAYER_STATE);
        std::printf("actor %s  player_state(0x%lx) %s\n", Hex(actor).c_str(),
                    static_cast<unsigned long>(off::PLAYER_STATE), Hex(state).c_str());
        if (PlausiblePointer(state)) {
            for (uintptr_t offset = 0; offset <= 0x800; offset += 8) {
                Text text{};
                if (!Read(driver, state + offset, text)) break;
                if (!PlausiblePointer(text.data)) continue;
                if (text.num < 2 || text.num > 60) continue;
                if (text.max < text.num || text.max > 128) continue;

                char16_t buffer[61]{};
                const size_t want = static_cast<size_t>(text.num) * sizeof(char16_t);
                if (driver.read(text.data, buffer, want) != want) continue;

                std::string decoded;
                int printable = 0;
                for (int32_t i = 0; i < text.num; ++i) {
                    const char16_t value = buffer[i];
                    if (value == 0) break;
                    if (value >= 32 && value < 127) {
                        decoded.push_back(static_cast<char>(value));
                        ++printable;
                    } else {
                        decoded.push_back('.');
                    }
                }
                if (decoded.size() < 2) continue;
                if (printable * 2 < static_cast<int>(decoded.size())) continue;
                std::printf("  FString en ps+0x%llx -> \"%s\" (num=%d)\n",
                            static_cast<unsigned long long>(offset), decoded.c_str(), text.num);
                ++found;
            }
        }
    }
    if (!found) std::printf("names: sin FString plausible en el PlayerState\n");
}

/* Busca la POV real: recorre el PlayerCameraManager buscando el patrón
 * Location(3 floats) + Rotation(3 floats) + FOV(1 float) con FOV 30..150. */
void DumpCamera(IMemoryDriver& driver, uintptr_t world, const ModuleSpan& span) {
    PlayerChain chain;
    if (!ReadPlayerChain(driver, world, chain)) {
        std::printf("camera: cadena del jugador incompleta\n");
        return;
    }
    std::printf("controller %s   pawn %s   camara(0x%lx) %s\n", Hex(chain.controller).c_str(),
                Hex(chain.pawn).c_str(), static_cast<unsigned long>(off::CAMERA_MANAGER),
                Hex(chain.camera_manager).c_str());
    if (!chain.controller) return;

    int printed = 0;
    std::printf("span módulo: %s..%s\n", Hex(span.start).c_str(), Hex(span.end).c_str());

    // Posición del pawn: la cámara está a pocos metros, así que sirve de ancla.
    Vec3 pawn_cm{};
    {
        const uintptr_t root = ReadPtr(driver, chain.pawn + off::ROOT_COMPONENT);
        FEncVector raw{};
        if (root && Read(driver, root + off::REL_LOCATION, raw)) {
            pawn_cm = Vec3{raw.x, raw.y, raw.z};
        }
    }
    std::printf("pawn pos (cm): %.0f %.0f %.0f\n", pawn_cm.x, pawn_cm.y, pawn_cm.z);

    for (uintptr_t offset = 0x100; offset <= 0x800; offset += 8) {
        const uintptr_t candidate = ReadPtr(driver, chain.controller + offset);
        if (!PlausiblePointer(candidate)) continue;
        const uintptr_t vtable = ReadPtr(driver, candidate);
        const bool in_module = span.valid() && vtable >= span.start && vtable < span.end;
        if (offset == off::CAMERA_MANAGER || in_module) {
            std::printf("  pc+0x%-3llx -> %s  vtable %s%s\n",
                        static_cast<unsigned long long>(offset), Hex(candidate).c_str(),
                        Hex(vtable).c_str(), in_module ? "" : "  (fuera del módulo)");
        }
        if (!in_module) continue;

        std::vector<unsigned char> block(0x4000);
        const size_t got = driver.read(candidate, block.data(), block.size());
        if (got < 0x100) continue;

        if (offset == off::CAMERA_MANAGER) {
            std::printf("    leídos %zu B; floats en +0x3D0..+0x420:", got);
            for (size_t at = 0x3D0; at < 0x420 && at + 4 <= got; at += 4) {
                float value = 0.f;
                std::memcpy(&value, block.data() + at, sizeof(value));
                std::printf(" %.1f", value);
            }
            std::printf("\n");
        }

        int hits = 0;
        for (size_t at = 0; at + 32 <= got; at += 4) {
            float values[8];
            std::memcpy(values, block.data() + at, sizeof(values));

            // Variante A: Location(3) + Rotation(3) + FOV  (28 B)
            // Variante B: Location(3) + pad(1) + Rotation(3) + FOV  (32 B)
            for (int variant = 0; variant < 2; ++variant) {
                const int rot = variant == 0 ? 3 : 4;
                const float fov = values[variant == 0 ? 6 : 7];
                if (!(fov >= 30.f && fov <= 150.f)) continue;
                if (!(std::fabs(values[rot]) <= 89.f)) continue;
                const float dx = values[0] - pawn_cm.x;
                const float dy = values[1] - pawn_cm.y;
                const float dz = values[2] - pawn_cm.z;
                if (dx * dx + dy * dy + dz * dz > 2000.f * 2000.f) continue;
                std::printf("  pc+0x%llx obj %s POV+0x%llx [%s] loc=(%.0f,%.0f,%.0f) "
                            "rot=(%.0f,%.0f,%.0f) fov=%.0f\n",
                            static_cast<unsigned long long>(offset), Hex(candidate).c_str(),
                            static_cast<unsigned long long>(at),
                            variant == 0 ? "sin pad" : "con pad", values[0], values[1],
                            values[2], values[rot], values[rot + 1], values[rot + 2], fov);
                if (++hits >= 4) break;
            }
            if (hits >= 4) break;
        }
        if (hits) ++printed;
    }
    if (!printed) std::printf("camera: sin POV plausible en pc+0x100..0x800\n");
}

/* Camina la cadena del jugador local e imprime cada salto, para ver dónde se
 * corta cuando WorldScore se queda en 2. */
void DumpChain(IMemoryDriver& driver, uintptr_t world) {
    if (!world) {
        std::printf("chain: sin world (ejecuta probe/test primero)\n");
        return;
    }
    struct Table { uintptr_t data; uint32_t num; uint32_t max; };

    const uintptr_t level = ReadPtr(driver, world + off::PERSISTENT_LEVEL);
    Table actors{};
    Read(driver, level + off::ACTORS, actors);
    std::printf("world      %s\n", Hex(world).c_str());
    std::printf("level      %s  (+0x%lx)\n", Hex(level).c_str(),
                static_cast<unsigned long>(off::PERSISTENT_LEVEL));
    std::printf("actors     %s  num=%u max=%u\n", Hex(actors.data).c_str(), actors.num,
                actors.max);

    const uintptr_t game_instance = ReadPtr(driver, world + off::GAME_INSTANCE);
    std::printf("gameinst   %s  (world+0x%lx)\n", Hex(game_instance).c_str(),
                static_cast<unsigned long>(off::GAME_INSTANCE));
    if (!game_instance) return;

    // Vecindad del GameInstance: aquí se ve dónde está el TArray de verdad.
    unsigned char window[64]{};
    if (driver.read(game_instance + 0x20, window, sizeof(window)) == sizeof(window)) {
        std::printf("gi+0x20..0x60:");
        for (int index = 0; index < 64; index += 8) {
            uintptr_t value = 0;
            std::memcpy(&value, window + index, sizeof(value));
            std::printf(" %s", Hex(value).c_str());
        }
        std::printf("\n");
    }

    // Busca un TArray<ULocalPlayer*> en gi+0x00..0x100: puntero plausible,
    // num entre 1 y 8 y data[0]+0x30 apuntando a un PlayerController.
    int matches = 0;
    for (uintptr_t offset = 0; offset <= 0x100; offset += 8) {
        Table candidate{};
        if (!Read(driver, game_instance + offset, candidate)) continue;
        if (!PlausiblePointer(candidate.data)) continue;
        if (candidate.num < 1 || candidate.num > 8) continue;
        if (candidate.max < candidate.num || candidate.max > 64) continue;
        const uintptr_t first = ReadPtr(driver, candidate.data);
        if (!PlausiblePointer(first)) continue;
        const uintptr_t pc = ReadPtr(driver, first + off::PLAYER_CONTROLLER);
        if (!PlausiblePointer(pc)) continue;
        std::printf("  LocalPlayers? gi+0x%llx -> %s num=%u  lp=%s pc=%s\n",
                    static_cast<unsigned long long>(offset), Hex(candidate.data).c_str(),
                    candidate.num, Hex(first).c_str(), Hex(pc).c_str());
        ++matches;
    }
    if (!matches) std::printf("  (ningún TArray<ULocalPlayer*> en gi+0x00..0x100)\n");

    if (!matches) {
        // Plan B: el offset de OwningGameInstance puede ser otro. Se busca un
        // puntero en world+0x100..0x400 que contenga un TArray de jugadores.
        std::printf("buscando GameInstance en world+0x100..0x400…\n");
        int found = 0;
        for (uintptr_t offset = 0x100; offset <= 0x400; offset += 8) {
            const uintptr_t candidate_gi = ReadPtr(driver, world + offset);
            if (!PlausiblePointer(candidate_gi)) continue;
            for (uintptr_t inner = 0x00; inner <= 0x100; inner += 8) {
                Table players{};
                if (!Read(driver, candidate_gi + inner, players)) break;
                if (!PlausiblePointer(players.data)) continue;
                if (players.num < 1 || players.num > 8) continue;
                if (players.max < players.num || players.max > 64) continue;
                const uintptr_t first = ReadPtr(driver, players.data);
                if (!PlausiblePointer(first)) continue;
                const uintptr_t pc = ReadPtr(driver, first + off::PLAYER_CONTROLLER);
                if (!PlausiblePointer(pc)) continue;
                std::printf("  GameInstance? world+0x%llx -> %s (LocalPlayers +0x%llx, "
                            "num=%u, pc=%s)\n",
                            static_cast<unsigned long long>(offset),
                            Hex(candidate_gi).c_str(),
                            static_cast<unsigned long long>(inner), players.num,
                            Hex(pc).c_str());
                ++found;
                break;
            }
            if (found >= 4) break;
        }
        if (!found) std::printf("  (tampoco: el world candidato probablemente no es UWorld)\n");
    }

    const uintptr_t local_players = ReadPtr(driver, game_instance + off::LOCAL_PLAYERS);
    std::printf("localplrs  %s  (gi+0x%lx)\n", Hex(local_players).c_str(),
                static_cast<unsigned long>(off::LOCAL_PLAYERS));
    if (!local_players) return;

    Table players{};
    Read(driver, local_players, players);
    std::printf("  .data=%s num=%u max=%u\n", Hex(players.data).c_str(), players.num,
                players.max);

    const uintptr_t local_player = ReadPtr(driver, local_players);
    std::printf("localply   %s\n", Hex(local_player).c_str());
    if (!local_player) return;

    const uintptr_t controller = ReadPtr(driver, local_player + off::PLAYER_CONTROLLER);
    std::printf("controller %s  (lp+0x%lx)\n", Hex(controller).c_str(),
                static_cast<unsigned long>(off::PLAYER_CONTROLLER));
    if (!controller) return;

    const uintptr_t pawn = ReadPtr(driver, controller + off::ACK_PAWN);
    const uintptr_t camera = ReadPtr(driver, controller + off::CAMERA_MANAGER);
    std::printf("pawn       %s  (pc+0x%lx)\n", Hex(pawn).c_str(),
                static_cast<unsigned long>(off::ACK_PAWN));
    std::printf("camera     %s  (pc+0x%lx)\n", Hex(camera).c_str(),
                static_cast<unsigned long>(off::CAMERA_MANAGER));
}

/* Vuelca `len` bytes desde `addr` como hex, para localizar offsets a mano. */
void HexDump(IMemoryDriver& driver, uintptr_t addr, size_t len, const char* label) {
    std::vector<unsigned char> block(len);
    const size_t got = driver.read(addr, block.data(), len);
    std::printf("%s @ %s (%zu/%zu B):\n", label, Hex(addr).c_str(), got, len);
    for (size_t row = 0; row < got; row += 16) {
        std::printf("  +0x%04zx:", row);
        for (size_t col = 0; col < 16 && row + col < got; ++col)
            std::printf(" %02x", block[row + col]);
        std::printf("\n");
    }
}

/* Diagnóstico del equipo del jugador local: vuelca las fuentes candidatas
 * (PlayerState+0x658 byte y TeamComp+0x108/0x10C int32) y un hex de la zona. */
void DumpTeam(IMemoryDriver& driver, uintptr_t world) {
    PlayerChain chain;
    if (!ReadPlayerChain(driver, world, chain)) {
        std::printf("team: cadena del jugador local inválida\n");
        return;
    }
    const uintptr_t pawn = chain.pawn;
    const uintptr_t state = ReadPtr(driver, pawn + off::PLAYER_STATE);
    std::printf("pawn %s  player_state(0x%lx) %s\n", Hex(pawn).c_str(),
                static_cast<unsigned long>(off::PLAYER_STATE), Hex(state).c_str());

    if (PlausiblePointer(state)) {
        std::string name;
        if (ReadPlayerName(driver, state, name))
            std::printf("name(0x%lx) = '%s'\n",
                        static_cast<unsigned long>(off::PLAYER_NAME), name.c_str());
        int8_t b = -1;
        if (Read(driver, state + off::TEAM_ID_FALLBACK, b))
            std::printf("ps+0x%lx (int8) = %d\n",
                        static_cast<unsigned long>(off::TEAM_ID_FALLBACK),
                        static_cast<int>(b));
        HexDump(driver, state + 0x640, 0x40, "PlayerState +0x640..+0x680");
    }

    /* Escaneo: enteros pequeños (0..16) en el PlayerState, candidatos a
     * equipo/escuadra. El equipo es más amplio que la escuadra. */
    if (PlausiblePointer(state)) {
        std::vector<unsigned char> block(0x400);
        const size_t got = driver.read(state + 0x300, block.data(), block.size());
        std::printf("PlayerState ints (0..16) en +0x300..+0x700:\n");
        for (size_t off = 0; off + 4 <= got; off += 4) {
            int32_t v = 0;
            std::memcpy(&v, block.data() + off, 4);
            if (v >= 0 && v <= 16) {
                std::printf("  ps+0x%llx = %d\n",
                            static_cast<unsigned long long>(0x300 + off), v);
            }
        }
    }

    uintptr_t team_comp = 0;
    if (ResolveTeamComponent(driver, pawn, team_comp)) {
        int32_t fast = -1, camp = -1;
        ReadTeamValue(driver, team_comp, fast);
        Read(driver, team_comp + off::CAMP_ID, camp);
        std::printf("teamcomp %s  +0x%lx(int32)=%d  +0x%lx(int32)=%d\n",
                    Hex(team_comp).c_str(),
                    static_cast<unsigned long>(off::TEAM_ID_FAST), fast,
                    static_cast<unsigned long>(off::CAMP_ID), camp);
        HexDump(driver, team_comp + 0xF0, 0x40, "TeamComp +0xF0..+0x130");
    } else {
        std::printf("teamcomp: no resuelto en pawn+0x%lx\n",
                    static_cast<unsigned long>(off::TEAM_COMP));
    }
}

/* Vuelca una muestra de actores (posición cruda + candidatos de equipo) para
 * localizar offsets sin filtros. `limit` = cuántos actores con root válido. */
void ScanActors(IMemoryDriver& driver, uintptr_t world, int limit) {
    const uintptr_t level = ReadPtr(driver, world + off::PERSISTENT_LEVEL);
    if (!PlausiblePointer(level)) { std::printf("scan: nivel inválido\n"); return; }
    struct ActorsArray { uintptr_t data; uint32_t num; uint32_t max; };
    ActorsArray arr{};
    if (!Read(driver, level + off::ACTORS, arr) || !PlausiblePointer(arr.data) ||
        arr.num == 0 || arr.num > static_cast<uint32_t>(off::MAX_ACTORS)) {
        std::printf("scan: tabla de actores inválida\n");
        return;
    }
    std::vector<uintptr_t> list(arr.num);
    const size_t got = driver.read(arr.data, list.data(), list.size() * sizeof(uintptr_t));
    const size_t count = got / sizeof(uintptr_t);
    std::printf("scan: %u actores (leídos %zu)\n", arr.num, count);

    DecodeLimits limits;
    limits.reference_cm = 0.f;   // sin filtro de distancia: queremos verlo todo
    int shown = 0;
    for (size_t i = 0; i < count && shown < limit; ++i) {
        const uintptr_t actor = list[i];
        if (!PlausiblePointer(actor)) continue;
        const uintptr_t root = ReadPtr(driver, actor + off::ROOT_COMPONENT);
        if (!PlausiblePointer(root)) continue;
        FEncVector raw{};
        if (!Read(driver, root + off::REL_LOCATION, raw)) continue;

        uint32_t xb = 0, yb = 0, zb = 0;
        std::memcpy(&xb, &raw.x, 4);
        std::memcpy(&yb, &raw.y, 4);
        std::memcpy(&zb, &raw.z, 4);
        const DecodeResult d = DecodeVector(raw, nullptr, limits, Codec::Plain);

        const uintptr_t ps = ReadPtr(driver, actor + off::PLAYER_STATE);
        int32_t c650 = 0, c660 = 0, c664 = 0, c668 = 0;
        if (PlausiblePointer(ps)) {
            Read(driver, ps + 0x650, c650);
            Read(driver, ps + 0x660, c660);
            Read(driver, ps + 0x664, c664);
            Read(driver, ps + 0x668, c668);
        }

        std::printf("[%2zu] a=%s root=%s raw=(%08x %08x %08x) idx=%u enc=%d "
                    "pos=(%.0f,%.0f,%.0f) plaus=%d ps=%s ps+650=%d +660=%d +664=%d +668=%d\n",
                    i, Hex(actor).c_str(), Hex(root).c_str(), xb, yb, zb,
                    static_cast<unsigned>(raw.index), static_cast<int>(raw.encrypted),
                    d.value.x, d.value.y, d.value.z, d.plausible ? 1 : 0,
                    Hex(ps).c_str(), c650, c660, c664, c668);
        ++shown;
    }
    std::printf("scan: mostrados %d actores\n", shown);
}

/* Solo jugadores (con PlayerState): nombre + escuadra + candidatos de equipo,
 * para comparar aliados vs enemigos y localizar el id de EQUIPO (más amplio). */
void ScanTeams(IMemoryDriver& driver, uintptr_t world, int limit) {
    const uintptr_t level = ReadPtr(driver, world + off::PERSISTENT_LEVEL);
    if (!PlausiblePointer(level)) { std::printf("teams: nivel inválido\n"); return; }
    struct ActorsArray { uintptr_t data; uint32_t num; uint32_t max; };
    ActorsArray arr{};
    if (!Read(driver, level + off::ACTORS, arr) || !PlausiblePointer(arr.data) ||
        arr.num == 0 || arr.num > static_cast<uint32_t>(off::MAX_ACTORS)) {
        std::printf("teams: tabla de actores inválida\n");
        return;
    }
    std::vector<uintptr_t> list(arr.num);
    const size_t got = driver.read(arr.data, list.data(), list.size() * sizeof(uintptr_t));
    const size_t count = got / sizeof(uintptr_t);
    int shown = 0;
    for (size_t i = 0; i < count && shown < limit; ++i) {
        const uintptr_t actor = list[i];
        if (!PlausiblePointer(actor)) continue;
        const uintptr_t ps = ReadPtr(driver, actor + off::PLAYER_STATE);
        if (!PlausiblePointer(ps)) continue;   // solo jugadores reales
        std::string name;
        ReadPlayerName(driver, ps, name);
        int32_t squad = -1;
        ReadTeamFallback(driver, ps, squad);   // PlayerState +0x658 (byte)
        int32_t tc108 = -1, tc10c = -1;
        uintptr_t tc = 0;
        if (ResolveTeamComponent(driver, actor, tc)) {
            ReadTeamValue(driver, tc, tc108);        // TeamComp +0x108
            Read(driver, tc + off::CAMP_ID, tc10c);  // TeamComp +0x10C (camp/team)
        }
        std::printf("[%2zu] name='%-16s' squad(0x658)=%d  tc+0x108=%d  tc+0x10C=%d\n",
                    i, name.c_str(), squad, tc108, tc10c);
        ++shown;
    }
    std::printf("teams: mostrados %d jugadores\n", shown);
}

/* Escanea la cápsula del jugador local buscando la media-altura (half-height),
 * para que la caja se achique al agacharse/tumbarse. */
void DumpCapsule(IMemoryDriver& driver, uintptr_t world) {
    PlayerChain chain;
    if (!ReadPlayerChain(driver, world, chain)) {
        std::printf("capsule: cadena del jugador inválida\n");
        return;
    }
    const uintptr_t capsule = ReadPtr(driver, chain.pawn + off::CAPSULE);
    std::printf("pawn %s  capsule(0x%lx) %s\n", Hex(chain.pawn).c_str(),
                static_cast<unsigned long>(off::CAPSULE), Hex(capsule).c_str());
    if (!PlausiblePointer(capsule)) {
        std::printf("capsule: puntero inválido\n");
        return;
    }
    std::vector<unsigned char> block(0x400);
    const size_t got = driver.read(capsule, block.data(), block.size());
    std::printf("capsule floats 20..200 en +0x0..+0x400:\n");
    for (size_t off = 0; off + 4 <= got; off += 4) {
        float f = 0.f;
        std::memcpy(&f, block.data() + off, 4);
        if (f >= 20.f && f <= 200.f) {
            std::printf("  cap+0x%zx = %.1f\n", off, f);
        }
    }
}

/* Escanea el SkeletalMesh de un actor para localizar el array de huesos,
 * el ComponentToWorld y la rotación (yaw) en la cápsula. */
void DumpBones(IMemoryDriver& driver, uintptr_t actor) {
    const uintptr_t mesh = ReadPtr(driver, actor + off::MESH);
    std::printf("actor %s  mesh(0x%lx) %s\n", Hex(actor).c_str(),
                static_cast<unsigned long>(off::MESH), Hex(mesh).c_str());
    if (!PlausiblePointer(mesh)) {
        std::printf("bones: mesh inválido\n");
        return;
    }

    Vec3 pawn_pos{};
    {
        const uintptr_t root = ReadPtr(driver, actor + off::ROOT_COMPONENT);
        FEncVector raw{};
        if (root && Read(driver, root + off::REL_LOCATION, raw)) {
            pawn_pos = Vec3{raw.x, raw.y, raw.z};
        }
    }
    std::printf("actor pos: %.0f %.0f %.0f\n", pawn_pos.x, pawn_pos.y, pawn_pos.z);

    std::vector<unsigned char> block(0x800);
    const size_t got = driver.read(mesh, block.data(), block.size());

    std::printf("TArray<FTransform> candidatos (data+num+max):\n");
    for (size_t off = 0; off + 16 <= got; off += 8) {
        uintptr_t ptr = 0;
        int32_t num = 0, max = 0;
        std::memcpy(&ptr, block.data() + off, 8);
        std::memcpy(&num, block.data() + off + 8, 4);
        std::memcpy(&max, block.data() + off + 12, 4);
        if (PlausiblePointer(ptr) && num >= 10 && num <= 400 && max >= num && max <= 2000) {
            std::printf("  mesh+0x%zx: data=%s num=%d max=%d\n", off, Hex(ptr).c_str(), num, max);
        }
    }

    std::printf("FVector cerca del pawn (ComponentToWorld):\n");
    for (size_t off = 0; off + 12 <= got; off += 4) {
        float x = 0.f, y = 0.f, z = 0.f;
        std::memcpy(&x, block.data() + off, 4);
        std::memcpy(&y, block.data() + off + 4, 4);
        std::memcpy(&z, block.data() + off + 8, 4);
        const float dx = x - pawn_pos.x, dy = y - pawn_pos.y, dz = z - pawn_pos.z;
        if (dx * dx + dy * dy + dz * dz < 200.f * 200.f) {
            std::printf("  mesh+0x%zx: (%.0f %.0f %.0f)\n", off, x, y, z);
        }
    }

    // ComponentToWorld FTransform (Rotation 16B + Translation 12B + Scale 12B)
    std::printf("ComponentToWorld @ mesh+0x138 (rot+trans+scale):\n");
    {
        float f[10] = {0};
        if (driver.read(mesh + 0x138, f, sizeof(f)) == sizeof(f)) {
            std::printf("  rot=(%.3f %.3f %.3f %.3f) trans=(%.0f %.0f %.0f) scale=(%.3f %.3f %.3f)\n",
                        f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]);
        }
    }

    // Cápsula: ComponentToWorld (la rotación/yaw del personaje vive aquí).
    {
        const uintptr_t cap = ReadPtr(driver, actor + off::CAPSULE);
        std::printf("capsule(0x%lx) %s\n", static_cast<unsigned long>(off::CAPSULE), Hex(cap).c_str());
        if (PlausiblePointer(cap)) {
            float f[10] = {0};
            if (driver.read(cap + 0x138, f, sizeof(f)) == sizeof(f)) {
                std::printf("  cap+0x138 rot=(%.3f %.3f %.3f %.3f) trans=(%.0f %.0f %.0f) scale=(%.3f %.3f %.3f)\n",
                            f[0], f[1], f[2], f[3], f[4], f[5], f[6], f[7], f[8], f[9]);
            }
            // Buscar cuaterniones unitarios (|q|~1) en la cápsula.
            std::vector<unsigned char> cb(0x400);
            const size_t cgot = driver.read(cap, cb.data(), cb.size());
            std::printf("  capsula: cuaterniones unitarios (|q|~1):\n");
            for (size_t off = 0; off + 16 <= cgot; off += 4) {
                float q[4] = {0};
                std::memcpy(q, cb.data() + off, 16);
                const float n = q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3];
                if (n > 0.9f && n < 1.1f && (std::fabs(q[0]) > 0.01f || std::fabs(q[3]) > 0.01f)) {
                    const float yaw = std::atan2(2.f * (q[3] * q[2] + q[0] * q[1]),
                                                 1.f - 2.f * (q[1] * q[1] + q[2] * q[2])) * 57.2957795f;
                    std::printf("    cap+0x%zx: q=(%.3f %.3f %.3f %.3f) yaw=%.1f°\n",
                                off, q[0], q[1], q[2], q[3], yaw);
                }
            }
        }
    }

    // Primeros huesos de AMBOS arrays (0x750 y 0x760), stride 48, trans en +16.
    for (uintptr_t arr : {0x750u, 0x760u}) {
        const uintptr_t bone_data = ReadPtr(driver, mesh + arr);
        int32_t bone_num = 0;
        Read(driver, mesh + arr + 8, bone_num);
        std::printf("bones @ mesh+0x%lx: data=%s num=%d (primeros 5, trans en +16)\n",
                    static_cast<unsigned long>(arr), Hex(bone_data).c_str(), bone_num);
        std::vector<unsigned char> bb(48 * 5);
        if (bone_data && driver.read(bone_data, bb.data(), bb.size()) == bb.size()) {
            for (int i = 0; i < 5; ++i) {
                float trans[3] = {0};
                std::memcpy(trans, bb.data() + i * 48 + 16, 12);
                std::printf("  bone%d: (%.1f %.1f %.1f)\n", i, trans[0], trans[1], trans[2]);
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    Options options;
    int interval_ms = 0;

    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        auto next = [&]() -> const char* { return (index + 1 < argc) ? argv[++index] : nullptr; };

        if (argument == "probe" || argument == "test" || argument == "loop" || argument == "chain" || argument == "camera" || argument == "names" || argument == "snapshot" ||
            argument == "pipe" || argument == "team" || argument == "dump" || argument == "scan" ||
            argument == "teams" || argument == "capsule" || argument == "bones") {
            g_mode = argument;
        } else if (argument == "--package") {
            if (const char* value = next()) options.package = value;
        } else if (argument == "--module") {
            if (const char* value = next()) options.module = value;
        } else if (argument == "--pid") {
            options.pid = static_cast<pid_t>(ParseInt(next(), -1));
        } else if (argument == "--gworld") {
            options.gworld = ParseHex(next());
        } else if (argument == "--ticks") {
            g_ticks = ParseInt(next(), 300);
        } else if (argument == "--interval") {
            interval_ms = ParseInt(next(), 0);
        } else if (argument == "--screen") {
            ParseScreen(next());
        } else if (argument == "--fov") {
            options.hfov_degrees = ParseFloat(next(), 0.f);
        } else if (argument == "--fov-vertical") {
            options.fov_horizontal = false;
        } else if (argument == "--aspect") {
            options.aspect = ParseFloat(next(), 16.f / 9.f);
        } else if (argument == "--max-dist") {
            options.max_distance_m = ParseFloat(next(), 250.f);
        } else if (argument == "--out") {
            if (const char* value = next()) g_out = value;
        } else if (argument == "--anchor") {
            g_anchor = ParseInt(next(), 2);
        } else if (argument == "--min-hp") {
            options.min_health = ParseFloat(next(), 0.f);
        } else if (argument == "--boxes") {
            g_max_boxes = ParseInt(next(), 12);
        } else if (argument == "--codec") {
            const std::string mode = next() ? argv[index] : "auto";
            if (mode == "plain") options.codec = Codec::Plain;
            else if (mode == "trailer") options.codec = Codec::Trailer;
            else options.codec = Codec::Auto;
        } else if (argument == "--all") {
            options.skip_teammates = false;
            options.require_character = false;
        } else if (argument == "--raw") {
            g_raw = true;
        } else if (argument == "--quiet") {
            g_quiet = true;
        } else if (argument == "--help" || argument == "-h") {
            Usage();
            return 0;
        } else {
            std::printf("[-] opción desconocida: %s\n", argument.c_str());
            Usage();
            return 1;
        }
    }

    if (g_mode == "dump") g_ticks = 1;   // dump es un snapshot de 1 tick
    if (g_ticks < 1) g_ticks = 1;
    if (interval_ms <= 0) interval_ms = (g_mode == "loop" || g_mode == "pipe") ? 16 : 500;

    std::printf("=== ESP Delta Force (driver phmeop) ===\n");
    std::printf("[*] modo=%s paquete=%s módulo=%s\n", g_mode.c_str(),
                options.package.c_str(), options.module.c_str());

    auto driver = MakePhmeopDriver();
    IMemoryDriver* raw_driver = driver.get();
    EspReader reader(std::move(driver), MakeLinuxProcessMap(), options);

    dc::Status started = reader.Start();
    if (started != dc::Status::Ok) {
        std::printf("[-] %s (%s)\n", reader.StatusText().c_str(),
                    StateName(reader.CurrentState()));
    }

    if (g_mode == "snapshot") {
        dc::FrameSnapshot frame;
        if (reader.ReadLatest(frame) != dc::Status::Ok) {
            std::printf("[-] %s\n", reader.StatusText().c_str());
            return 1;
        }
        std::vector<ScreenBox> boxes;
        for (size_t index = 0; index < frame.entities.size(); ++index) {
            boxes.push_back(ProjectEntity(frame, reader.HorizontalFovRadians(), index,
                                          g_screen_width, g_screen_height));
        }
        RenderSnapshot(boxes, reader.Targets(), static_cast<int>(g_screen_width),
                       static_cast<int>(g_screen_height), g_anchor, g_out.c_str());
        return 0;
    }

    if (g_mode == "names") {
        std::printf("[*] estado=%s\n", StateName(reader.CurrentState()));
        DumpNames(*raw_driver, reader.World());
        return 0;
    }

    if (g_mode == "camera") {
        std::printf("[*] estado=%s\n", StateName(reader.CurrentState()));
        DumpCamera(*raw_driver, reader.World(), reader.Module());
        return 0;
    }

    // pipe: un frame por línea para que un overlay lo consuma por stdout.
    //   B <x,y,w,h,mate,team,hp,dist,nombre> ... <count>
    if (g_mode == "pipe") {
        dc::FrameSnapshot frame;
        auto last_start = std::chrono::steady_clock::now();
        while (g_running) {
            const dc::Status status = reader.ReadLatest(frame);
            if (status != dc::Status::Ok) {
                // Persistencia: si lleva >3 s sin leer (juego reiniciado con
                // otro PID, driver perdido, cambio de mapa), reabre y reescanea.
                const auto now = std::chrono::steady_clock::now();
                if (now - last_start > std::chrono::seconds(3)) {
                    last_start = now;
                    reader.Start();   // reencuentra PID, reabre driver, reescanea
                }
                std::printf("B 0\n");
                std::fflush(stdout);
                std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
                continue;
            }
            last_start = std::chrono::steady_clock::now();   // sano: reinicia el contador
            std::printf("B");
            int count = 0;
            const std::vector<Target>& targets = reader.Targets();

            // Matriz de proyección (una por frame) para convertir huesos a pantalla.
            const float aspect = g_screen_width / g_screen_height;
            dc::CameraFrame view = frame.camera;
            const float hfov = reader.HorizontalFovRadians();
            if (hfov > 0.f) {
                const float vfov = VerticalFovFromHorizontal(hfov, aspect);
                if (vfov > 0.02f && vfov < 3.05f) view.vertical_fov_radians = vfov;
            }
            const dc::Mat4 matrix = dc::ViewProjection(view, aspect);

            for (size_t index = 0; index < frame.entities.size() && index < targets.size();
                 ++index) {
                const ScreenBox box = ProjectEntity(frame, reader.HorizontalFovRadians(), index,
                                                    g_screen_width, g_screen_height);
                if (!box.visible) continue;
                // x,y,w,h,mate,team,hp,dist,nombre(sanitizado),huesos
                std::printf(" %.1f,%.1f,%.1f,%.1f,%d,%d,%.1f,%.1f,%s,",
                            box.x, box.y, box.w, box.h,
                            targets[index].teammate ? 1 : 0,
                            targets[index].team,
                            targets[index].health,
                            targets[index].distance_m,
                            SanitizeName(targets[index].name).c_str());
                // huesos proyectados: sx:sy;sx:sy;... (10º campo, ':' para x/y)
                for (int b = 0; b < targets[index].bone_count; ++b) {
                    const Vec3& bcm = targets[index].bones[b];
                    const dc::Vec3 canonical{bcm.y * 0.01f, bcm.z * 0.01f, -bcm.x * 0.01f};
                    dc::Vec3 ndc{};
                    if (!dc::ProjectPoint(matrix, canonical, ndc)) continue;
                    const float sx = (0.5f + ndc.x * 0.5f) * g_screen_width;
                    const float sy = (0.5f - ndc.y * 0.5f) * g_screen_height;
                    if (b) std::printf(";");
                    std::printf("%.1f:%.1f", sx, sy);
                }
                ++count;
            }
            std::printf(" %d\n", count);
            std::fflush(stdout);
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
        }
        return 0;
    }

    if (g_mode == "chain") {
        std::printf("[*] estado=%s score=%d\n", StateName(reader.CurrentState()),
                    WorldScore(*raw_driver, reader.World(), reader.Module()));
        DumpChain(*raw_driver, reader.World());
        return 0;
    }

    if (g_mode == "team") {
        std::printf("[*] estado=%s\n", StateName(reader.CurrentState()));
        DumpTeam(*raw_driver, reader.World());
        return 0;
    }

    if (g_mode == "scan") {
        std::printf("[*] estado=%s\n", StateName(reader.CurrentState()));
        ScanActors(*raw_driver, reader.World(), 40);
        return 0;
    }

    if (g_mode == "teams") {
        std::printf("[*] estado=%s\n", StateName(reader.CurrentState()));
        ScanTeams(*raw_driver, reader.World(), 80);
        return 0;
    }

    if (g_mode == "capsule") {
        std::printf("[*] estado=%s\n", StateName(reader.CurrentState()));
        DumpCapsule(*raw_driver, reader.World());
        return 0;
    }

    if (g_mode == "bones") {
        std::printf("[*] estado=%s\n", StateName(reader.CurrentState()));
        dc::FrameSnapshot frame;
        reader.ReadLatest(frame);   // poblar Targets()
        // Escanear el primer objetivo con mesh válido (enemigos vivos).
        uintptr_t target_actor = 0;
        for (const Target& t : reader.Targets()) {
            const uintptr_t m = ReadPtr(*raw_driver, t.actor + off::MESH);
            if (PlausiblePointer(m)) { target_actor = t.actor; break; }
        }
        if (target_actor) DumpBones(*raw_driver, target_actor);
        else std::printf("bones: sin objetivo con mesh válido\n");
        return 0;
    }

    if (g_mode == "probe") {
        const uintptr_t base = raw_driver->module_base(options.module.c_str());
        std::printf("[%c] %s @ %s\n", base ? '+' : '-', options.module.c_str(),
                    Hex(base).c_str());
        std::printf("[*] pid=%d estado=%s variable=%s world=%s\n",
                    static_cast<int>(reader.ProcessId()),
                    StateName(reader.CurrentState()),
                    Hex(reader.WorldVariable()).c_str(), Hex(reader.World()).c_str());
        std::printf("[*] %s\n", reader.StatusText().c_str());
        (void)started;
        return reader.WorldVariable() ? 0 : 1;
    }

    // En el lobby el GWorld todavía no existe: se reintenta hasta entrar.
    // Si el arranque falló (driver, PID o GWorld), reintentar hasta entrar.
    while (g_running && started != dc::Status::Ok) {
        std::printf("[-] %s — reintentando en 3 s (entra a una partida)\n",
                    reader.StatusText().c_str());
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (!g_running) return 1;
        started = reader.Start();
    }
    if (!g_running) return 1;

    if (!g_quiet) {
        const uintptr_t base = raw_driver->module_base(options.module.c_str());
        std::printf("[+] PID juego: %d\n", static_cast<int>(reader.ProcessId()));
        std::printf("[%c] %s @ %s\n", base ? '+' : '-', options.module.c_str(),
                    Hex(base).c_str());
        std::printf("[+] GWorld @ %s (variable), world %s\n",
                    Hex(reader.WorldVariable()).c_str(), Hex(reader.World()).c_str());
    }

    dc::FrameSnapshot frame;
    std::vector<ScreenBox> boxes;
    int tick = 0;
    int failures = 0;

    while (g_running && (g_mode == "loop" || tick < g_ticks)) {
        ++tick;
        const dc::Status status = reader.ReadLatest(frame);
        if (status != dc::Status::Ok) {
            ++failures;
            if (!g_quiet) {
                std::printf("[*] tick %d: %s (%s)\n", tick, reader.StatusText().c_str(),
                            dc::StatusName(status));
            }
            if (status == dc::Status::Disconnected) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                reader.Start();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
            continue;
        }

        const CameraState& camera = reader.Camera();
        std::printf("CAM: loc=%.0f,%.0f,%.0f rot=%.0f,%.0f,%.0f fov=%.0f\n",
                    camera.location.x, camera.location.y, camera.location.z,
                    camera.rotation.pitch, camera.rotation.yaw, camera.rotation.roll,
                    camera.fov_degrees);

        // Self-test: el punto a 10 m delante de la cámara debe caer en el centro.
        {
            const float aspect = g_screen_width / g_screen_height;
            dc::CameraFrame view = frame.camera;
            const float hfov = reader.HorizontalFovRadians();
            if (hfov > 0.f) {
                const float vfov = VerticalFovFromHorizontal(hfov, aspect);
                if (vfov > 0.02f && vfov < 3.05f) view.vertical_fov_radians = vfov;
            }
            const dc::Mat4 matrix = dc::ViewProjection(view, aspect);
            const dc::Vec3 ahead = view.position + Unit(view.target - view.position) * 10.f;
            dc::Vec3 ndc{};
            if (dc::ProjectPoint(matrix, ahead, ndc)) {
                std::printf("SELFTEST centro=(%.0f,%.0f) de (%.0f,%.0f)\n",
                            (0.5f + ndc.x * 0.5f) * g_screen_width,
                            (0.5f - ndc.y * 0.5f) * g_screen_height, g_screen_width,
                            g_screen_height);
            } else {
                std::printf("SELFTEST falló: la cámara no proyecta su propio frente\n");
            }
        }

        boxes.clear();
        for (size_t index = 0; index < frame.entities.size(); ++index) {
            boxes.push_back(ProjectEntity(frame, reader.HorizontalFovRadians(), index,
                                          g_screen_width, g_screen_height));
        }

        const std::vector<Target>& targets = reader.Targets();
        std::printf("---- %zu targets ----\n", targets.size());
        for (size_t index = 0; index < targets.size(); ++index) {
            const Target& target = targets[index];
            const ScreenBox box = boxes[index];
            if (box.visible && static_cast<int>(index) < g_max_boxes) {
                std::printf("BOX x=%.0f y=%.0f w=%.0f h=%.0f | %dm | HP=%d | T%d | %s%s\n",
                            box.x, box.y, box.w, box.h,
                            static_cast<int>(target.distance_m),
                            static_cast<int>(target.health), target.team,
                            target.name.empty() ? "?" : target.name.c_str(),
                            target.encrypted ? " [ENC!]" : "");
            }
            if (g_mode == "dump" || g_raw) {
                const uintptr_t mesh = ReadPtr(*raw_driver, target.actor + off::MESH);
                const uintptr_t vtable = ReadPtr(*raw_driver, mesh);
                const bool mesh_ok = PlausiblePointer(mesh) &&
                                      vtable >= reader.Module().start && vtable < reader.Module().end;
                std::printf("  [%zu] actor=%s d=%.1fm T%d HP=%.0f pos=(%.0f,%.0f,%.0f) "
                            "box=(x%.0f y%.0f w%.0f h%.0f) mesh=%s %s%s\n",
                            index, Hex(target.actor).c_str(), target.distance_m, target.team,
                            target.health, target.feet_cm.x, target.feet_cm.y,
                            target.feet_cm.z, box.x, box.y, box.w, box.h,
                            mesh_ok ? Hex(mesh).c_str() : "-", 
                            box.visible ? "VISIBLE" : "fuera",
                            target.name.empty() ? "" : (" " + target.name).c_str());
            }
            if (g_mode == "dump" || g_raw) {
                const uintptr_t root = ReadPtr(*raw_driver, target.actor + off::ROOT_COMPONENT);
                FEncVector vector{};
                if (root && Read(*raw_driver, root + off::REL_LOCATION, vector)) {
                    uint32_t xb = 0, yb = 0, zb = 0;
                    std::memcpy(&xb, &vector.x, 4);
                    std::memcpy(&yb, &vector.y, 4);
                    std::memcpy(&zb, &vector.z, 4);
                    std::printf("  actor=%s raw=(%08x %08x %08x) index=%u enc=%d box=%s\n",
                                Hex(target.actor).c_str(), xb, yb, zb,
                                static_cast<unsigned>(vector.index),
                                static_cast<int>(vector.encrypted),
                                box.visible ? "visible" : "fuera");
                }
            }
        }

        const Stats& stats = reader.Statistics();
        std::printf("== STATS: self_team=%d pawn=%s | cifrados %d/%zu | aceptados %d | vistos %d | lejos %d | "
                    "equipo %d | sin root %d | ilegibles %d | dup %d ==\n",
                    reader.SelfTeam(), Hex(reader.SelfPawn()).c_str(),
                    stats.encrypted, targets.size(), stats.accepted, stats.actors_considered,
                    stats.rejected_distance, stats.teammates, stats.rejected_pointer,
                    stats.unreadable, stats.duplicates);

        std::this_thread::sleep_for(std::chrono::milliseconds(interval_ms));
    }

    reader.Stop();
    std::printf("=== fin (%d ticks, %d fallos) ===\n", tick, failures);
    return 0;
}
