#include "elf.h"

#include <algorithm>
#include <cstring>

namespace dc::esp {
namespace {

/* Cabeceras ELF64 copiadas literalmente: nada de <elf.h> para no depender de
 * la libc del host ni del NDK. */
struct Elf64Header {
    unsigned char ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t program_offset;
    uint64_t section_offset;
    uint32_t flags;
    uint16_t header_size;
    uint16_t program_entry_size;
    uint16_t program_count;
    uint16_t section_entry_size;
    uint16_t section_count;
    uint16_t section_names;
};
static_assert(sizeof(Elf64Header) == 64, "Elf64Header debe medir 64 bytes");

struct Elf64Program {
    uint32_t type;
    uint32_t flags;
    uint64_t offset;
    uint64_t vaddr;
    uint64_t paddr;
    uint64_t file_size;
    uint64_t memory_size;
    uint64_t alignment;
};
static_assert(sizeof(Elf64Program) == 56, "Elf64Program debe medir 56 bytes");

constexpr uint32_t kProgramLoad = 1;
constexpr uint32_t kFlagWrite = 2;
constexpr uint16_t kMaxPrograms = 128;
constexpr uint64_t kMaxSegmentBytes = 1024ULL * 1024ULL * 1024ULL;  // 1 GB

} // namespace

ModuleSegments ReadModuleSegments(IMemoryDriver& driver, uintptr_t module_base) {
    ModuleSegments segments;
    if (!module_base) return segments;

    Elf64Header header{};
    if (driver.read(module_base, &header, sizeof(header)) != sizeof(header)) return segments;
    if (header.ident[0] != 0x7F || header.ident[1] != 'E' || header.ident[2] != 'L' ||
        header.ident[3] != 'F') return segments;
    if (header.ident[4] != 2 || header.ident[5] != 1) return segments;   // 64 bits, LE
    if (header.program_count == 0 || header.program_count > kMaxPrograms) return segments;
    if (header.program_entry_size < sizeof(Elf64Program)) return segments;

    std::vector<Elf64Program> programs(header.program_count);
    const size_t bytes = size_t(header.program_count) * header.program_entry_size;
    std::vector<unsigned char> raw(bytes);
    if (driver.read(module_base + header.program_offset, raw.data(), bytes) != bytes) {
        return segments;
    }
    for (uint16_t index = 0; index < header.program_count; ++index) {
        std::memcpy(&programs[index], raw.data() + size_t(index) * header.program_entry_size,
                    sizeof(Elf64Program));
    }

    uint64_t lowest = UINT64_MAX;
    uint64_t highest = 0;
    for (const auto& program : programs) {
        if (program.type != kProgramLoad || program.memory_size == 0) continue;
        lowest = std::min(lowest, program.vaddr);
        highest = std::max(highest, program.vaddr + program.memory_size);
    }
    if (lowest == UINT64_MAX) return segments;

    // Sesgo: module_base es el mapping del PT_LOAD con vaddr más bajo.
    const uintptr_t bias = module_base - static_cast<uintptr_t>(lowest);
    segments.base = bias + static_cast<uintptr_t>(lowest);
    segments.end = bias + static_cast<uintptr_t>(highest);

    for (const auto& program : programs) {
        if (program.type != kProgramLoad) continue;
        if (!(program.flags & kFlagWrite)) continue;
        if (program.memory_size == 0 || program.memory_size > kMaxSegmentBytes) continue;
        AddressRange range;
        range.start = bias + static_cast<uintptr_t>(program.vaddr);
        range.end = range.start + static_cast<uintptr_t>(program.memory_size);
        if (range.end <= range.start) continue;
        segments.writable.push_back(range);
    }

    std::sort(segments.writable.begin(), segments.writable.end(),
              [](const AddressRange& a, const AddressRange& b) { return a.start < b.start; });
    segments.valid = !segments.writable.empty() && segments.end > segments.base;
    return segments;
}

} // namespace dc::esp
