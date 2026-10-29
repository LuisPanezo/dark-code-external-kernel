#pragma once
#include "core/coordinate_system.h"
#include "core/frame_snapshot.h"
#include "core/status.h"
namespace dc {
// Input consists ONLY of values exported by an application owned by the caller.
// The camera uses world position/target/up, avoiding ambiguity in engine GPU matrices.
struct TelemetryFrame { CameraFrame camera; std::vector<Entity> entities; uint64_t sequence{},timestamp_ns{}; };
Vec3 ToCanonical(Vec3 value,Coordinates coordinates);
Vec3 FromCanonical(Vec3 value,Coordinates coordinates);
Status Normalize(const TelemetryFrame& input,Coordinates coordinates,FrameSnapshot& output);
}
