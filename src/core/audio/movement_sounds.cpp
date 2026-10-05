// Ports FF1 Patches/MovementSoundPatches.cs: wall bumps and footsteps.
//
// Wall bump: a prefix on FieldPlayerKeyController.OnTouchPadCallback(Vector2) (the field
// movement input; one body per game, not folded in any dump.cs) notes the player's position,
// and 0.08 s later (the C# coroutine's WaitForSeconds) a check plays the thud if the player
// did not move - only after two hits at the same spot, and at most every 0.3 s.
//
// Footsteps: PollFootsteps, the per-frame tile-crossing check the C# InputManager.Update ran
// right after ControllerRouter.Update. It is the action "MovementSoundPatches.PollFootsteps";
// the input area runs it each frame (an allowed audio loop).
//
// Reads other areas' state by name: "ControllerRouter.IsFieldActive" (input/router) and
// "MoveStateHelper.IsOnFoot" (vehicle state); the player is field::player().
#include <cmath>
#include "sound.h"
#include "../core.h"
#include "../frame.h"
#include "../hooks.h"
#include "../log.h"
#include "../refs.h"
#include "../settings.h"
#include "../unity.h"
#include "../field/field_api.h"

namespace ffpr::audio {
namespace {

ref::Class kFieldPlayerController{"Last.Map", "FieldPlayerController"};
ref::Class kFieldPlayerKeyController{"Last.Map", "FieldPlayerKeyController"};
ref::Field fFieldPlayer{kFieldPlayerController, "fieldPlayer"};
ref::Method mOnTouchPadCallback{kFieldPlayerKeyController, "OnTouchPadCallback", 1};

// Cooldown so holding a direction against a wall does not spam the bump.
constexpr double kBumpCooldown = 0.3;          // seconds between bump sounds
// Consecutive failed moves at one spot, to tell real walls from false positives.
constexpr int kRequiredConsecutiveHits = 2;
constexpr float kPositionTolerance = 1.0f;     // positions within 1 unit are "the same"
constexpr float kTileSize = 16.0f;
constexpr double kWallCheckDelay = 0.08;       // movement takes ~0.067 s per tile

double g_last_bump_time = -1.0e9;   // C# 0 against Time.time: no cooldown after a reset
bool g_logged_active = false;

unity::Vec3 g_last_collision_pos{};
int g_same_position_count = 0;

// Only one wall check at a time (no stacking while a direction is held).
bool g_wall_check_pending = false;

// Set by a wall check that saw no movement, so the footstep poll skips the coincident step.
bool g_collision_detected_this_frame = false;

// Tile tracking for the per-frame footstep poll.
int g_last_tile_x = 0;
int g_last_tile_y = 0;
bool g_tile_tracking_initialized = false;

float distance(const unity::Vec3& a, const unity::Vec3& b)
{
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

bool has_movement_input(unity::Vec2 axis)
{
    constexpr float kInputThreshold = 0.1f;
    return std::fabs(axis.x) > kInputThreshold || std::fabs(axis.y) > kInputThreshold;
}

void play_bump_sound()
{
    hooks::guarded("MovementSoundPatches.PlayBumpSound", [] { sound::play_wall_bump(); });
}

/// CheckForWallBumpAfterFrame, after its 0.08 s wait.
void check_for_wall_bump(void* player, unity::Vec3 position_before)
{
    hooks::guarded("MovementSoundPatches.CheckForWallBumpAfterFrame", [&] {
        // The player may be gone (map change); a destroyed object reads as not alive.
        if (!unity::alive(player) || !unity::transform(player)) return;

        unity::Vec3 position_after = unity::local_position(player);
        float distance_moved = distance(position_before, position_after);

        if (distance_moved < 0.1f) {
            // Mark the collision so the per-frame footstep poll skips this step.
            g_collision_detected_this_frame = true;

            float dist_from_last = distance(position_before, g_last_collision_pos);
            if (dist_from_last < kPositionTolerance) {
                g_same_position_count++;
            } else {
                // New position: restart the count.
                g_same_position_count = 1;
                g_last_collision_pos = position_before;
            }

            // Only after confirmed consecutive hits.
            if (g_same_position_count < kRequiredConsecutiveHits) return;

            // Cooldown checked after the wait, so several checks never all play at once.
            double current_time = frame::now();
            if (current_time - g_last_bump_time < kBumpCooldown) return;

            play_bump_sound();
            g_last_bump_time = current_time;
        } else {
            // The player moved: reset the collision counter.
            g_same_position_count = 0;
        }

        // Reset the collision flag at the end of the check.
        g_collision_detected_this_frame = false;
    });
    // Always clear the pending flag so the next check can start (the C# finally).
    g_wall_check_pending = false;
}

/// OnTouchPadCallback prefix: capture the position, check it after the move.
void on_touch_pad_prefix(void* self, unity::Vec2 axis)
{
    if (!g_logged_active) {
        g_logged_active = true;
        LOGF("MovementSoundPatches: OnTouchPadCallback hook active");
    }

    // No audio when menus or battle are active (same gate as controller routing).
    if (!queries::is("ControllerRouter.IsFieldActive")) return;

    // Only real movement input.
    if (!has_movement_input(axis)) return;

    void* player = ref::rd<void*>(self, fFieldPlayer);
    if (!player || !unity::transform(player)) return;

    // One pending check at a time: holding a direction must not stack them.
    if (!g_wall_check_pending) {
        g_wall_check_pending = true;
        unity::Vec3 position_before = unity::local_position(player);
        frame::after_seconds(kWallCheckDelay, [player, position_before] {
            check_for_wall_bump(player, position_before);
        }, "MovementSoundPatches.WallCheck");
    }
    // Wall tones and beacons are the audio loops' job, not this hook's.
}

using OnTouchPadCallback_t = void (*)(void* self, unity::Vec2 axis, const void* mi);
OnTouchPadCallback_t o_OnTouchPadCallback = nullptr;

void d_OnTouchPadCallback(void* self, unity::Vec2 axis, const void* mi)
{
    hooks::guarded("MovementSoundPatches.OnTouchPadCallback", [&] { on_touch_pad_prefix(self, axis); });
    o_OnTouchPadCallback(self, axis, mi);
}

/// PollFootsteps: a footstep each time the player's tile changes, so the cadence follows the
/// real speed (walk vs dash). Silent in vehicles, in menus and battle, and on the step that
/// coincides with a wall bump.
void poll_footsteps()
{
    hooks::guarded("MovementSoundPatches.PollFootsteps", [] {
        if (!queries::is("ControllerRouter.IsFieldActive")) return;
        if (!settings::get_bool("Footsteps", false)) return;
        if (!queries::is("MoveStateHelper.IsOnFoot")) return;

        void* player = field::player();
        if (!player || !unity::transform(player)) return;

        unity::Vec3 pos = unity::local_position(player);
        if (std::isnan(pos.x) || std::fabs(pos.x) > 10000.0f) return;

        int tile_x = static_cast<int>(std::floor(pos.x / kTileSize));
        int tile_y = static_cast<int>(std::floor(pos.y / kTileSize));

        if (!g_tile_tracking_initialized) {
            g_last_tile_x = tile_x;
            g_last_tile_y = tile_y;
            g_tile_tracking_initialized = true;
            return;
        }

        if (tile_x == g_last_tile_x && tile_y == g_last_tile_y) return;
        g_last_tile_x = tile_x;
        g_last_tile_y = tile_y;

        // A wall check set this for the frame it fired: skip the coincident step.
        if (g_collision_detected_this_frame) return;

        sound::play_footstep();
    });
}

/// ResetState: on scene loads, so stale collision and footstep data never cross maps.
void reset_state()
{
    g_last_bump_time = -1.0e9;
    g_last_collision_pos = unity::Vec3{};
    g_same_position_count = 0;
    g_wall_check_pending = false;
    g_collision_detected_this_frame = false;
    g_last_tile_x = 0;
    g_last_tile_y = 0;
    g_tile_tracking_initialized = false;
}

int install()
{
    actions::define("MovementSoundPatches.PollFootsteps", [] { poll_footsteps(); });
    actions::define("MovementSoundPatches.ResetState", [] { reset_state(); });
    events::on(events::Kind::SceneLoaded, [] { reset_state(); });
    return hooks::attach(mOnTouchPadCallback, &d_OnTouchPadCallback, &o_OnTouchPadCallback) ? 1 : 0;
}

core::Install s_install{"MovementSoundPatches", &install, 100};

} // namespace
} // namespace ffpr::audio
