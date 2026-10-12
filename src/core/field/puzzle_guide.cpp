// Guided puzzle solutions (user decision, 2026-10-10: "guided puzzle solutions with
// step-by-step directions"). A puzzle is a list of steps authored per game in
// data/<game>/puzzles.json (embedded as PUZZLES_FFN): each step is a destination on some
// sub-map (a switch to step on, an object to examine from beside it, a floor to reach) with a
// short instruction. The guide speaks the current step with live directions from the player's
// position, across maps when needed (baked_nav::find_across: exits and script warps), and
// advances when the player reaches the step's cell (or stands beside an examined object).
//
// Keys: Shift+G starts the guide for this map or repeats the current step; Ctrl+G stops it.
// Pad: mod mode + RB / LB. Events: a map change re-announces the step after the map name;
// each movement input (FieldPlayerKeyController.OnTouchPadCallback, an input event) checks
// the arrival. No polling.
//
// puzzles.json:
//   {"puzzles": [{"id": "citadel_of_trials", "maps": [93, 94, 95],
//                 "steps": [{"map": 95, "x": 29, "y": 29, "layer": 0, "examine": false,
//                            "text": "Go to {0}", "arg": "map:95"}]}]}
// "text" is a mod-text key; "arg" fills {0}: "map:<id>" = that map's name through the game,
// anything else = the literal. Map ids are the map master's (tools/mapdump/<game>_master/map.txt).
#include <string>
#include <vector>
#include "baked_nav.h"
#include "field_api.h"
#include "field_internal.h"
#include "map_names.h"
#include "../core.h"
#include "../frame.h"
#include "../hooks.h"
#include "../json.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"

namespace field::puzzle_guide {
namespace {

struct Step {
    int map_id = 0;
    baked_route::Cell cell;
    bool examine = false;
    std::wstring text;   // mod-text key
    std::wstring arg;    // "map:<id>" or a literal
};

struct Puzzle {
    std::wstring id;
    std::vector<int> maps;
    std::vector<Step> steps;
};

std::vector<Puzzle>* g_puzzles = nullptr;
bool g_loaded = false;
int g_active = -1;         // index into g_puzzles
int g_step = 0;
int g_arrived_step = -1;   // the step whose arrival was already announced

void load()
{
    if (g_loaded || !data::game_data_ready()) return;
    g_loaded = true;
    g_puzzles = new std::vector<Puzzle>();
    const json::Value& root = data::game_json("puzzles");
    const json::Value* list = root.get(L"puzzles");
    if (!list || list->type != json::Value::Type::Array) {
        LOGF("[PuzzleGuide] no puzzles for this game");
        return;
    }
    for (const json::Value& p : list->arr) {
        Puzzle pz;
        if (const json::Value* v = p.get(L"id")) pz.id = v->s;
        if (const json::Value* v = p.get(L"maps"))
            for (const json::Value& m : v->arr) pz.maps.push_back(static_cast<int>(m.n));
        if (const json::Value* v = p.get(L"steps")) {
            for (const json::Value& s : v->arr) {
                Step st;
                if (const json::Value* x = s.get(L"map")) st.map_id = static_cast<int>(x->n);
                if (const json::Value* x = s.get(L"x")) st.cell.x = static_cast<int>(x->n);
                if (const json::Value* x = s.get(L"y")) st.cell.y = static_cast<int>(x->n);
                if (const json::Value* x = s.get(L"layer")) st.cell.layer = static_cast<int>(x->n);
                if (const json::Value* x = s.get(L"examine")) st.examine = x->b;
                if (const json::Value* x = s.get(L"text")) st.text = x->s;
                if (const json::Value* x = s.get(L"arg")) st.arg = x->s;
                if (st.map_id > 0 && !st.text.empty()) pz.steps.push_back(st);
            }
        }
        if (!pz.steps.empty()) g_puzzles->push_back(std::move(pz));
    }
    LOGF("[PuzzleGuide] %d puzzle guide(s) loaded", static_cast<int>(g_puzzles->size()));
}

const Puzzle* active() { return g_active >= 0 && g_active < static_cast<int>(g_puzzles->size()) ? &(*g_puzzles)[static_cast<size_t>(g_active)] : nullptr; }

std::wstring resolve_arg(const std::wstring& arg)
{
    if (text::starts_with(arg, L"map:")) {
        int id = 0;
        for (size_t i = 4; i < arg.size(); ++i)
            if (arg[i] >= L'0' && arg[i] <= L'9') id = id * 10 + (arg[i] - L'0');
        std::wstring name = baked_nav::map_name_of(id);
        return name.empty() ? TF(L"Map {0}", {text::num(id)}) : name;
    }
    return arg;
}

std::wstring step_text(const Step& s) { return s.arg.empty() ? T(s.text) : TF(s.text, {resolve_arg(s.arg)}); }

std::wstring puzzle_name(const Puzzle& p)
{
    const int id = p.maps.empty() ? p.steps.front().map_id : p.maps.front();
    std::wstring name = baked_nav::map_name_of(id);
    return name.empty() ? p.id : name;
}

/// "Step 2 of 4: Step on switch 2. North 3, East 1."
void announce_step(bool interrupt)
{
    const Puzzle* p = active();
    if (!p) return;
    if (g_step >= static_cast<int>(p->steps.size())) return;
    const Step& s = p->steps[static_cast<size_t>(g_step)];
    std::wstring line = TF(L"Step {0} of {1}: {2}", {text::num(g_step + 1), text::num(static_cast<long long>(p->steps.size())), step_text(s)});

    baked_nav::MapGoal goal;
    goal.map_id = s.map_id;
    goal.cell = s.cell;
    goal.examine = s.examine;
    const int step = g_step;
    baked_nav::request_route(goal, TargetKind::Infer, L"", [p, step, line = std::move(line), interrupt](PathInfo info) mutable {
        if (active() != p || g_step != step) return;
        if (info.success) {
            if (!info.description.empty() && info.description != T(L"No movement needed")) line += L". " + info.description;
        } else line += L". " + (!info.reason.empty() ? info.reason : T(L"No path to the next step"));
        ffpr::say(line, interrupt);
    }, false);
}

void start_or_repeat()
{
    load();
    if (!g_puzzles) { ffpr::say(T(L"Navigation data is loading")); return; }
    if (!active()) {
        const int map_id = current_map_id();
        for (size_t i = 0; i < g_puzzles->size(); ++i) {
            const Puzzle& p = (*g_puzzles)[i];
            bool here = false;
            for (int m : p.maps) here = here || m == map_id;
            for (const Step& s : p.steps) here = here || s.map_id == map_id;
            if (!here) continue;
            g_active = static_cast<int>(i);
            g_step = 0;
            g_arrived_step = -1;
            break;
        }
        if (!active()) {
            ffpr::say(T(L"No puzzle guide for this map"), true);
            return;
        }
        ffpr::say(TF(L"Puzzle guide: {0}", {puzzle_name(*active())}), true);
        announce_step(false);
        return;
    }
    announce_step(true);
}

void stop()
{
    if (!active()) return;
    g_active = -1;
    g_step = 0;
    g_arrived_step = -1;
    ffpr::say(T(L"Puzzle guide stopped"), true);
}

/// Each movement input: has the player reached the current step?
void on_move()
{
    const Puzzle* p = active();
    if (!p || g_step >= static_cast<int>(p->steps.size()) || g_arrived_step == g_step) return;
    const Step& s = p->steps[static_cast<size_t>(g_step)];
    if (current_map_id() != s.map_id) return;
    baked_route::Cell at;
    if (!baked_nav::player_cell(map_handle(), player(), at)) return;
    const int dx = at.x - s.cell.x, dy = at.y - s.cell.y;
    const bool on_cell = dx == 0 && dy == 0;
    const bool beside = (dx == 0 && (dy == 1 || dy == -1)) || (dy == 0 && (dx == 1 || dx == -1));
    if (!(on_cell || (s.examine && beside))) return;

    g_arrived_step = g_step;
    if (g_step + 1 >= static_cast<int>(p->steps.size())) {
        ffpr::say(T(L"Puzzle guide finished"), true);
        g_active = -1;
        g_step = 0;
        g_arrived_step = -1;
        return;
    }
    ffpr::say(T(L"Step done"), true);
    ++g_step;
    announce_step(false);
}

void on_map_changed()
{
    if (!active()) return;
    // After the map name ("Entering X"): the step with directions from the new position.
    frame::after_seconds(0.8, [] { if (active()) announce_step(false); }, "PuzzleGuide.MapChanged");
}

int install()
{
    actions::define("PuzzleGuide.Next", [] { hooks::guarded("PuzzleGuide.Next", [] { start_or_repeat(); }); });
    actions::define("PuzzleGuide.Stop", [] { hooks::guarded("PuzzleGuide.Stop", [] { stop(); }); });
    actions::define("PuzzleGuide.OnMove", [] { hooks::guarded("PuzzleGuide.OnMove", [] { on_move(); }); });
    queries::define("PuzzleGuide.IsActive", [] { return active() != nullptr; });
    events::on(events::Kind::MapChanged, [] { hooks::guarded("PuzzleGuide.MapChanged", [] { on_map_changed(); }); });
    return 0;
}
core::Install s_install{"PuzzleGuide", &install, 130};

} // namespace
} // namespace field::puzzle_guide
