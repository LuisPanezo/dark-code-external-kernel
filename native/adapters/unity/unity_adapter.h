#pragma once
#include "adapters/normalize.h"
namespace dc {
struct UnityAdapter {
    Status Convert(const TelemetryFrame& in,FrameSnapshot& out) const {
        return Normalize(in,Coordinates::UnityWorldMeters,out);
    }
};
}
