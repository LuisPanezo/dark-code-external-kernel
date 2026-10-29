#include "frame_runtime.h"
#include "sources/mock_source.h"
#include "sources/replay_source.h"
namespace dc {
static std::unique_ptr<IFrameSource> MakeSource(int mode) {
    if(mode==3) {
        std::vector<FrameSnapshot> frames;frames.reserve(90);
        for(int i=0;i<90;++i) frames.push_back(SampleScene(i/30.f,Coordinates::CanonicalMeters));
        return std::make_unique<ReplaySource>(std::move(frames));
    }
    Coordinates c=mode==1?Coordinates::UnityWorldMeters:mode==2?Coordinates::UnrealWorldCentimeters:Coordinates::CanonicalMeters;
    return std::make_unique<MockSource>(c);
}
void FrameRuntime::Start() {
    if(running_.exchange(true)) return;
    {std::lock_guard<std::mutex> lock(mutex_);latest_={nullptr,Status::NoData};}
    try {worker_=std::thread(&FrameRuntime::Loop,this);}
    catch(...) {running_=false;throw;}
}
void FrameRuntime::Stop() {
    running_=false;wake_.notify_all();
    if(worker_.joinable()) worker_.join();
    std::lock_guard<std::mutex> lock(mutex_);latest_={nullptr,Status::Stopped};
}
void FrameRuntime::SetMode(int mode) {
    if(mode<0||mode>3||mode_.exchange(mode)==mode) return;
    {std::lock_guard<std::mutex> lock(mutex_);latest_={nullptr,Status::NoData};}
    wake_.notify_all();
}
LatestFrame FrameRuntime::Latest() const {
    std::lock_guard<std::mutex> lock(mutex_);auto copy=latest_;
    if(copy.frame) {
        if(paused_) copy.status=Status::Paused;
        else if(!IsFresh(*copy.frame,MonotonicNs())) {copy.frame.reset();copy.status=Status::Stale;}
    } return copy;
}
void FrameRuntime::Loop() {
    std::unique_ptr<IFrameSource> source;int active=-1;uint64_t sequence=0;
    Status source_status=Status::NoData;
    while(running_) {
        int desired=mode_.load();
        if(desired!=active) {
            if(source) source->Stop();
            active=desired;
            try {source=factory_?factory_(desired):MakeSource(desired);source_status=source?source->Start():Status::Disconnected;}
            catch(...) {source.reset();source_status=Status::Disconnected;}
            std::lock_guard<std::mutex> lock(mutex_);latest_={nullptr,source_status};
        }
        bool need_frame;
        {std::lock_guard<std::mutex> lock(mutex_);need_frame=!paused_||!latest_.frame;}
        if(need_frame) {
            FrameSnapshot frame;Status status=source_status;
            if(status==Status::Ok) {
                try {status=source->ReadLatest(frame);}catch(...) {status=Status::Disconnected;}
            }
            if(status==Status::Ok&&!Validate(frame)) status=Status::InvalidFrame;
            if(status==Status::Ok&&!IsFresh(frame,MonotonicNs())) status=Status::Stale;
            std::lock_guard<std::mutex> lock(mutex_);
            if(desired==mode_) {
                if(status==Status::Ok) {frame.sequence=++sequence;latest_={std::make_shared<const FrameSnapshot>(std::move(frame)),status};}
                else latest_={nullptr,status};
            }
        }
        std::unique_lock<std::mutex> lock(mutex_);
        wake_.wait_for(lock,std::chrono::milliseconds(33),[&] {return !running_||mode_!=active;});
    }
    if(source) source->Stop();
}
}
