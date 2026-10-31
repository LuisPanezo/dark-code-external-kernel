#pragma once
#include <memory>
#include <string>
#include <vector>

#include "types.h"

namespace dc::esp {

/*
 * Acceso a memoria de otro proceso. `read` puede devolver menos bytes de los
 * pedidos (página sin mapear); el llamador decide si le sirve el parcial.
 * Sin ioctl en el hilo gráfico: el servicio (api/esp_service.h) lo usa desde
 * su propio hilo.
 */
class IMemoryDriver {
public:
    virtual ~IMemoryDriver() = default;

    virtual const char* name() const = 0;
    virtual bool  open(std::string* error) = 0;
    virtual void  close() = 0;
    virtual bool  is_open() const = 0;
    virtual pid_t pid() const = 0;
    virtual void  set_pid(pid_t pid) = 0;

    virtual size_t read(uintptr_t address, void* buffer, size_t size) = 0;
    virtual bool   write(uintptr_t address, const void* buffer, size_t size) = 0;

    virtual uintptr_t module_base(const char* module) = 0;
    virtual bool module_data_range(const char* module, AddressRange& out) = 0;

    virtual int last_errno() const = 0;
};

/* Mappings del proceso objetivo (en Android: /proc/<pid>/maps). */
class IProcessMap {
public:
    virtual ~IProcessMap() = default;
    virtual bool writable_module_ranges(pid_t pid, const char* module,
                                        std::vector<AddressRange>& out) = 0;
};

/* ── Helpers tipados: exigen lectura COMPLETA ── */
template <typename T>
bool Read(IMemoryDriver& driver, uintptr_t address, T& out) {
    return driver.read(address, &out, sizeof(T)) == sizeof(T);
}

template <typename T>
bool Write(IMemoryDriver& driver, uintptr_t address, const T& value) {
    return driver.write(address, &value, sizeof(T));
}

uintptr_t   ReadPtr(IMemoryDriver& driver, uintptr_t address);
bool        ReadFull(IMemoryDriver& driver, uintptr_t address, void* buffer, size_t size);
std::string DescribeErrno(int error);

/* ── Implementaciones reales (Linux/Android) ── */
std::unique_ptr<IMemoryDriver> MakePhmeopDriver();
std::unique_ptr<IProcessMap>   MakeLinuxProcessMap();

/* Busca el PID cuyo cmdline contiene `needle` (p. ej. el paquete del juego).
 * Si se pasa `module`, prefiere un proceso que tenga ese .so mapeado. */
pid_t FindProcessId(const char* needle, const char* module = nullptr);

} // namespace dc::esp
