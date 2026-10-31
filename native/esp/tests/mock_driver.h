#pragma once
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "driver.h"

namespace dc::esp {

/*
 * Memoria simulada por páginas de 4 KB. Imita al driver:
 *   - una página no escrita cuenta como no mapeada;
 *   - al cruzar un hueco la lectura devuelve lo copiado hasta ahí.
 * Así las pruebas de scanner/reader ejercitan el mismo camino que el .ko.
 */
class MockDriver final : public IMemoryDriver {
public:
    static constexpr size_t kPageSize = 0x1000;

    MockDriver() = default;
    explicit MockDriver(pid_t pid) : pid_(pid) {}

    const char* name() const override { return "mock"; }
    bool open(std::string* error) override {
        opend_ = true;
        if (error) error->clear();
        return true;
    }
    void close() override { opend_ = false; }
    bool is_open() const override { return opend_; }
    pid_t pid() const override { return pid_; }
    void set_pid(pid_t value) override { pid_ = value; }
    int last_errno() const override { return 0; }

    size_t read(uintptr_t address, void* buffer, size_t size) override {
        ++reads;
        auto* out = static_cast<unsigned char*>(buffer);
        size_t done = 0;
        while (done < size) {
            const uintptr_t current = address + done;
            const uintptr_t page = current & ~uintptr_t(kPageSize - 1);
            auto it = pages_.find(page);
            if (it == pages_.end()) break;
            const size_t offset = static_cast<size_t>(current - page);
            const size_t chunk = std::min(size - done, kPageSize - offset);
            std::memcpy(out + done, it->second.data() + offset, chunk);
            done += chunk;
        }
        if (done < size) ++short_reads;
        return done;
    }

    bool write(uintptr_t address, const void* buffer, size_t size) override {
        write_bytes(address, buffer, size);
        return true;
    }

    uintptr_t module_base(const char* module) override {
        auto it = modules_.find(module ? module : "");
        return it == modules_.end() ? 0 : it->second;
    }

    bool module_data_range(const char* module, AddressRange& out) override {
        auto it = ranges_.find(module ? module : "");
        if (it == ranges_.end()) return false;
        out = it->second;
        return true;
    }

    /* ── utilidades de prueba ── */
    void write_bytes(uintptr_t address, const void* data, size_t size) {
        const auto* in = static_cast<const unsigned char*>(data);
        size_t done = 0;
        while (done < size) {
            const uintptr_t current = address + done;
            const uintptr_t page = current & ~uintptr_t(kPageSize - 1);
            const size_t offset = static_cast<size_t>(current - page);
            const size_t chunk = std::min(size - done, kPageSize - offset);
            auto& storage = pages_[page];
            if (storage.empty()) storage.assign(kPageSize, 0);
            std::memcpy(storage.data() + offset, in + done, chunk);
            done += chunk;
        }
    }

    template <typename T>
    void put(uintptr_t address, const T& value) {
        write_bytes(address, &value, sizeof(T));
    }

    void put_ptr(uintptr_t address, uintptr_t value) { put(address, value); }
    void put_i32(uintptr_t address, int32_t value) { put(address, value); }
    void put_u32(uintptr_t address, uint32_t value) { put(address, value); }
    void put_float(uintptr_t address, float value) { put(address, value); }

    void put_fstring(uintptr_t address, uintptr_t data, int32_t characters) {
        put(address, data);
        put(address + 8, characters);
        put(address + 12, characters);
    }

    void register_module(const std::string& module, uintptr_t base, AddressRange data) {
        modules_[module] = base;
        ranges_[module] = data;
    }

    size_t reads = 0;
    size_t short_reads = 0;

private:
    std::map<uintptr_t, std::vector<unsigned char>> pages_;
    std::map<std::string, uintptr_t> modules_;
    std::map<std::string, AddressRange> ranges_;
    pid_t pid_{-1};
    bool opend_{false};
};

/* Mapa de proceso falso: devuelve los rangos registrados. */
class MockProcessMap final : public IProcessMap {
public:
    std::vector<AddressRange> ranges;
    bool writable_module_ranges(pid_t, const char*,
                                std::vector<AddressRange>& out) override {
        out = ranges;
        return !out.empty();
    }
};

} // namespace dc::esp
