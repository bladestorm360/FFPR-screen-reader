#include "displayed_ui.h"
#include "modtext.h"
#include "text.h"
#include "unity.h"
#include "ui/preferences.h"
#include <cwchar>
#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <limits>

namespace displayed {
std::wstring equipment_icon(void* image)
{
    void* sprite = image_sprite(image);
    if (!sprite) return {};
    const auto name = unity::name(sprite);
    constexpr std::wstring_view prefix = L"UI_Common_Icon";
    if (name.size() != prefix.size() + 2 || name.compare(0, prefix.size(), prefix) != 0) return {};
    const auto tens = name[prefix.size()], units = name.back();
    if (tens < L'0' || tens > L'9' || units < L'0' || units > L'9') return {};
    const int id = 10 * (tens - L'0') + units - L'0';
    // Master icon.txt tags, native IconTextUtility's sprite format and extracted pixels.
    static const wchar_t* types[] = {L"", L"Swords", L"Greatswords", L"Ninja blades", L"Katanas",
        L"Daggers", L"Spears", L"Nunchaku", L"Axes", L"Hammers", L"Staves", L"Rods", L"Bows",
        L"Arrows", L"Throwing weapons", L"Shuriken", L"Harps", L"Bells", L"Claws", L"Whips",
        L"Flails", L"Brushes", L"Cards", L"Scrolls", L"Machines", L"Books", L"Clothing",
        L"Armor", L"Accessories", L"Shields", L"Helmets", L"Gloves", L"Hats"};
    return id > 0 && id < static_cast<int>(std::size(types)) ? T(types[id]) : std::wstring();
}
namespace {
// Parse only complete rendered numeric strings. Never recover hidden native parameters.
std::optional<long long> number(const std::wstring& value)
{
    if (value.empty()) return {};
    wchar_t* end = nullptr;
    const auto result = std::wcstoll(value.c_str(), &end, 10);
    return end != value.c_str() && *end == 0 && result >= -1000000000 && result <= 1000000000
        ? std::optional<long long>(result) : std::nullopt;
}
std::wstring delta(const std::wstring& label, const std::wstring& before,
                   const std::wstring& after, const std::wstring& unit)
{
    const auto first = number(before), last = number(after);
    if (!first || !last || *first == *last) return {};
    const auto difference = *last - *first;
    return TF(difference > 0 ? L"{0}: gained {1}" : L"{0}: lost {1}",
        {label, std::to_wstring(std::llabs(difference)) + unit});
}
}
Comparison comparison_reading(void* view)
{
    auto category = text_field(view, "categoryText");
    if (category.empty()) category = text_field(view, "paramNameText");
    auto before = text_field(view, "beforValueText");
    auto after = text_field(view, "afterValueText");
    const auto before_percent = text_field(view, "beforePercentValueText");
    const auto after_percent = text_field(view, "afterPercentValueText");
    // Compare the two rendered components independently. FF2 can increase only
    // the rate, only the multiplier, or both. An unchanged rate is not a gain.
    const bool changed = (!after.empty() && after != before) ||
        (!after_percent.empty() && after_percent != before_percent);
    if (ui::prefs::stat_delta_mode() == 1 && !before.empty() && !after.empty()) {
        const auto unit = text_field(view, "afterPercentSymbolText") + text_field(view, "afterValuePercentText");
        std::vector<std::wstring> gains;
        auto primary = delta(category, before, after, unit);
        if (!primary.empty()) gains.push_back(std::move(primary));
        auto multiple = text_field(view, "afterMultipleSymbolText");
        auto rate = delta(category, before_percent, after_percent, multiple.empty() ? text_field(view, "afterPercentSymbolText") : multiple);
        if (!rate.empty()) gains.push_back(std::move(rate));
        if (!gains.empty()) return {::text::join(gains, L", "), changed};
        if (!changed) return {category + (category.empty() ? L"" : L": ") + T(L"No change"), false};
        // Non-numeric localized values retain the actual displayed comparison.
    }
    if (!before_percent.empty()) before += text_field(view, "beforeMultipleSymbolText") +
        before_percent + text_field(view, "beforePercentSymbolText");
    if (!after_percent.empty()) after += text_field(view, "afterMultipleSymbolText") +
        after_percent + text_field(view, "afterPercentSymbolText");
    before += text_field(view, "beforValuePercentText");
    after += text_field(view, "afterValuePercentText");
    const auto arrow = comparison_arrow(view);
    if (before.empty() && after.empty()) {
        auto line = ::text::join(texts(view), L" ");
        return {line + (arrow.empty() ? L"" : L", " + arrow), false};
    }
    auto line = category + (category.empty() ? L"" : L" ") + before;
    if (!after.empty()) line += before.empty() ? after : TF(L" to {0}", {after});
    if (!arrow.empty()) line += L", " + arrow;
    return {std::move(line), changed};
}
std::wstring comparison(void* view) { return comparison_reading(view).line; }

std::optional<int> sprite_number(const std::vector<void*>& images)
{
    struct Digit { float x; int value; };
    std::vector<Digit> digits;
    constexpr std::wstring_view prefix = L"Battle_Common_DamageNumber_";
    for (void* image : images) {
        void* sprite = image_sprite(image);
        if (!sprite) continue; // hidden/unused number slots do not become zeroes
        const auto name = unity::name(sprite);
        // The ten serialized glyphs use these exact names in all six games.
        // HIT/HITS locale suffixes (1..12) are word images, never numeric digits.
        if (name.size() != prefix.size() + 1 || name.compare(0, prefix.size(), prefix) != 0 ||
            name.back() < L'0' || name.back() > L'9') return {};
        const float x = unity::position(image).x;
        if (!std::isfinite(x)) return {};
        digits.push_back({x, static_cast<int>(name.back() - L'0')});
    }
    if (digits.empty()) return {};
    // Damage prefab children are filled units-first; hit slots are filled in
    // string order. Actual rendered left-to-right placement covers both.
    std::sort(digits.begin(), digits.end(), [](const Digit& a, const Digit& b) { return a.x < b.x; });
    int value = 0;
    for (size_t i = 0; i < digits.size(); ++i) {
        if (i && digits[i - 1].x == digits[i].x) return {}; // layout not ready
        if (value > (std::numeric_limits<int>::max() - digits[i].value) / 10) return {};
        value = value * 10 + digits[i].value;
    }
    return value;
}
}
