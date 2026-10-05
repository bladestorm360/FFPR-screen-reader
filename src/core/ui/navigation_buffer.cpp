// Ports FF1 Core/NavigationBuffer.cs.
#include "navigation_buffer.h"
#include "text.h"

namespace ui {

NavigationBuffer::NavigationBuffer(std::vector<Entry> entries, std::vector<int> group_starts,
                                   std::vector<std::wstring> group_names)
    : entries_(std::move(entries)), group_starts_(std::move(group_starts)),
      group_names_(std::move(group_names)), index_(0)
{
}

std::pair<int, int> NavigationBuffer::current_group_position() const
{
    if (empty()) return {0, 0};
    if (group_starts_.empty()) return {index_, count()};

    int g_start = 0;
    int g_end = count();
    for (size_t i = 0; i < group_starts_.size(); ++i) {
        if (group_starts_[i] <= index_) {
            g_start = group_starts_[i];
            g_end = (i + 1 < group_starts_.size()) ? group_starts_[i + 1] : count();
        } else {
            break;
        }
    }
    return {index_ - g_start, g_end - g_start};
}

std::wstring NavigationBuffer::current()
{
    if (empty()) return {};
    if (index_ < 0 || index_ >= count()) index_ = 0;
    const Entry& e = entries_[static_cast<size_t>(index_)];
    return e ? e() : std::wstring();
}

std::wstring NavigationBuffer::next()
{
    if (empty()) return {};
    index_ = (index_ + 1) % count();
    return current();
}

std::wstring NavigationBuffer::previous()
{
    if (empty()) return {};
    --index_;
    if (index_ < 0) index_ = count() - 1;
    return current();
}

std::wstring NavigationBuffer::jump_top()
{
    if (empty()) return {};
    index_ = 0;
    return current();
}

std::wstring NavigationBuffer::jump_bottom()
{
    if (empty()) return {};
    index_ = count() - 1;
    return current();
}

std::wstring NavigationBuffer::next_group()
{
    if (empty()) return {};
    if (group_starts_.empty()) return current();

    int gi = 0;
    int target = group_starts_[0];
    for (size_t i = 0; i < group_starts_.size(); ++i) {
        if (group_starts_[i] > index_) {
            gi = static_cast<int>(i);
            target = group_starts_[i];
            break;
        }
    }
    index_ = target;
    return with_group(gi);
}

std::wstring NavigationBuffer::previous_group()
{
    if (empty()) return {};
    if (group_starts_.empty()) return current();

    int gi = static_cast<int>(group_starts_.size()) - 1;
    int target = group_starts_[static_cast<size_t>(gi)];
    for (int i = static_cast<int>(group_starts_.size()) - 1; i >= 0; --i) {
        if (group_starts_[static_cast<size_t>(i)] < index_) {
            gi = i;
            target = group_starts_[static_cast<size_t>(i)];
            break;
        }
    }
    index_ = target;
    return with_group(gi);
}

std::wstring NavigationBuffer::with_group(int group_index)
{
    std::wstring cur = current();
    if (group_index >= 0 && group_index < static_cast<int>(group_names_.size())) {
        const std::wstring& name = group_names_[static_cast<size_t>(group_index)];
        if (!text::is_blank(name)) return name + L". " + cur;
    }
    return cur;
}

} // namespace ui
