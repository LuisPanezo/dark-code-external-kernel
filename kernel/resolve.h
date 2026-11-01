#ifndef PHMEOP_RESOLVE_H
#define PHMEOP_RESOLVE_H

/*
 * phmeop: resolución en runtime de símbolos del kernel no exportados en GKI.
 *
 * GKI (Generic Kernel Image) solo exporta a los módulos externos la lista KMI
 * (android/abi_gki_aarch64_*). Símbolos "internos" de mm/sched/fs como
 * get_user_pages_remote, get_task_mm, d_path, etc. NO están en esa lista, así
 * que un módulo que los referencie directamente falla en el insmod con
 * "Unknown symbol".
 *
 * phmeop_resolve_symbol() devuelve la dirección (unsigned long) de un símbolo
 * por su nombre, o 0 si no lo encuentra. Las llamadas deben hacerse a través
 * de un puntero de función con el prototipo EXACTO del kernel, porque con
 * CONFIG_CFI_CLANG (KCFI) estricto llamar a través de un tipo incorrecto
 * dispara un panic.
 */

unsigned long phmeop_resolve_symbol(const char *name);

#endif /* PHMEOP_RESOLVE_H */
