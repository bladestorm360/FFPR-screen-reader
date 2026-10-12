#pragma once
#include <string>
#include <vector>

// Current displayed page/phase only. Consumers supply renderer-backed lines.
namespace ui::review {
enum class Domain { Results, Status, Information };
struct Options {
    Domain domain = Domain::Results;
    bool announce = true;
    bool defer_details = false;
    // Essential displayed totals are announced in either Autodetail state and
    // retained as individual review lines. Detailed rows are separate.
    std::vector<std::wstring> summary;
    bool include_headline = false;
    int restore_index = 0;
    std::vector<int> groups;
};
void show(void* owner, const std::wstring& headline, std::vector<std::wstring> lines, Options options = {});
void clear();
void replace_lines(std::vector<std::wstring> lines, int restore_index);
bool active();
bool owns(void* owner);
Domain domain();
int index();
void details();
void next();
void previous();
void first();
void last();
void current();
void next_group();
void previous_group();
}
