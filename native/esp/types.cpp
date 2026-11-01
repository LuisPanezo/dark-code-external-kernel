#include "types.h"

namespace dc::esp {

const char* StateName(State state) {
    switch (state) {
    case State::Detached: return "Detached";
    case State::Starting: return "Starting";
    case State::NoPid:   return "NoPid";
    case State::NoDriver:return "NoDriver";
    case State::NoWorld: return "NoWorld";
    case State::NoCamera:return "NoCamera";
    case State::Live:    return "Live";
    case State::Error:   return "Error";
    }
    return "Error";
}

} // namespace dc::esp
