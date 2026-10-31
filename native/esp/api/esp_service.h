#pragma once
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "api/esp_api.h"
#include "driver.h"
#include "reader.h"
#include "types.h"

namespace dc::esp {

/* Frame publicado: snapshot canónico (metros) + objetivos en el mismo orden. */
struct EspFrame {
    dc::FrameSnapshot   snapshot;
    std::vector<Target> targets;
    float               horizontal_fov_radians{};
    uint64_t            tick{};
};

/*
 * Dueño único del lector. Un solo hilo adquiere memoria; el hilo de dibujo
 * (ImGui) sólo lee el último frame publicado y proyecta, sin ioctl.
 *
 * Es un singleton porque el driver admite un único cliente por dispositivo:
 * dos instancias se pelearían por /dev/phmeop (EBUSY).
 */
class EspService {
public:
    static EspService& Instance();
    ~EspService();

    int  Attach(const Options& options, std::unique_ptr<IMemoryDriver> driver = {},
                std::unique_ptr<IProcessMap> map = {});
    void Detach();

    bool SetEnabled(bool enabled);
    bool Enabled() const;
    bool Attached() const;

    std::shared_ptr<const EspFrame> Latest() const;
    int  Poll(float width, float height, DcEspBox* out, int capacity);
    int  CopyView(DcEspView* view) const;

    State       CurrentState() const;
    std::string Status() const;
    std::string LastError() const;
    int         TargetCount() const;
    void        SetAspect(float aspect);

private:
    EspService() = default;

    void Loop();
    void Publish(std::shared_ptr<const EspFrame> frame);
    void PublishStatus(State state, const std::string& text);

    mutable std::mutex      mutex_;
    std::condition_variable wake_;
    std::thread             worker_;

    std::shared_ptr<const EspFrame> latest_;
    std::unique_ptr<EspReader>      reader_;
    Options                         options_;

    bool              running_{false};
    bool              attached_{false};
    std::atomic<bool> enabled_{true};
    State             state_{State::Detached};
    std::string       status_{"Desconectado"};
    std::string       error_;
    float             aspect_{16.f / 9.f};
};

} // namespace dc::esp
