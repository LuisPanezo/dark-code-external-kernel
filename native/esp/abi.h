#pragma once
/*
 * ABI compartido con el driver de kernel.
 *
 * No se replica el contrato: se incluye kernel/phmeop.h tal cual. Así el
 * cliente y el .ko no pueden divergir (structs, orden de campos y opcodes).
 * Los static_assert de abajo fijan el caso soportado: cliente de 64 bits
 * (arm64-v8a / x86_64) sobre kernel de 64 bits.
 */
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#include "../../kernel/phmeop.h"

#define DC_ESP_DEVICE "/dev/phmeop"

namespace dc::esp {

/* ioctl(2) recibe un unsigned long, no un int. */
constexpr unsigned long kOpReadMem      = static_cast<unsigned long>(OP_READ_MEM);
constexpr unsigned long kOpWriteMem     = static_cast<unsigned long>(OP_WRITE_MEM);
constexpr unsigned long kOpModuleBase   = static_cast<unsigned long>(OP_MODULE_BASE);
constexpr unsigned long kOpModuleRange  = static_cast<unsigned long>(OP_MODULE_DATA_RANGE);

static_assert(sizeof(void*) == 8, "el cliente debe ser LP64 (arm64-v8a o x86_64)");
static_assert(sizeof(pid_t) == 4, "pid_t inesperado");
static_assert(offsetof(struct CopyMemory, pid)    ==  0, "ABI CopyMemory.pid");
static_assert(offsetof(struct CopyMemory, addr)   ==  8, "ABI CopyMemory.addr");
static_assert(offsetof(struct CopyMemory, buffer) == 16, "ABI CopyMemory.buffer");
static_assert(offsetof(struct CopyMemory, size)   == 24, "ABI CopyMemory.size");
static_assert(sizeof(struct CopyMemory)           == 32, "ABI sizeof(CopyMemory)");
static_assert(offsetof(struct ModuleBase, base)   == 16, "ABI ModuleBase.base");
static_assert(sizeof(struct ModuleBase)           == 24, "ABI sizeof(ModuleBase)");
static_assert(offsetof(struct ModuleRange, start) == 16, "ABI ModuleRange.start");
static_assert(offsetof(struct ModuleRange, end)   == 24, "ABI ModuleRange.end");
static_assert(sizeof(struct ModuleRange)          == 32, "ABI sizeof(ModuleRange)");

} // namespace dc::esp
