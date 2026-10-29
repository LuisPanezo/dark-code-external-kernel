#pragma once
#include "sources/frame_source.h"
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>
#include <functional>
namespace dc {
enum class DemoMode { Custom, Unity, Unreal, Replay };
struct LatestFrame { std::shared_ptr<const FrameSnapshot> frame; Status status{Status::NoData}; };
class FrameRuntime {
public:
    using SourceFactory=std::function<std::unique_ptr<IFrameSource>(int)>;
private:
    SourceFactory factory_;
    std::atomic<bool> running_{false},paused_{false};
    std::atomic<int> mode_{0};std::thread worker_;
    mutable std::mutex mutex_;std::condition_variable wake_;
    LatestFrame latest_;
    void Loop();
public:
    explicit FrameRuntime(SourceFactory factory={}):factory_(std::move(factory)) {}
    ~FrameRuntime() {Stop();}
    void Start();void Stop();
    void SetMode(int mode);
    void SetPaused(bool paused) {if(paused_.exchange(paused)!=paused)wake_.notify_all();}
    LatestFrame Latest() const;
};
}
