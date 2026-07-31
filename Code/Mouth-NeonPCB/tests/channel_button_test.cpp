#include <cassert>

#include "../channel_button.hpp"

int main() {
    using mouth_channel::ButtonAction;
    using mouth_channel::ButtonState;
    using mouth_channel::nextChannel;
    using mouth_channel::updateButton;

    assert(nextChannel(1) == 2);
    assert(nextChannel(12) == 13);
    assert(nextChannel(13) == 1);
    assert(nextChannel(0) == 1);

    ButtonState shortPress;
    assert(updateButton(shortPress, true, 100) == ButtonAction::None);
    assert(updateButton(shortPress, true, 129) == ButtonAction::None);
    assert(updateButton(shortPress, true, 130) == ButtonAction::None);
    assert(updateButton(shortPress, false, 400) == ButtonAction::None);
    assert(updateButton(shortPress, false, 430) ==
           ButtonAction::NextChannel);
    assert(updateButton(shortPress, false, 500) == ButtonAction::None);

    ButtonState longPress;
    assert(updateButton(longPress, true, 1000) == ButtonAction::None);
    assert(updateButton(longPress, true, 1030) == ButtonAction::None);
    assert(updateButton(longPress, true, 2229) == ButtonAction::None);
    assert(updateButton(longPress, true, 2230) ==
           ButtonAction::ResetChannel);
    assert(updateButton(longPress, true, 2500) == ButtonAction::None);
    assert(updateButton(longPress, false, 2600) == ButtonAction::None);
    assert(updateButton(longPress, false, 2630) == ButtonAction::None);
    return 0;
}
