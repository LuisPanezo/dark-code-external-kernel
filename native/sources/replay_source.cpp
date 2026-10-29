#include "replay_source.h"
namespace dc {
Status ReplaySource::Start() {
    running_=false;
    if(frames_.empty()||frames_.size()>10000||!interval_) return Status::InvalidFrame;
    for(const auto& frame:frames_) if(!Validate(frame)) return Status::InvalidFrame;
    start_=MonotonicNs();running_=true;return Status::Ok;
}
Status ReplaySource::ReadLatest(FrameSnapshot& out) {
    if(!running_) return Status::Stopped;
    uint64_t now=MonotonicNs(),index=(now-start_)/interval_;
    if(!loop_ && index>=frames_.size()) return Status::EndOfStream;
    out=frames_[index%frames_.size()];out.timestamp_ns=now;out.sequence=index+1;return Status::Ok;
}
}
