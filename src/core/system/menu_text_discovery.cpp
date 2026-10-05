// Ports FF1 Menus/MenuTextDiscovery.cs: the generic cursor reader's text discovery.
#include <string>
#include "core.h"
#include "frame.h"
#include "hooks.h"
#include "log.h"
#include "refs.h"
#include "say.h"
#include "text.h"
#include "ui_util.h"
#include "unity.h"
#include "dialogue/dialogue.h"
#include "system/cursor_event.h"
#include "system/system.h"

namespace ffpr::system {

// cursor_navigation.cpp: run fn(state) with cursor::current() set to this cursor.
void with_cursor_event(void* cur, int index, void* transform, void (*fn)(void*), void* state);

namespace {

ref::Class kTransform{"UnityEngine", "Transform"};

// TextUtils.StripIconMarkup(s.Trim())
std::wstring strip_icons(const std::wstring& s) { return text::trim(text::strip_markup(text::trim(s))); }

// FindContentList: the first active "Content" transform below root (root included, pre-order)
// whose parent is "Viewport" or whose grandparent is "Scroll View".
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

// Strategy 3: walk up the parents looking for a Text directly on each object (not children).
std::wstring try_direct_text_search(void* cursor_transform)
{
    void* current = cursor_transform;
    for (int depth = 0; current && depth < 10; ++depth) {
        if (!unity::game_object(current)) break;
        void* t = unity::get_component(current, unity::text_class());
        std::wstring raw = unity::ui_text(t);
        if (!text::trim(raw).empty()) {
            std::wstring menu_text = strip_icons(raw);
            if (!menu_text.empty()) return menu_text;
        }
        current = unity::parent(current);
    }
    return {};
}

// Strategy 4: a Content list's child at the cursor index.
std::wstring try_content_list_search(void* cursor_transform, int index, int& count)
{
    count = -1;
    void* current = cursor_transform;
    for (int depth = 0; current && depth < 15; ++depth) {
        void* content = find_content_list(current);
        if (content && index >= 0 && index < unity::child_count(content)) {
            if (void* selected = unity::child(content, index)) {
                void* t = unity::get_component_in_children(selected, unity::text_class());
                std::wstring raw = unity::ui_text(t);
                if (!text::trim(raw).empty()) {
                    std::wstring menu_text = strip_icons(raw);
                    if (!menu_text.empty()) {
                        count = unity::child_count(content);
                        return menu_text;
                    }
                }
            }
        }
        current = unity::parent(current);
    }
    return {};
}

// Strategy 5: GetComponentInChildren<Text> up the parents.
std::wstring try_fallback_text_search(void* cursor_transform)
{
    void* current = cursor_transform;
    for (int depth = 0; current && depth < 10; ++depth) {
        if (!unity::game_object(current)) break;
        void* t = unity::get_component_in_children(current, unity::text_class());
        std::wstring raw = unity::ui_text(t);
        if (!text::trim(raw).empty()) {
            std::wstring menu_text = strip_icons(raw);
            if (!menu_text.empty()) return menu_text;
        }
        current = unity::parent(current);
    }
    return {};
}

struct CharSelect {
    std::wstring text;
    int count = -1;
};

void read_character_selection(void* state)
{
    auto* r = static_cast<CharSelect*>(state);
    cursor::current().out_count = -1;
    r->text = queries::text("CharacterSelectionReader.TryReadCharacterSelection");
    r->count = cursor::current().out_count;
}

// TryAllStrategies. `count` is the list size from the strategies that read an indexed list;
// the others leave the last strategy's value, as the C# `out` chain did.
std::wstring try_all_strategies(void* cur, void* t, int index, int& count)
{
    count = -1;

    // Strategy 1: save/load slots, only when a real save/load menu is up (a background save list
    // built during a map load would otherwise read "Autosave: Empty").
    if (should_read_save_slot()) {
        std::wstring s = try_read_save_slot(t, index, count);
        if (!s.empty()) return s;
    }

    // Strategy 2: character selection (formation, status, magic, equipment...), owned by the
    // menus area.
    CharSelect cs;
    with_cursor_event(cur, index, t, &read_character_selection, &cs);
    count = cs.count;
    if (!cs.text.empty()) return cs.text;

    // Strategy 3: direct text up the parents.
    std::wstring s = try_direct_text_search(t);
    if (!s.empty()) return s;

    // Strategy 4: Content list by cursor index.
    s = try_content_list_search(t, index, count);
    if (!s.empty()) return s;

    // Strategy 5: GetComponentInChildren fallback.
    return try_fallback_text_search(t);
}

void read_cursor_now(void* cur)
{
    // Safety checks (the cursor may be gone a frame later).
    if (!unity::alive(cur) || !unity::game_object(cur)) return;
    void* t = unity::transform(cur);
    if (!t) return;

    const int index = ui::cursor_index(cur);
    int count = -1;
    std::wstring menu_text = try_all_strategies(cur, t, index, count);
    if (menu_text.empty()) return;

    // The title and field menus keep their commands in C# lists, not Content transforms: fall
    // back to their command counts (each is -1, inert, on every other menu).
    if (count < 0) count = dialogue::title_active_command_count(cur);
    if (count < 0) count = field_command_count(cur);
    ffpr::say(ui::menu_position(menu_text, index, count));
}

} // namespace

// MenuTextDiscovery.WaitAndReadCursor: wait one frame (the game updates the cursor position
// asynchronously), then read.
void wait_and_read_cursor(void* cur)
{
    if (!cur) return;
    frame::next_frame([cur] {
        hooks::guarded("MenuTextDiscovery.WaitAndReadCursor", [&] { read_cursor_now(cur); });
    }, "MenuTextDiscovery");
}

} // namespace ffpr::system
