#include <cassert>

#include "animation_playback.hpp"
#include "animation_transition.hpp"
#include "display_orientation.hpp"
#include "device_identity.hpp"
#include "eye_status.hpp"

using eye_playback::Spec;
using eye_playback::State;

static void advance_to(State &state, const Spec &spec, uint8_t expected) {
    assert(!eye_playback::advance(spec, state));
    assert(state.frame == expected);
}

int main() {
    // Hardware must not add a role-specific mirror after the asset compiler
    // has applied the emulator's independent left/right orientation flags.
    static_assert(eye_display_orientation::kMadctl == 0x88);
    static_assert(eye_device_identity::marker(0) == 0x45594500);
    static_assert(eye_device_identity::marker(1) == 0x45594501);
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

        assert(eye_transition::request(transition, 7, 12, 100) ==
               eye_transition::RequestResult::Queued);
        assert(transition.active_animation == 3);
        assert(transition.active_token == 0);
        assert(transition.pending_animation == 7);
        assert(transition.pending_token == 12);
        assert(eye_transition::request(transition, 7, 12, 101) ==
               eye_transition::RequestResult::Duplicate);

        // A newer request replaces the queued target without activating it.
        assert(eye_transition::request(transition, 8, 13, 102) ==
               eye_transition::RequestResult::Queued);
        assert(transition.pending_animation == 8);
        assert(transition.pending_token == 13);
        assert(transition.active_token == 0);
        assert(!transition.ready);
        assert(eye_transition::playback_flags(transition, true) ==
               (eye_transition::kFlagPending |
                eye_transition::kFlagExiting));

        eye_transition::mark_ready(transition, 120);
        assert(transition.ready);
        assert(eye_transition::playback_flags(transition, true) ==
               (eye_transition::kFlagPending |
                eye_transition::kFlagExiting |
                eye_transition::kFlagReady));

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
                              eye_transition::kFlagExiting |
                              eye_transition::kFlagReady));
        assert(status[12] == 3);

        assert(eye_transition::arm_commit(transition, 99, 50, 200) ==
               eye_transition::CommitResult::WrongToken);
        assert(!transition.commit_armed);
        assert(eye_transition::arm_commit(transition, 13, 9, 200) ==
               eye_transition::CommitResult::InvalidDelay);
        assert(eye_transition::arm_commit(transition, 13, 50, 200) ==
               eye_transition::CommitResult::Armed);
        assert(transition.commit_deadline_ms == 250);
        assert(eye_transition::arm_commit(transition, 13, 60, 210) ==
               eye_transition::CommitResult::Duplicate);
        assert(transition.commit_deadline_ms == 250);
        assert(!eye_transition::commit_due(transition, 249));
        assert(eye_transition::commit_due(transition, 250));

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

        assert(eye_transition::arm_commit(transition, 13, 50, 300) ==
               eye_transition::CommitResult::Duplicate);

        // A new token for the active animation becomes an immediately READY
        // restart, allowing Brain to resynchronize drift in the same state.
        assert(eye_transition::request(transition, 8, 255, 300) ==
               eye_transition::RequestResult::Ready);
        assert(transition.active_token == 13);
        assert(transition.pending_animation == 8);
        assert(transition.pending_token == 255);
        assert(transition.ready);
        assert(eye_transition::arm_commit(transition, 255, 50, 301) ==
               eye_transition::CommitResult::Armed);

        // A replacement invalidates the old READY and commit state.
        assert(eye_transition::request(transition, 9, 1, 310) ==
               eye_transition::RequestResult::Queued);
        assert(!transition.ready);
        assert(!transition.commit_armed);
        assert(transition.commit_deadline_ms == 0);
    }
    {
        // A one-frame animation remains visible for its interval before the
        // queued target and token activate.
        const Spec spec{1, eye_playback::kLoop, 0, 0, 0};
        State playback;
        eye_playback::start(playback);
        eye_transition::State transition;
        transition.active_animation = 4;
        assert(eye_transition::request(transition, 5, 44, 0) ==
               eye_transition::RequestResult::Queued);
        eye_playback::request_exit(spec, playback);
        assert(transition.active_animation == 4);
        assert(transition.active_token == 0);
        assert(eye_playback::advance(spec, playback));
        eye_transition::mark_ready(transition, 100);
        assert(transition.active_animation == 4);
        assert(!eye_transition::commit_due(transition, 1000));
        assert(eye_transition::ready_timed_out(transition, 5100, 5000));
        assert(transition.active_animation == 4);
        assert(eye_transition::arm_commit(transition, 44, 50, 5200) ==
               eye_transition::CommitResult::Armed);
        assert(!eye_transition::commit_due(transition, 5249));
        assert(eye_transition::commit_due(transition, 5250));
        eye_transition::activate_pending(transition);
        assert(transition.active_animation == 5);
        assert(transition.active_token == 44);
    }
    {
        // Different outgoing positions reach READY independently. Sequential
        // commit STOP timestamps produce only their one-millisecond skew, and
        // a late wake-up catches both destination playbacks up to frame 3.
        eye_transition::State left_transition;
        eye_transition::State right_transition;
        left_transition.active_animation = 2;
        right_transition.active_animation = 2;
        assert(eye_transition::request(left_transition, 7, 33, 0) ==
               eye_transition::RequestResult::Queued);
        assert(eye_transition::request(right_transition, 7, 33, 0) ==
               eye_transition::RequestResult::Queued);
        eye_transition::mark_ready(left_transition, 100);
        assert(!right_transition.ready);
        assert(left_transition.active_animation == 2);
        eye_transition::mark_ready(right_transition, 180);
        assert(right_transition.active_animation == 2);
        assert(eye_transition::arm_commit(left_transition, 33, 50, 200) ==
               eye_transition::CommitResult::Armed);
        assert(eye_transition::arm_commit(right_transition, 33, 50, 201) ==
               eye_transition::CommitResult::Armed);
        assert(left_transition.commit_deadline_ms == 250);
        assert(right_transition.commit_deadline_ms == 251);
        assert(!eye_transition::commit_due(left_transition, 249));
        assert(!eye_transition::commit_due(right_transition, 250));

        eye_transition::activate_pending(left_transition);
        eye_transition::activate_pending(right_transition);
        State left_playback;
        State right_playback;
        eye_playback::start(left_playback);
        eye_playback::start(right_playback);
        const Spec destination{8, eye_playback::kLoop, 0, 0, 0};
        uint32_t left_frame_started = 250;
        uint32_t right_frame_started = 251;
        const uint32_t late_wake = 371;
        while (late_wake - left_frame_started >= 40) {
            left_frame_started += 40;
            assert(!eye_playback::advance(destination, left_playback));
        }
        while (late_wake - right_frame_started >= 40) {
            right_frame_started += 40;
            assert(!eye_playback::advance(destination, right_playback));
        }
        assert(left_playback.frame == 3);
        assert(right_playback.frame == 3);
    }
    return 0;
}
