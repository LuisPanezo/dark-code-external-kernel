#pragma once
#include <sys/types.h>
#include <stddef.h>
#include <cstdint>
#include <string>

#include "core/frame_snapshot.h"   // dc::Vec3

namespace dc::esp {

using Vec3 = dc::Vec3;

struct AddressRange {
    uintptr_t start{}, end{};
    size_t size() const { return end > start ? static_cast<size_t>(end - start) : 0; }
};

/* Estado agnóstico del lector (NoWorld = "nivel/escena del juego no encontrado"). */
enum class State { Detached, Starting, NoPid, NoDriver, NoWorld, NoCamera, Live, Error };

const char* StateName(State state);

} // namespace dc::esp
