#pragma once
#include "frame_source.h"
#include "core/coordinate_system.h"
namespace dc {
FrameSnapshot SampleScene(float seconds,Coordinates coordinates);
class MockSource final:public IFrameSource {
    Coordinates coordinates_;uint64_t start_{},sequence_{};bool running_{};
public:
    explicit MockSource(Coordinates c):coordinates_(c) {}
    Status Start() override;
    Status ReadLatest(FrameSnapshot& out) override;
    void Stop() noexcept override {running_=false;}
};
}
