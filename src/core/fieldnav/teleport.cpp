// Ports FF1 Core/FFI_ScreenReaderMod.cs: TeleportInDirection and GetDirectionFromOffset.
// FF1's KeyBindingRegistry binds Ctrl+arrows with a 16-unit (one tile) offset, so the
// lambda's four arguments become four actions: "Mod.TeleportNorth" (0, 16),
// "Mod.TeleportSouth" (0, -16), "Mod.TeleportEast" (16, 0), "Mod.TeleportWest" (-16, 0).
#include <cmath>
#include <string>
#include "../core.h"
#include "../log.h"
#include "../modtext.h"
#include "../refs.h"
#include "../say.h"
#include "../unity.h"
#include "../field/field_api.h"

namespace ffpr::fieldnav::teleport {
namespace {

ref::Class  kTransform{"UnityEngine", "Transform"};
ref::Method mSetLocalPosition{kTransform, "set_localPosition", 1};

std::wstring direction_from_offset(float ox, float oy)
{
    if (std::fabs(ox) > std::fabs(oy))
        return ox > 0 ? T(L"east") : T(L"west");
    return oy > 0 ? T(L"north") : T(L"south");
}

/// Teleport the player to the selected entity's position plus an offset (16 units = 1 tile).
void teleport_in_direction(float ox, float oy)
{
    try {
        void* player = ::field::player();
        if (!player) {
            ffpr::say(T(L"Not on field map"), true);
            return;
        }

        ::field::Target entity = ::field::selected();
        if (!entity.valid) {
            ffpr::say(T(L"No entity selected"), true);
            return;
        }
        // Another floor's entry (2026-10-11) has no position on this map: nothing to teleport to.
        if (entity.remote_map_id > 0) {
            ffpr::say(T(L"Not on map"), true);
            return;
        }

        // Target position: entity position + offset
        unity::Vec3 target{entity.position.x + ox, entity.position.y + oy, entity.position.z + 0.0f};

        // player.transform.localPosition = target
        void* tr = unity::transform(player);
        void* fp = mSetLocalPosition.ptr();
        if (!tr || !fp) {
            ffpr::say(T(L"Teleport failed"), true);
            return;
        }
        using F = void (*)(void*, unity::Vec3, const void*);
        reinterpret_cast<F>(fp)(tr, target, mSetLocalPosition.get());

        std::wstring direction = direction_from_offset(ox, oy);
        ffpr::say(TF(L"Teleported to {0} of {1}", {direction, entity.name}), true);
    } catch (...) {
        LOGF("Error teleporting");
        ffpr::say(T(L"Teleport failed"), true);
    }
}

int install()
{
    actions::define("Mod.TeleportNorth", [] { teleport_in_direction(0.0f, 16.0f); });
    actions::define("Mod.TeleportSouth", [] { teleport_in_direction(0.0f, -16.0f); });
    actions::define("Mod.TeleportEast", [] { teleport_in_direction(16.0f, 0.0f); });
    actions::define("Mod.TeleportWest", [] { teleport_in_direction(-16.0f, 0.0f); });
    return 0;
}
core::Install s_install{"Teleport", &install, 110};

} // namespace
} // namespace ffpr::fieldnav::teleport
