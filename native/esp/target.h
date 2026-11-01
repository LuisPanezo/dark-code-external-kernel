#pragma once
#include <cstdint>
#include <string>

namespace dc::esp {

/*
 * Objetivo canónico del ESP (agnóstico de motor). El proyector, el servicio y
 * el overlay consumen SOLO esto; cada adaptador de motor (Unreal, Unity) lo
 * produce. Las distancias van en metros. La geometría (caja, posición) no está
 * aquí: vive en el dc::FrameSnapshot canónico que acompaña a esta lista.
 */
struct EspTarget {
    uintptr_t actor{};
    float     distance_m{};
    float     health{-1.f};   // -1 = sin vida legible
    int32_t   team{-1};       // -1 = sin equipo
    bool      encrypted{};
    bool      teammate{};
    std::string name;
};

/* Contadores de diagnóstico (agnósticos de motor). */
struct Stats {
    uint64_t ticks{};
    uint64_t read_calls{};
    int actors_in_level{};
    int actors_considered{};
    int accepted{};
    int rejected_pointer{};
    int rejected_position{};
    int rejected_distance{};
    int teammates{};
    int encrypted{};
    int plain{};
    int unreadable{};
    int duplicates{};
    int last_errno{};
};

} // namespace dc::esp
