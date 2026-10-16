#pragma once
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace ui::review {
// A results-session-only exception to the no-content-deduplication rule, requested
// October 16. Only designated native notifications participate; totals and rows do not.
struct ResultNotificationHistory {
    std::set<std::wstring> messages;
    size_t characters = 0;
};
struct ResultNotificationSession {
    // Native owner only. Workers receive an immutable snapshot of this pointer.
    std::shared_ptr<const ResultNotificationHistory> history;
};
struct ResultNotificationSelection {
    std::vector<std::wstring> lines;
    std::shared_ptr<const ResultNotificationHistory> history;
};
inline ResultNotificationSelection select_result_notifications(
    const std::vector<std::wstring>& lines,
    const std::shared_ptr<const ResultNotificationHistory>& previous)
{
    // Worker only. Retention is finite; over-limit notifications still speak.
    constexpr size_t kMaxMessages = 1024, kMaxCharacters = 65536, kMaxLine = 4096;
    auto next = std::make_shared<ResultNotificationHistory>();
    if (previous) *next = *previous;
    ResultNotificationSelection result;
    for (const auto& line : lines) {
        if (line.empty() || (previous && previous->messages.contains(line))) continue;
        result.lines.push_back(line);
        // Equal rows in this first buffer remain distinct. Only earlier buffers
        // suppress repeats, never another row in this same presentation.
        if (line.size() <= kMaxLine && next->messages.size() < kMaxMessages &&
            line.size() <= kMaxCharacters - next->characters && next->messages.insert(line).second)
            next->characters += line.size();
    }
    result.history = std::move(next);
    return result;
}
}
