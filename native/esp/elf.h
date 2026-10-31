#pragma once
#include <stdint.h>
#include <vector>

#include "driver.h"

namespace dc::esp {

/*
 * Segmentos de un módulo ya cargado, leídos de sus program headers ELF64.
 *
 * ¿Por qué no basta /proc/<pid>/maps? En Android el .bss de una .so suele
 * quedar como mapping ANÓNIMO (sin ruta), así que no aparece al filtrar por
 * nombre de módulo. Aquí sale del propio ELF: PT_LOAD con permiso de
 * escritura, incluyendo la parte que excede el tamaño del archivo.
 */
struct ModuleSegments {
    uintptr_t base{};            // sesgo de carga (dirección del primer PT_LOAD)
    uintptr_t end{};             // fin del último PT_LOAD
    std::vector<AddressRange> writable;   // .data + .bss
    bool valid{};
};

/* `module_base` es lo que devuelve OP_MODULE_BASE (primer mapping, el más bajo). */
ModuleSegments ReadModuleSegments(IMemoryDriver& driver, uintptr_t module_base);

} // namespace dc::esp
