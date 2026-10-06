// Ports FF2 Menus/ShopCommandReader.cs: the shop command bar (Buy / Sell / Equipment / Back) as a
// strategy of FF2's generic cursor reader (MenuTextDiscovery strategy 1).
//
// FF1 has no such strategy and the core's MenuTextDiscovery has no hook for one, so it runs as a
// cursor suppressor: the core asks it only after its own suppression checks (the shop tracker
// among them, which silences the generic reader while ShopMenuTracker is valid, as in FF2). When
// the cursor sits under a shop command menu, this query claims the move and, one frame later as
// MenuTextDiscovery did, reads the command; when nothing is read it hands the cursor back to the
// core's MenuTextDiscovery (the remaining strategies, one frame later).
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/ui_util.h"
#include "../../core/unity.h"
#include "../../core/system/cursor_event.h"
#include "../../core/system/system.h"

namespace ffpr::ff2 {
namespace {

ref::Class kTransform{"UnityEngine", "Transform", game::kFF2};
ref::Class kShopCommandMenu{"Last.UI.KeyInput", "ShopCommandMenuController", game::kFF2};
ref::Field fScmContentList{kShopCommandMenu, "contentList"};
ref::Class kShopCommandContent{"Last.UI.KeyInput", "ShopCommandMenuContentController", game::kFF2};
ref::Method mSccCommandId{kShopCommandContent, "get_CommandId", 0};
ref::Method mSccFocusCursorParent{kShopCommandContent, "get_FocusCursorParent", 0};

bool is_shop_command_name(const std::wstring& lower) { return text::contains(lower, L"shop") && text::contains(lower, L"command"); }
bool is_command_content_name(const std::wstring& lower) { return text::contains(lower, L"command") && text::contains(lower, L"content"); }

/// GetCommandName: CommandBarReader.GetShopCommandName(content.CommandId); empty = null.
std::wstring command_name(void* content)
{
    return system::shop_command_name(ref::call<int32_t>(mSccCommandId, content));
}

/// FindContentList: the first "Content" below root whose parent is "Viewport" or whose
/// grandparent is "Scroll View".
void* find_content_list(void* root)
{
    for (void* t : unity::components_in_children(root, kTransform, false)) {
        if (unity::name(t) != L"Content") continue;
        void* p = unity::parent(t);
        if (!p) continue;
        if (unity::name(p) == L"Viewport") return t;
        void* gp = unity::parent(p);
        if (gp && unity::name(gp) == L"Scroll View") return t;
    }
    return nullptr;
}

/// ReadCommandFromTransform.
std::wstring read_command_from_transform(void* slot)
{
    if (!slot) return {};
    void* cc = unity::get_component(slot, kShopCommandContent);
    if (!cc) cc = unity::get_component_in_children(slot, kShopCommandContent);
    return cc ? command_name(cc) : std::wstring();
}

/// ReadFromController.
std::wstring read_from_controller(void* controller, int index)
{
    void* list = ref::rd<void*>(controller, fScmContentList);
    const int count = unity::list_count(list);
    if (!list || count == 0) return {};
    if (index >= 0 && index < count) {
        void* content = unity::list_at(list, index);
        if (content) return command_name(content);
    }
    for (int i = 0; i < count; ++i) {
        void* content = unity::list_at(list, i);
        if (!content) continue;
        void* focus = ref::call<void*>(mSccFocusCursorParent, content);
        if (focus && unity::active_in_hierarchy(focus)) return command_name(content);
    }
    return {};
}

/// TryReadShopCommand: the command under the cursor, or empty.
std::wstring try_read_shop_command(void* cursor_transform, int index)
{
    void* current = cursor_transform;
    for (int depth = 0; current && depth < 15; ++depth) {
        const std::wstring lower = text::lower(unity::name(current));
        if (is_shop_command_name(lower)) {
            void* content_list = find_content_list(current);
            if (content_list && index >= 0 && index < unity::child_count(content_list)) {
                std::wstring s = read_command_from_transform(unity::child(content_list, index));
                if (!s.empty()) return s;
            }
            void* controller = unity::get_component(current, kShopCommandMenu);
            if (!controller) controller = unity::get_component_in_children(current, kShopCommandMenu);
            if (controller) return read_from_controller(controller, index);
        }
        if (is_command_content_name(lower)) {
            std::wstring s = read_command_from_transform(current);
            if (!s.empty()) return s;
        }
        current = unity::parent(current);
    }
    return {};
}

/// Whether the cursor sits where TryReadShopCommand could read (the same walk, names only).
bool under_shop_command(void* cursor_transform)
{
    void* current = cursor_transform;
    for (int depth = 0; current && depth < 15; ++depth) {
        const std::wstring lower = text::lower(unity::name(current));
        if (is_shop_command_name(lower)) return true;
        if (is_command_content_name(lower) &&
            (unity::get_component(current, kShopCommandContent) || unity::get_component_in_children(current, kShopCommandContent)))
            return true;
        current = unity::parent(current);
    }
    return false;
}

bool claim_cursor()
{
    const cursor::CursorEvent& e = cursor::current();
    if (!e.cursor || !e.transform || !under_shop_command(e.transform)) return false;
    void* cur = e.cursor;
    frame::next_frame([cur] {
        hooks::guarded("FF2.ShopCommandReader", [&] {
            if (!unity::alive(cur)) return;
            void* t = unity::transform(cur);
            if (!t) return;
            std::wstring s = try_read_shop_command(t, ui::cursor_index(cur));
            if (!s.empty()) {
                ffpr::say(s);   // no list count from this strategy: no position suffix
                return;
            }
            system::wait_and_read_cursor(cur);   // the remaining MenuTextDiscovery strategies
        });
    }, "FF2.ShopCommandReader");
    return true;
}

int install()
{
    queries::define("ShopCommandReader.ClaimsCursor", [] { return claim_cursor(); });
    cursor::add_suppressor("ShopCommandReader.ClaimsCursor");
    return 0;
}

core::Install s_install{"FF2.ShopCommandReader", &install, 520, game::kFF2};

} // namespace
} // namespace ffpr::ff2
