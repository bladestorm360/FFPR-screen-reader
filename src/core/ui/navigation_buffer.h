#pragma once
// Ports FF1 Core/NavigationBuffer.cs.
//
// Shared virtual-buffer navigation. Holds an ordered list of entries - each produced by a
// function so a feature can read live values (status stats) or return a pre-rendered string
// (bestiary, key help) - plus optional group boundaries. Owns all the index wrap/clamp and
// group-jump logic so the feature readers (status, bestiary, key help) do not duplicate it.
// Holds DATA only; the caller speaks the returned string.
//
// index() is public so a feature that keeps its position elsewhere (the status tracker) can
// sync it in and out around a navigation call. Group navigation is a no-op (returns the current
// entry) when no groups were supplied; group names are optional - when present, group jumps
// prefix "GroupName. " (bestiary), when absent they do not (status).
//
// Where the C# returned null (an empty buffer, or a null entry), these return an empty string.
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace ui {

class NavigationBuffer {
public:
    using Entry = std::function<std::wstring()>;

    NavigationBuffer() = default;
    /// `group_starts` empty = no groups; `group_names` empty = no group-name prefix.
    explicit NavigationBuffer(std::vector<Entry> entries, std::vector<int> group_starts = {},
                              std::vector<std::wstring> group_names = {});

    int index() const { return index_; }
    void set_index(int i) { index_ = i; }
    int count() const { return static_cast<int>(entries_.size()); }
    bool empty() const { return entries_.empty(); }

    /// Position of the current entry for an "(X of Y)" announcement. With groups: the index
    /// within the group and the group's size; without: (index, count) for the whole buffer.
    /// The caller passes the pair to ui::menu_position.
    std::pair<int, int> current_group_position() const;

    /// The current entry's text (empty if the buffer is empty). Clamps a stray index to 0.
    std::wstring current();
    /// Advance one entry (wraps to the top) and return its text.
    std::wstring next();
    /// Step back one entry (wraps to the bottom) and return its text.
    std::wstring previous();
    std::wstring jump_top();
    std::wstring jump_bottom();
    /// First entry of the next group (wraps to the first group). No groups: the current entry.
    std::wstring next_group();
    /// First entry of the previous group (wraps to the last group). No groups: the current entry.
    std::wstring previous_group();

private:
    std::wstring with_group(int group_index);

    std::vector<Entry> entries_;
    std::vector<int> group_starts_;
    std::vector<std::wstring> group_names_;
    int index_ = 0;
};

} // namespace ui
