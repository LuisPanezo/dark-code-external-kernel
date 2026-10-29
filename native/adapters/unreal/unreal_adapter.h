#pragma once
#include "adapters/normalize.h"
namespace dc {
struct UnrealAdapter {
    Status Convert(const TelemetryFrame& in,FrameSnapshot& out) const {
        return Normalize(in,Coordinates::UnrealWorldCentimeters,out);
    }
};
}
