#include <algorithm>
#include "review_buffer.h"
#include "navigation_buffer.h"
#include "preferences.h"
#include "../core.h"
#include "../displayed_ui.h"
#include "../say.h"
#include "../text.h"
#include "../ui_util.h"
#include "../input/input.h"

namespace ui::review {
namespace {
void* g_owner = nullptr;
NavigationBuffer g_buffer;
std::vector<std::wstring> g_lines;
std::wstring g_headline;
Options g_options;
Domain g_domain = Domain::Results;
void speak(std::wstring line) {
    if (!active() || line.empty()) return;
    ffpr::say(ui::menu_position(line, g_buffer.index(), g_buffer.count()), true);
}
int install() {
    queries::define("ReviewBuffer.IsActive", [] { return active(); });
    actions::define("ReviewBuffer.Details", [] { details(); });
    actions::define("ReviewBuffer.Next", [] { next(); });
    actions::define("ReviewBuffer.Previous", [] { previous(); });
    actions::define("ReviewBuffer.First", [] { first(); });
    actions::define("ReviewBuffer.Last", [] { last(); });
    using namespace input;
    add_binding(Key::DownArrow, Mod::None, Context::Results, "ReviewBuffer.Next", "Next displayed result line");
    add_binding(Key::UpArrow, Mod::None, Context::Results, "ReviewBuffer.Previous", "Previous displayed result line");
    add_binding(Key::LeftArrow, Mod::None, Context::Results, "ReviewBuffer.First", "First displayed result line");
    add_binding(Key::RightArrow, Mod::None, Context::Results, "ReviewBuffer.Last", "Last displayed result line");
    add_binding(Key::I, Mod::None, Context::Results, "ReviewBuffer.Details", "Read displayed results");
    events::on(events::Kind::BattleStart, [] { clear(); });
    events::on(events::Kind::BattleEnd, [] { clear(); });
    return 0;
}
core::Install s_install{"UI.ReviewBuffer", &install, 90};
}
bool active() { return displayed::visible(g_owner) && !g_buffer.empty(); }
bool owns(void* owner) { return owner && owner == g_owner; }
Domain domain() { return g_domain; }
int index() { return g_buffer.index(); }
void clear() { g_owner = nullptr; g_buffer = {}; g_lines.clear(); g_headline.clear(); g_options = {}; }
void replace_lines(std::vector<std::wstring> lines, int restore_index) {
    const auto headline = g_headline;
    Options options = g_options;
    options.announce = false; options.restore_index = restore_index;
    show(g_owner, headline, std::move(lines), options);
}
void show(void* owner, const std::wstring& headline, std::vector<std::wstring> lines, Options options) {
    clear();
    if (!displayed::visible(owner)) return;
    g_owner = owner;
    g_domain = options.domain;
    g_headline = headline;
    g_options = options;
    const auto details = text::join(lines, L". ");
    if (options.include_headline && !headline.empty()) g_lines.push_back(headline);
    for (const auto& line : options.summary) if (!line.empty()) g_lines.push_back(line);
    for (auto& line : lines) if (!line.empty()) g_lines.push_back(std::move(line));
    std::vector<NavigationBuffer::Entry> entries;
    for (const auto& line : g_lines) entries.push_back([line] { return line; });
    g_buffer = NavigationBuffer(std::move(entries), std::move(options.groups));
    if (!g_lines.empty()) g_buffer.set_index(std::max(0, std::min(options.restore_index, static_cast<int>(g_lines.size()) - 1)));
    if (options.announce) {
        auto announcement = headline;
        const auto summary = text::join(options.summary, L". ");
        if (!summary.empty()) announcement += (announcement.empty() ? L"" : L". ") + summary;
        if (!announcement.empty()) ffpr::say(announcement, true);
        if (prefs::auto_detail_enabled() && !options.defer_details && !details.empty()) ffpr::say(details, false);
    }
}
void details() {
    if (!active()) return;
    auto line = text::join(g_lines, L". ");
    if (!g_options.include_headline && !g_headline.empty()) line = g_headline + (line.empty() ? L"" : L". " + line);
    if (!line.empty()) ffpr::say(line, false);
}
void next() { if (active()) speak(g_buffer.next()); }
void previous() { if (active()) speak(g_buffer.previous()); }
void first() { if (active()) speak(g_buffer.jump_top()); }
void last() { if (active()) speak(g_buffer.jump_bottom()); }
void current() { if (active()) speak(g_buffer.current()); }
void next_group() { if (active()) speak(g_buffer.next_group()); }
void previous_group() { if (active()) speak(g_buffer.previous_group()); }
}
