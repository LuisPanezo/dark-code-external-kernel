#pragma once
#include "frame_source.h"
namespace dc {
// An in-memory recording of owned telemetry; timestamps are rebased during playback.
class ReplaySource final:public IFrameSource {
    std::vector<FrameSnapshot> frames_;uint64_t start_{};uint64_t interval_;bool loop_,running_{};
public:
    explicit ReplaySource(std::vector<FrameSnapshot> frames,uint64_t interval_ns=33333333,bool loop=true)
        :frames_(std::move(frames)),interval_(interval_ns),loop_(loop) {}
    Status Start() override;
    Status ReadLatest(FrameSnapshot& out) override;
    void Stop() noexcept override {running_=false;}
};
}
