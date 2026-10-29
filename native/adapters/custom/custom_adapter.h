#pragma once
#include "adapters/normalize.h"
namespace dc {
struct CustomAdapter {
    Status Convert(const TelemetryFrame& in,FrameSnapshot& out) const {
        return Normalize(in,Coordinates::CanonicalMeters,out);
    }
};
}
