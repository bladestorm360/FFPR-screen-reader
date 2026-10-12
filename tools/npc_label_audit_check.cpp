// check-sources: field/npc_names.cpp field/entity_translator.cpp json.cpp text.cpp
// Exhaustive offline NPC label audit using the real translator and numbering service.
// Each submap/story-group is a naming fixture, not a claim about live story visibility.
#include <algorithm>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>
#include <string>
#include <tuple>
#include <vector>
#include "field/npc_names.h"
#include "field/entity_translator.h"
#include "json.h"
#include "modtext.h"
#include "text.h"

namespace fixture {
json::Value dictionary;
std::wstring language = L"en";
}
namespace data {
bool game_data_ready() { return true; }
const json::Value& game_json(const char*) { return fixture::dictionary; }
}
namespace modtext {
const std::wstring& lang() { return fixture::language; }
std::wstring T(std::wstring_view key) { return std::wstring(key); }
std::wstring TF(std::wstring_view key, std::initializer_list<std::wstring> args) { return text::format(key, args); }
}
namespace field { std::wstring current_map_name() { return L"Offline NPC audit"; } }
namespace log_ { void write(const char*, ...) {} }

std::string read_file(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

struct Record {
    std::wstring raw, scope;
    int layer = 0;
    float x = 0, y = 0;
};

int main(int argc, char** argv)
{
    if (argc != 2) { std::puts("usage: npc_label_audit ffN"); return 1; }
    const std::string game = argv[1];
    if (!json::parse(read_file("data/" + game + "/translation.json"), fixture::dictionary)) {
        std::puts("FAIL dictionary load"); return 1;
    }
    field::prepare_entity_translations();
    std::ifstream objects("../tools/mapdump/" + game + "_objects.jsonl", std::ios::binary);
    if (!objects) { std::puts("FAIL object data load"); return 1; }
    std::vector<Record> records;
    std::string line;
    while (std::getline(objects, line)) {
        json::Value row;
        if (!json::parse(line, row)) { std::puts("FAIL object parse"); return 1; }
        const auto* type = row.get(L"type");
        if (!type || (type->n != 6 && type->n != 17)) continue;
        auto str = [&](const wchar_t* key) { const auto* p = row.get(key); return p ? p->s : std::wstring(); };
        auto num = [&](const wchar_t* key) { const auto* p = row.get(key); return p ? static_cast<float>(p->n) : 0.f; };
        const std::wstring layer = str(L"layer");
        records.push_back({str(L"name"), str(L"submap") + L"\n" + str(L"group"),
                           layer == L"BottomObject" ? 0 : layer == L"UpperObject" ? 2 : 1, num(L"tx"), num(L"ty")});
    }
    int failures = 0;
    size_t checked = 0, decorated = 0, duplicate_groups = 0;
    auto fail = [&](const char* why, const Record& r) {
        if (failures++ < 6) std::printf("FAIL %s %s %s\n", why, text::to_utf8(r.scope).c_str(), text::to_utf8(r.raw).c_str());
    };
    for (const wchar_t* lang : {L"en", L"ja", L"fr", L"it", L"de", L"es", L"ko", L"zht", L"zhc", L"ru", L"th", L"pt"}) {
        fixture::language = lang;
        std::map<std::wstring, int> scopes;
        std::vector<std::wstring> outputs(records.size());
        std::vector<field::npc_names::Entry> entries;
        for (size_t i = 0; i < records.size(); ++i) {
            const auto& r = records[i];
            // Live conversion uses a generic NPC fallback when the property is empty.
            std::wstring translated = field::translate_entity_name(r.raw);
            if (translated.empty()) translated = L"NPC";
            const auto label = field::npc_display_label(translated);
            const auto again = field::npc_names::parse(label.base);
            if (label.base.empty() || again.base != label.base || again.ordinal != -1)
                fail("residual decoration", r);
            const std::wstring lower = text::lower(label.base);
            if (lower.find(L":ev_e_") != std::wstring::npos || lower.find(L":sc_e_") != std::wstring::npos ||
                lower.find(L"：ev_e_") != std::wstring::npos || lower.find(L"：sc_e_") != std::wstring::npos)
                fail("residual script identifier", r);
            auto [it, inserted] = scopes.emplace(r.scope, static_cast<int>(scopes.size()) + 1);
            entries.push_back({&outputs[i], label, it->second, r.layer, r.x, r.y});
            ++checked;
            if (translated != label.base) ++decorated;
        }
        field::npc_names::renumber(entries);
        using Group = std::pair<int, std::wstring>;
        std::map<Group, std::vector<size_t>> groups;
        for (size_t i = 0; i < entries.size(); ++i) groups[{entries[i].map_id, entries[i].label.base}].push_back(i);
        for (const auto& [key, members] : groups) {
            if (members.size() == 1) {
                if (outputs[members[0]] != key.second) fail("unique NPC still numbered", records[members[0]]);
            } else {
                ++duplicate_groups;
                std::vector<std::wstring> actual, expected;
                for (size_t i : members) actual.push_back(outputs[i]);
                for (size_t i = 1; i <= members.size(); ++i) expected.push_back(key.second + std::to_wstring(i));
                std::sort(actual.begin(), actual.end()); std::sort(expected.begin(), expected.end());
                if (actual != expected) fail("nonsequential duplicate group", records[members[0]]);
            }
        }
        const auto first = outputs;
        std::reverse(entries.begin(), entries.end());
        field::npc_names::renumber(entries);
        // Exact position/ordinal ties have no distinct source identity in this dump fixture;
        // do not claim an order for those overlapping story records.
        std::map<std::tuple<int, std::wstring, long long, int, float, float>, int> ties;
        for (const auto& e : entries) ++ties[{e.map_id, e.label.base, e.label.ordinal, e.layer, e.x, e.y}];
        for (const auto& e : entries) {
            const size_t i = static_cast<size_t>(e.output - outputs.data());
            if (ties[{e.map_id, e.label.base, e.label.ordinal, e.layer, e.x, e.y}] == 1 && outputs[i] != first[i])
                fail("input order changed numbering", records[i]);
        }
    }
    std::printf("%s: %zu records, 12 languages, %zu labels, %zu decorations removed, %zu duplicate groups: %s\n",
                game.c_str(), records.size(), checked, decorated, duplicate_groups, failures ? "FAILED" : "PASS");
    return failures ? 1 : 0;
}
