#pragma once

namespace dc::esp {
/* Ejecuta las comprobaciones del módulo ESP en memoria simulada.
 * Devuelve cuántas pasaron; lanza std::runtime_error en la primera que falla. */
int RunEspTests();
} // namespace dc::esp
