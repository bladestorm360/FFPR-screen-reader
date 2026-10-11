// Ports FF1 Menus/CharacterSelectionReader.cs: reads "Name, Job, Level X, HP a/b, MP c/d" from
// the character slot under a menu cursor (formation, status, magic, equipment ... and battle).
// All five games: it only walks Unity objects by name. Called by the generic cursor reader
// (MenuTextDiscovery, strategy 2) through the text query below, or through menus.h.
#include <cwctype>
#include "menus.h"
#include "menus_internal.h"
#include "../core.h"
#include "../modtext.h"
#include "../text.h"
#include "../unity.h"
#include "../displayed_ui.h"
#include "../system/cursor_event.h"

namespace ffpr::menus {
namespace {

ref::Class  kSceneManager{"UnityEngine.SceneManagement", "SceneManager"};
ref::Method mGetActiveScene{kSceneManager, "GetActiveScene", 0};
ref::Class  kScene{"UnityEngine.SceneManagement", "Scene"};
ref::Method mSceneGetNameInternal{kScene, "GetNameInternal", 1};

constexpr int kMaxDepth = 15;

/// SceneManager.GetActiveScene().name. Scene is a struct holding one int (m_Handle); it comes
/// back in a register, and Scene.get_name is GetNameInternal(handle).
std::wstring active_scene_name()
{
    int32_t handle = ref::call_static<int32_t>(mGetActiveScene);
    return ref::str(ref::call_static<void*>(mSceneGetNameInternal, handle));
}

/// Only digits (a job id rather than a job name).
bool is_numeric_only(const std::wstring& value)
{
    if (value.empty()) return false;
    for (wchar_t c : value)
        if (!std::iswdigit(c)) return false;
    return true;
}

/// FindContentList: the "Content" transform of a ScrollView below `root`.
void* find_content_list(void* root)
{
    void* content = unity::find_descendant(root, L"Content");
    if (!content) return nullptr;
    void* parent = unity::parent(content);
    if (!parent) return nullptr;
    if (unity::name(parent) == L"Viewport") return content;
    void* grand = unity::parent(parent);
    if (grand && unity::name(grand) == L"Scroll View") return content;
    return nullptr;
}

/// TextUtils.ForEachTextInChildren(parent, cb, includeInactive: true): every UI.Text on the
/// descendants of `parent` (not on `parent` itself), depth first.
template <typename F>
void for_each_text_in_children(void* parent, F&& fn, int depth = 0)
{
    if (!parent || depth > 32) return;
    int n = unity::child_count(parent);
    for (int i = 0; i < n; ++i) {
        void* child = unity::child(parent, i);
        if (!child) continue;
        if (void* t = unity::get_component(child, unity::text_class())) fn(t);
        for_each_text_in_children(child, fn, depth + 1);
    }
}

/// ReadFromTextComponents: pick name, job, level, HP and MP out of the slot's texts by the
/// names of their objects.
std::wstring read_character_information(void* slot)
{
    std::wstring character_name, job_name, level, current_hp, max_hp, current_mp, max_mp;

    for_each_text_in_children(slot, [&](void* t) {
        std::wstring content = displayed::text(t);
        if (content.empty()) return;
        std::wstring n = text::lower(unity::name(t));
        auto has = [&](const wchar_t* part) { return n.find(part) != std::wstring::npos; };

        // Character name - excluding job names, area names, etc.
        if ((has(L"name") && !has(L"job") && !has(L"area") && !has(L"floor") && !has(L"slot")) ||
            n == L"nametext" || n == L"name_text") {
            // Skip labels and very short text.
            if (content.size() > 1 && content.find(L':') == std::wstring::npos &&
                content != L"HP" && content != L"MP" && content != L"Lv")
                character_name = content;
        }
        // Job/class name (FF1: Fighter, Thief, Black Mage, White Mage, ...).
        else if ((has(L"job") || has(L"class")) && !has(L"label") && !has(L"id") &&
                 !has(L"level") && !has(L"lv")) {
            // Numeric-only content is likely a job id, not a job name.
            if (!is_numeric_only(content)) job_name = content;
        }
        // Level.
        else if ((has(L"level") || has(L"lv")) && !has(L"label") && !has(L"fixed")) {
            if (content != L"Lv" && content != L"Level" && content != L"LV") level = content;
        }
        // HP values.
        else if (has(L"hp") && !has(L"label")) {
            if (has(L"current") || has(L"now")) current_hp = content;
            else if (has(L"max")) max_hp = content;
        }
        // MP values.
        else if (has(L"mp") && !has(L"label")) {
            if (has(L"current") || has(L"now")) current_mp = content;
            else if (has(L"max")) max_mp = content;
        }
    });

    // "Name, Job, Level X, HP current/max, MP current/max"
    //
    // Two rules the FF1 reader did not need (FF2 test, 2026-10-10):
    //  * a job text that repeats the character's name is not a job (FF2 has no jobs; its slot's
    //    job text carries the name again, which read as "Firion, Firion, ...");
    //  * MP only in the games that show MP (game::flags().has_mp). FF1 and FF3 use spell charges;
    //    their slots still carry MP texts, which are never displayed.
    std::wstring announcement;
    if (!character_name.empty()) announcement = character_name;
    if (text::clean(job_name) == text::clean(character_name)) job_name.clear();
    // FF2 has no jobs: nothing from its slot's job text is ever a job (user, 2026-10-10: the
    // name was still read twice from it after the equality rule above).
    if (!game::flags().reads_job_slot) job_name.clear();
    if (!game::flags().has_mp) {
        current_mp.clear();
        max_mp.clear();
    }
    if (!game::flags().has_levels) level.clear();   // FF2: no character levels (user, 2026-10-10)
    if (!job_name.empty()) {
        if (!announcement.empty()) announcement += L", " + job_name;
        else announcement = job_name;
    }
    if (!level.empty()) {
        if (!announcement.empty()) announcement += L", " + TF(L"Level {0}", {level});
        else announcement = TF(L"Level {0}", {level});
    }
    if (!current_hp.empty()) {
        if (!max_hp.empty()) announcement += L", " + TF(L"HP {0}/{1}", {current_hp, max_hp});
        else announcement += L", " + TF(L"HP {0}", {current_hp});
    }
    if (!current_mp.empty()) {
        if (!max_mp.empty()) announcement += L", " + TF(L"MP {0}/{1}", {current_mp, max_mp});
        else announcement += L", " + TF(L"MP {0}", {current_mp});
    }
    return announcement;
}

/// The generic reader's call (cursor_event.h): the cursor being read is cursor::current(), and
/// the C# `out int count` goes to current().out_count.
std::wstring read_from_cursor_event()
{
    const cursor::CursorEvent& ev = cursor::current();
    ev.out_count = -1;
    if (!ev.cursor || !unity::alive(ev.cursor)) return {};
    void* t = unity::alive(ev.transform) ? ev.transform : unity::transform(ev.cursor);
    int count = -1;
    std::wstring result = try_read_character_selection(t, ev.index, count);
    ev.out_count = count;
    return result;
}

int install()
{
    queries::define_text("CharacterSelectionReader.TryReadCharacterSelection", [] { return read_from_cursor_event(); });
    return 0;
}

core::Install s_install{"Menus.CharacterSelection", &install, 120};

} // namespace

std::wstring try_read_character_selection(void* cursor_transform, int cursor_index, int& count)
{
    count = -1;
    if (!cursor_transform) return {};

    // Only read character data in a menu or a battle: never during a game load.
    std::wstring scene = active_scene_name();
    bool battle_scene = scene.find(L"Battle") != std::wstring::npos;
    bool menu_open = detail::menu_manager_open();
    if (!battle_scene && !menu_open) return {};

    // Walk up the hierarchy looking for character selection structures.
    void* current = cursor_transform;
    for (int depth = 0; current && depth < kMaxDepth; ++depth) {
        std::wstring lower = text::lower(unity::name(current));
        auto has = [&](const wchar_t* part) { return lower.find(part) != std::wstring::npos; };

        if (has(L"character") || has(L"chara") || has(L"status") || has(L"formation") ||
            has(L"party") || has(L"member")) {
            void* content_list = find_content_list(current);
            if (content_list && cursor_index >= 0 && cursor_index < unity::child_count(content_list)) {
                count = unity::child_count(content_list);
                std::wstring info = read_character_information(unity::child(content_list, cursor_index));
                if (!info.empty()) return info;
            }
        }

        // Directly on a character info element.
        if (has(L"info_content") || has(L"status_info") || has(L"chara_status")) {
            std::wstring info = read_character_information(current);
            if (!info.empty()) return info;
        }

        current = unity::parent(current);
    }
    return {};
}

} // namespace ffpr::menus
