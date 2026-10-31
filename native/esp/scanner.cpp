#include "scanner.h"

#include <algorithm>
#include <cstring>

#include "offsets.h"
#include "ue.h"

namespace dc::esp {
namespace {

struct ActorsTable { uintptr_t data; uint32_t num; uint32_t max; };

bool ReadActorsTable(IMemoryDriver& driver, uintptr_t level, ActorsTable& table) {
    table = ActorsTable{};
    if (!Read(driver, level + off::ACTORS, table)) return false;
    if (!PlausiblePointer(table.data)) return false;
    if (table.num == 0 || table.num > static_cast<uint32_t>(off::MAX_ACTORS)) return false;
    if (table.max < table.num || table.max > 65535u) return false;
    return true;
}

bool InsideSpan(const ModuleSpan& span, uintptr_t value) {
    return span.valid() && value >= span.start && value < span.end;
}

/*
 * Un UObject vivo empieza por su vtable, que vive en el propio módulo. Si el
 * span no se conoce (pruebas con memoria simulada) no se puede comprobar y se
 * deja pasar.
 */
bool LooksLikeUObject(IMemoryDriver& driver, uintptr_t object, const ModuleSpan& span) {
    if (!PlausiblePointer(object)) return false;
    if (!span.valid()) return true;
    return InsideSpan(span, ReadPtr(driver, object));
}

} // namespace

int WorldScore(IMemoryDriver& driver, uintptr_t world, const ModuleSpan& span) {
    if (!PlausiblePointer(world)) return 0;

    uintptr_t level = ReadPtr(driver, world + off::PERSISTENT_LEVEL);
    if (!LooksLikeUObject(driver, level, span)) return 0;
    ActorsTable table{};
    if (!ReadActorsTable(driver, level, table)) return 0;

    // El primer actor del nivel debe ser también un UObject real.
    const uintptr_t first_actor = ReadPtr(driver, table.data);
    if (!LooksLikeUObject(driver, first_actor, span)) return 0;

    int score = 1;

    uintptr_t game_instance = ReadPtr(driver, world + off::GAME_INSTANCE);
    if (!PlausiblePointer(game_instance)) return score;
    ++score;

    uintptr_t local_players = ReadPtr(driver, game_instance + off::LOCAL_PLAYERS);
    if (!PlausiblePointer(local_players)) return score;
    uint32_t count = 0;
    if (!Read(driver, local_players + 8, count)) return score;
    if (count == 0 || count > 8) return score;
    uintptr_t local_player = ReadPtr(driver, local_players);
    if (!PlausiblePointer(local_player)) return score;
    ++score;

    uintptr_t controller = ReadPtr(driver, local_player + off::PLAYER_CONTROLLER);
    if (!PlausiblePointer(controller)) return score;
    if (PlausiblePointer(ReadPtr(driver, controller + off::ACK_PAWN)) ||
        PlausiblePointer(ReadPtr(driver, controller + off::CAMERA_MANAGER))) {
        ++score;
    }
    return score;
}

bool LooksLikeWorld(IMemoryDriver& driver, uintptr_t world, bool* strong) {
    const int score = WorldScore(driver, world);
    if (strong) *strong = score >= 3;
    return score > 0;
}

WorldScan ScanForWorld(IMemoryDriver& driver, const std::vector<AddressRange>& ranges,
                       const ScanOptions& options) {
    WorldScan result;
    const size_t chunk = std::max<size_t>(options.chunk_bytes, 8);
    std::vector<unsigned char> buffer(chunk);

    for (const auto& range : ranges) {
        const size_t length = std::min(range.size(), options.max_range_bytes);
        size_t offset = 0;
        while (offset + 8 <= length) {
            const size_t want = std::min(chunk, length - offset);
            const uintptr_t base = range.start + offset;
            const size_t got = driver.read(base, buffer.data(), want);
            result.bytes_scanned += got;

            for (size_t word = 0; word * 8 + 8 <= got; ++word) {
                uintptr_t candidate = 0;
                std::memcpy(&candidate, buffer.data() + word * 8, sizeof(candidate));
                if (!PlausiblePointer(candidate)) continue;
                if (options.module_end && candidate >= options.module_start &&
                    candidate < options.module_end) {
                    continue;   // dentro del .so: no es un UWorld del heap
                }

                const int score = WorldScore(driver, candidate,
                                            ModuleSpan{options.module_start, options.module_end});
                if (score == 0) continue;
                ++result.candidates;

                // Puntuación 4: nivel + GameInstance + LocalPlayers + pawn/cámara.
                if (score > result.score) {
                    result.score = score;
                    result.variable = base + word * 8;
                    result.world = candidate;
                    result.strong = score >= 4;
                }
                if (result.strong) return result;
                if (result.candidates >= options.max_candidates) return result;
            }

            // got == 0: página sin mapear, avanzar una página (no perder el
            // resto del chunk). Parcial: seguir justo después del hueco.
            if (got == 0) {
                offset += 0x1000;
            } else if (got < want) {
                offset += (got + 7) & ~static_cast<size_t>(7);
            } else {
                offset += want;
            }
        }
    }
    return result;
}

} // namespace dc::esp
