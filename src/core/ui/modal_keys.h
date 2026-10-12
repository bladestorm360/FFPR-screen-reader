#pragma once
// Internal to src/core/ui: keyboard state for the mod's own modal UI.
//
// FF1 read these keys through GamepadManager (GetAsyncKeyState, edge-detected once per frame).
// Here the modals keep their own copy of that edge detection, limited to the keys they read and
// sampled only while one of them is open and the game window is focused.
namespace ui::modal_keys {

/// Take the keyboard as it is now as "already seen": a key held at this moment is not a press.
/// Called when a modal opens, so the key that opened it does not act inside it.
void seed();
/// Read the keyboard once per frame. Background keys read up; the first focused sample
/// seeds held keys without press edges. Later calls in the same frame do nothing.
void sample();
/// True on the frame a key (Windows VK code) went down.
bool pressed(int vk);
/// True while a key is down.
bool held(int vk);

} // namespace ui::modal_keys
