#pragma once
#include <vector>

#include "driver.h"
#include "types.h"

namespace dc::esp {

/* Rango [start, end) que ocupa el módulo ya cargado (todos sus PT_LOAD). */
struct ModuleSpan {
    uintptr_t start{};
    uintptr_t end{};
    bool valid() const { return end > start; }
};

/*
 * Puntúa un puntero como UWorld:
 *   0  no es un mundo
 *   1  nivel + TArray de actores creíble
 *   2  además OwningGameInstance plausible
 *   3  además LocalPlayers con datos
 *   4  además PlayerController con pawn o cámara  ← lo que necesita el lector
 *
 * Con `span` válido se exige además que el ULevel y el primer actor sean
 * UObject de verdad (su vtable cae dentro del módulo). Eso elimina los
 * punteros del .data que "parecen" mundos y no lo son.
 */
int WorldScore(IMemoryDriver& driver, uintptr_t world, const ModuleSpan& span = {});

/* Atajo: true si WorldScore > 0. `strong` informa si llegó a 4. */
bool LooksLikeWorld(IMemoryDriver& driver, uintptr_t world, bool* strong = nullptr);

struct WorldScan {
    uintptr_t variable{0};   // dirección del global (la variable GWorld)
    uintptr_t world{0};      // valor que tenía al encontrarlo
    bool      strong{false}; // puntuación 4
    int       score{0};      // mejor puntuación encontrada
    size_t    bytes_scanned{0};
    int       candidates{0};
};

struct ScanOptions {
    size_t chunk_bytes{0x40000};                  // 256 KB por lectura
    size_t max_range_bytes{128u * 1024u * 1024u}; // techo por rango
    int    max_candidates{4096};                  // corta escaneos patológicos
    uintptr_t module_start{};                     // un UWorld está en el heap,
    uintptr_t module_end{};                       // no dentro del propio .so
};

/* Escanea los rangos RW y devuelve la MEJOR variable que apunta a un mundo
 * (no la primera: en el .data hay punteros que lo parecen y no lo son). */
WorldScan ScanForWorld(IMemoryDriver& driver, const std::vector<AddressRange>& ranges,
                       const ScanOptions& options = {});

} // namespace dc::esp
