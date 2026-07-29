#include <cassert>

#include "animation_playback.hpp"

using eye_playback::Spec;
using eye_playback::State;

static void advance_to(State &state, const Spec &spec, uint8_t expected) {
    assert(!eye_playback::advance(spec, state));
    assert(state.frame == expected);
}

int main() {
    {
        const Spec spec{7, eye_playback::kLoop, 2, 4, eye_playback::kLoop};
        State state;
        eye_playback::start(state);
        advance_to(state, spec, 1);
        advance_to(state, spec, 2);
        advance_to(state, spec, 3);
        advance_to(state, spec, 2);
        advance_to(state, spec, 3);
        eye_playback::request_exit(spec, state);
        advance_to(state, spec, 4);
        advance_to(state, spec, 5);
        advance_to(state, spec, 6);
        assert(eye_playback::advance(spec, state));
    }
    {
        const Spec spec{
            5, eye_playback::kLoop, 1, 4, eye_playback::kPingPong,
        };
        State state;
        eye_playback::start(state);
        advance_to(state, spec, 1);
        advance_to(state, spec, 2);
        advance_to(state, spec, 3);
        advance_to(state, spec, 2);
        eye_playback::request_exit(spec, state);
        advance_to(state, spec, 1);
        advance_to(state, spec, 2);
        advance_to(state, spec, 3);
        advance_to(state, spec, 4);
        assert(eye_playback::advance(spec, state));
    }
    {
        const Spec spec{4, eye_playback::kPingPong, 0, 0, 0};
        State state;
        eye_playback::start(state);
        advance_to(state, spec, 1);
        advance_to(state, spec, 2);
        advance_to(state, spec, 3);
        advance_to(state, spec, 2);
        eye_playback::request_exit(spec, state);
        advance_to(state, spec, 1);
        advance_to(state, spec, 0);
        assert(eye_playback::advance(spec, state));
    }
    {
        // A one-frame final-pose loop in an overall ping-pong animation
        // automatically returns through its entry frames when asked to end.
        const Spec spec{
            6, eye_playback::kPingPong, 5, 6, eye_playback::kLoop,
        };
        State state;
        eye_playback::start(state);
        for (uint8_t frame = 1; frame <= 5; ++frame) {
            advance_to(state, spec, frame);
        }
        advance_to(state, spec, 5);
        eye_playback::request_exit(spec, state);
        advance_to(state, spec, 4);
        advance_to(state, spec, 3);
        advance_to(state, spec, 2);
        advance_to(state, spec, 1);
        advance_to(state, spec, 0);
        assert(eye_playback::advance(spec, state));
    }
    return 0;
}
