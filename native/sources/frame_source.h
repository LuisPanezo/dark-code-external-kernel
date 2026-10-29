#pragma once
#include "core/frame_snapshot.h"
#include "core/status.h"
namespace dc {
class IFrameSource {
public:
    virtual ~IFrameSource()=default;
    virtual Status Start()=0;
    // Nonblocking; only the acquisition thread calls ReadLatest or Stop.
    virtual Status ReadLatest(FrameSnapshot& output)=0;
    virtual void Stop() noexcept=0;
};
}
