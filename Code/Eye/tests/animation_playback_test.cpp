#include <cassert>

#include "animation_playback.hpp"
#include "animation_transition.hpp"
#include "eye_status.hpp"

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
    {
        eye_transition::State transition;
        transition.active_animation = 3;

        assert(eye_transition::request(transition, 7, 12) ==
               eye_transition::RequestResult::Queued);
        assert(transition.active_animation == 3);
        assert(transition.active_token == 0);
        assert(transition.pending_animation == 7);
        assert(transition.pending_token == 12);
        assert(eye_transition::request(transition, 7, 12) ==
               eye_transition::RequestResult::Duplicate);

        // A newer request replaces the queued target without activating it.
        assert(eye_transition::request(transition, 8, 13) ==
               eye_transition::RequestResult::Queued);
        assert(transition.pending_animation == 8);
        assert(transition.pending_token == 13);
        assert(transition.active_token == 0);
        assert(eye_transition::playback_flags(transition, true) ==
               (eye_transition::kFlagPending |
                eye_transition::kFlagExiting));

        uint8_t status[kEyeStatusSize];
        eye_status::encode(
            status, 1, 1, 1, transition, 22, 0, 180, true, 3);
        assert(status[0] == kProtocolVersion);
        assert(status[1] == 1);
        assert(status[4] == 3);
        assert(status[8] == 8);
        assert(status[9] == 0);
        assert(status[10] == 13);
        assert(status[11] == (eye_transition::kFlagPending |
                              eye_transition::kFlagExiting));
        assert(status[12] == 3);

        eye_transition::activate_pending(transition);
        assert(transition.active_animation == 8);
        assert(transition.active_token == 13);
        assert(transition.pending_animation == eye_transition::kNoAnimation);
        assert(transition.pending_token == 0);
        assert(eye_transition::playback_flags(transition, false) ==
               eye_transition::kFlagActivated);
        eye_status::encode(
            status, 0, 1, 1, transition, 22, 0, 180, false, 0);
        assert(status[4] == 8);
        assert(status[8] == eye_transition::kNoAnimation);
        assert(status[9] == 13);
        assert(status[10] == 0);
        assert(status[11] == eye_transition::kFlagActivated);

        // Repeating the active target acknowledges a new token immediately.
        assert(eye_transition::request(transition, 8, 255) ==
               eye_transition::RequestResult::Activated);
        assert(transition.active_token == 255);
        assert(eye_transition::request(transition, 8, 1) ==
               eye_transition::RequestResult::Activated);
        assert(transition.active_token == 1);
    }
    {
        // A one-frame animation remains visible for its interval before the
        // queued target and token activate.
        const Spec spec{1, eye_playback::kLoop, 0, 0, 0};
        State playback;
        eye_playback::start(playback);
        eye_transition::State transition;
        transition.active_animation = 4;
        assert(eye_transition::request(transition, 5, 44) ==
               eye_transition::RequestResult::Queued);
        eye_playback::request_exit(spec, playback);
        assert(transition.active_animation == 4);
        assert(transition.active_token == 0);
        assert(eye_playback::advance(spec, playback));
        eye_transition::activate_pending(transition);
        assert(transition.active_animation == 5);
        assert(transition.active_token == 44);
    }
    return 0;
}
