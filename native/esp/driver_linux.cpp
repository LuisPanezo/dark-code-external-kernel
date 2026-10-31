#include "driver.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#if defined(__linux__)
#  include <fcntl.h>
#  include <sys/ioctl.h>
#  include <unistd.h>
#  include "abi.h"
#endif

/* ENOTTY no existe en MSVC; el archivo se compila también en escritorio. */
#ifndef ENOTTY
#  define ENOTTY 25
#endif

namespace dc::esp {

uintptr_t ReadPtr(IMemoryDriver& driver, uintptr_t address) {
    uintptr_t value = 0;
    return driver.read(address, &value, sizeof(value)) == sizeof(value) ? value : 0;
}

bool ReadFull(IMemoryDriver& driver, uintptr_t address, void* buffer, size_t size) {
    return size > 0 && driver.read(address, buffer, size) == size;
}

std::string DescribeErrno(int error) {
    switch (error) {
    case 0:      return "sin error";
    case EPERM:  return "EPERM: /dev/phmeop exige root (CAP_SYS_ADMIN)";
    case EACCES: return "EACCES: sin permiso sobre /dev/phmeop";
    case ENOENT: return "ENOENT: el driver no está cargado (insmod phmeop.ko)";
    case ENODEV: return "ENODEV: no existe /dev/phmeop";
    case EBUSY:  return "EBUSY: otro cliente ya tiene /dev/phmeop abierto";
    case ESRCH:  return "ESRCH: ese PID ya no existe";
    case EFAULT: return "EFAULT: dirección no mapeada o proceso sin MM";
    case EINVAL: return "EINVAL: argumento inválido";
    case ENOTTY: return "ENOTTY: ioctl no soportado por el driver";
    case EBADF:  return "EBADF: descriptor inválido";
    default:     return "errno " + std::to_string(error);
    }
}

#if defined(__linux__)

namespace {

/*
 * Cliente del driver phmeop. Un solo cliente por dispositivo (regla del .ko):
 * si otro proceso lo tiene abierto, open() devuelve EBUSY.
 */
class PhmeopDriver final : public IMemoryDriver {
public:
    ~PhmeopDriver() override { close(); }

    const char* name() const override { return "phmeop"; }
    pid_t pid() const override { return pid_; }
    void set_pid(pid_t pid) override { pid_ = pid; }
    bool is_open() const override { return fd_ >= 0; }
    int last_errno() const override { return error_; }

    bool open(std::string* error) override {
        if (fd_ >= 0) {
            if (error) *error = "el dispositivo ya está abierto";
            return true;
        }
        fd_ = ::open(DC_ESP_DEVICE, O_RDWR | O_CLOEXEC);
        if (fd_ < 0) {
            error_ = errno;
            if (error) *error = DescribeErrno(error_);
            return false;
        }
        error_ = 0;
        return true;
    }

    void close() override {
        if (fd_ >= 0) ::close(fd_);
        fd_ = -1;
    }

    size_t read(uintptr_t address, void* buffer, size_t size) override {
        if (fd_ < 0 || pid_ <= 0 || !buffer || size == 0) return 0;
        struct CopyMemory request { pid_, address, buffer, size };
        long result = ::ioctl(fd_, kOpReadMem, &request);
        if (result < 0) {
            error_ = errno;
            return 0;
        }
        error_ = 0;
        return static_cast<size_t>(result);
    }

    bool write(uintptr_t address, const void* buffer, size_t size) override {
        if (fd_ < 0 || pid_ <= 0 || !buffer || size == 0) return false;
        struct CopyMemory request { pid_, address, const_cast<void*>(buffer), size };
        long result = ::ioctl(fd_, kOpWriteMem, &request);
        if (result < 0) {
            error_ = errno;
            return false;
        }
        error_ = 0;
        return static_cast<size_t>(result) == size;
    }

    uintptr_t module_base(const char* module) override {
        if (fd_ < 0 || pid_ <= 0 || !module) return 0;
        char name[256]{};
        std::snprintf(name, sizeof(name), "%s", module);
        struct ModuleBase request { pid_, name, 0 };
        long result = ::ioctl(fd_, kOpModuleBase, &request);
        if (result < 0) {
            error_ = errno;
            return 0;
        }
        error_ = 0;
        return request.base;
    }

    bool module_data_range(const char* module, AddressRange& out) override {
        if (fd_ < 0 || pid_ <= 0 || !module) return false;
        char name[256]{};
        std::snprintf(name, sizeof(name), "%s", module);
        struct ModuleRange request { pid_, name, 0, 0 };
        long result = ::ioctl(fd_, kOpModuleRange, &request);
        if (result < 0) {
            error_ = errno;
            return false;
        }
        error_ = 0;
        out.start = request.start;
        out.end = request.end;
        return out.end > out.start;
    }

private:
    int   fd_{-1};
    pid_t pid_{-1};
    int   error_{0};
};

} // namespace

std::unique_ptr<IMemoryDriver> MakePhmeopDriver() {
    return std::make_unique<PhmeopDriver>();
}

#else

namespace {

class UnsupportedDriver final : public IMemoryDriver {
public:
    const char* name() const override { return "ninguno"; }
    pid_t pid() const override { return pid_; }
    void set_pid(pid_t pid) override { pid_ = pid; }
    bool is_open() const override { return false; }
    int last_errno() const override { return ENOSYS; }
    bool open(std::string* error) override {
        if (error) *error = "el driver phmeop solo existe en Linux/Android";
        return false;
    }
    void close() override {}
    size_t read(uintptr_t, void*, size_t) override { return 0; }
    bool write(uintptr_t, const void*, size_t) override { return false; }
    uintptr_t module_base(const char*) override { return 0; }
    bool module_data_range(const char*, AddressRange&) override { return false; }

private:
    pid_t pid_{-1};
};

} // namespace

std::unique_ptr<IMemoryDriver> MakePhmeopDriver() {
    return std::make_unique<UnsupportedDriver>();
}

#endif

} // namespace dc::esp
