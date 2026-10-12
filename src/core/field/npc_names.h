#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace field::npc_names {

struct Label {
    std::wstring base;
    long long ordinal = -1;
};

Label parse(std::wstring_view name);

struct Entry {
    std::wstring* output = nullptr;
    Label label;
    int map_id = 0;
    int layer = 0;
    float x = 0;
    float y = 0;
};

void renumber(std::vector<Entry>& entries);

} // namespace field::npc_names
