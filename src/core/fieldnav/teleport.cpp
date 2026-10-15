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
#include "../field/baked_nav.h"
#include "../field/field_internal.h"
#include "../field/capture_budget.h"
#include "../input/input.h"
#include "../ui/preferences.h"
#include "../ui/mod_ui.h"
#include "../worker_job.h"
#include "autowalk.h"

namespace ffpr::fieldnav::teleport {
namespace {

ref::Class  kTransform{"UnityEngine", "Transform"};
ref::Method mSetLocalPosition{kTransform, "set_localPosition", 1};
ref::Class kFieldEntity{"Last.Entity.Field", "FieldEntity"};
ref::Method mPlayerIsMoving{kFieldEntity, "get_IsMoving", 0};
worker_job::Owner g_owner;
constexpr double kMaxLandingAge = 0.5; // fail closed if capture/worker publication becomes stale
constexpr size_t kMaxEntities = 8192;

std::wstring direction_from_offset(float ox, float oy)
{
    if (std::fabs(ox) > std::fabs(oy))
        return ox > 0 ? T(L"east") : T(L"west");
    return oy > 0 ? T(L"north") : T(L"south");
}

/// Teleport the player to the selected entity's position plus an offset (16 units = 1 tile).
void teleport_in_direction(float ox, float oy)
{
    g_owner.cancel();
    frame::cancel_label("Teleport.");
    if (!ui::prefs::teleport_shortcuts_enabled() ||
        !queries::is("GameStatePatches.IsFieldNavigationReady") ||
        queries::is("GameStatePatches.IsInputTransitionBlocked") ||
        queries::is("MessageWindowPatches.IsInDialogue") ||
        queries::is("BattleStateHelper.IsInBattle") || menu_state::any_active() || ui::any_modal_open()) return;
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
        if (!mPlayerIsMoving.ptr() || ref::call<bool>(mPlayerIsMoving, player)) {
            ffpr::say(T(L"Teleport cancelled."), true); return;
        }
        const auto vehicle = ::field::current_vehicle();
        if (!vehicle.valid || (vehicle.type != 0 && vehicle.type != 1)) {
            ffpr::say(T(L"Teleport unavailable while riding."), true);
            return;
        }
        void* map = ::field::map_handle();
        ::field::baked_route::Cell origin;
        auto snapshot = ::field::baked_nav::begin_exit_capture(false, true);
        if (!snapshot || !::field::baked_nav::player_cell(map, player, origin)) {
            ffpr::say(T(L"Teleport blocked: no safe landing."), true); return;
        }
        autowalk::cancel();
        unity::Vec3 target{entity.position.x + ox, entity.position.y + oy, entity.position.z + 0.0f};
        const int map_id = ::field::current_map_id();
        const double started = frame::now();
        auto current = [map_id, origin, entity, vehicle, started, player] {
            if (frame::now() - started > kMaxLandingAge || !ui::prefs::teleport_shortcuts_enabled() ||
                !input::game_window_focused() || !queries::is("GameStatePatches.IsFieldNavigationReady") ||
                queries::is("GameStatePatches.IsInputTransitionBlocked") ||
                queries::is("MessageWindowPatches.IsInDialogue") ||
                queries::is("BattleStateHelper.IsInBattle") || menu_state::any_active() || ui::any_modal_open() ||
                ::field::current_map_id() != map_id || ::field::player() != player ||
                ref::call<bool>(mPlayerIsMoving, player)) return false;
            ::field::baked_route::Cell now;
            const auto selected = ::field::selected();
            const auto riding = ::field::current_vehicle();
            return ::field::baked_nav::player_cell(::field::map_handle(), ::field::player(), now) && now == origin &&
                riding.valid && riding.id == vehicle.id && riding.type == vehicle.type &&
                selected.valid && selected.identity == entity.identity && selected.remote_map_id == 0 &&
                selected.position.x == entity.position.x && selected.position.y == entity.position.y;
        };
        auto cancel_notice = [map_id] {
            if (input::game_window_focused() && queries::is("GameStatePatches.IsFieldNavigationReady") &&
                ::field::current_map_id() == map_id && !queries::is("MessageWindowPatches.IsInDialogue") &&
                !queries::is("BattleStateHelper.IsInBattle") && !menu_state::any_active() && !ui::any_modal_open())
                ffpr::say(T(L"Teleport cancelled."), true);
        };
        auto cursor = std::make_shared<::field::EntityCursor>();
        auto count = std::make_shared<size_t>(0);
        auto epoch = g_owner.epoch(); const auto generation = epoch->load();
        ffpr::say(T(L"Checking teleport landing."), true);
        frame::every_frame([=]() mutable {
            if (epoch->load() != generation) return false;
            if (!current()) { cancel_notice(); return false; }
            ::field::CaptureBudget budget;
            void* entity_ptr = nullptr;
            bool more = true;
            while (budget.available() && (more = cursor->next(entity_ptr))) {
                if (++*count > kMaxEntities) { cancel_notice(); return false; }
                if (entity_ptr && entity_ptr != ::field::player())
                    ::field::baked_nav::capture_exit_obstacle(*snapshot, entity_ptr);
            }
            if (more) return true;
            ::field::baked_nav::finish_exit_capture(*snapshot);
            auto completed = std::make_shared<bool>(false);
            worker_job::start(g_owner, std::pair{snapshot, target},
                [](auto& work) { return ::field::baked_nav::teleport_landing_safe(*work.first, work.second); },
                [=](bool safe) {
                    *completed = true;
                    if (!current() || input::suppress_game_input()) { cancel_notice(); return; }
                    if (!safe) { ffpr::say(T(L"Teleport blocked: no safe landing."), true); return; }
                    void* tr = unity::transform(::field::player());
                    void* fp = mSetLocalPosition.ptr();
                    if (!tr || !fp) { ffpr::say(T(L"Teleport failed"), true); return; }
                    auto landing = target;
                    landing.z = unity::local_position(tr).z; // retain the native player's current layer/depth
                    using F = void (*)(void*, unity::Vec3, const void*);
                    reinterpret_cast<F>(fp)(tr, landing, mSetLocalPosition.get());
                    ::field::baked_nav::invalidate();
                    LOGF("[Teleport] landing checked: %zu entities, %.3f ms request-to-move", *count, (frame::now()-started)*1000);
                    ffpr::say(TF(L"Teleported to {0} of {1}", {direction_from_offset(ox, oy), entity.name}), true);
                }, background::Lane::Navigation);
            frame::every_frame([=] {
                if (*completed || epoch->load() != generation) return false;
                if (current() && !input::suppress_game_input()) return true;
                g_owner.cancel(); cancel_notice(); return false;
            }, "Teleport.Expiry");
            return false;
        }, "Teleport.Capture");
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
