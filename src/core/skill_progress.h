#pragma once
#include <cstdint>
#include <optional>
#include <array>

// The native FF2 UI computes 1 - remaining / interval. Preserve its integer
// operands until speech rounding, including the full gauge at the level cap.
namespace displayed::skill_progress {
struct Value {
    int level = 0, remaining = 0, interval = 0;
    bool valid() const {
        return level > 0 && level <= 1000 && remaining >= 0 &&
            (capped() || (interval > 0 && interval <= 1000000 && remaining <= interval));
    }
    bool capped() const { return remaining == 0; }
    int numerator() const { return capped() ? 0 : interval - remaining; }
    int denominator() const { return capped() ? 1 : interval; }
    int percent() const {
        return capped() ? 100 : static_cast<int>((int64_t(numerator()) * 100 + denominator() - 1) / denominator());
    }
    float fill() const { return capped() ? 1.0f : 1.0f - float(remaining) / interval; }
};
// FF2 exp_table: type_id=1/group_id=1, verified against the native provider.
// Read-only local math; never invoke the game's LINQ/cache-building utilities.
inline constexpr std::array<int, 16> thresholds{0,20,45,70,100,135,175,220,270,330,400,480,570,660,760,860};
inline Value from_points(int points) {
    if (points < 0) return {};
    size_t i = 0;
    while (i + 1 < thresholds.size() && points >= thresholds[i + 1]) ++i;
    if (i + 1 == thresholds.size()) return {16, 0, 0};
    return {static_cast<int>(i + 1), thresholds[i + 1] - points, thresholds[i + 1] - thresholds[i]};
}
inline std::optional<int> gain(Value before, Value after) {
    if (!before.valid() || !after.valid()) return {};
    const int64_t denominator = int64_t(before.denominator()) * after.denominator();
    const int64_t numerator = int64_t(after.level - before.level) * 100 * denominator +
        int64_t(after.numerator()) * 100 * before.denominator() -
        int64_t(before.numerator()) * 100 * after.denominator();
    // A capped gauge is visually full, but is not an extra unearned level.
    if (numerator <= 0) return 0;
    return static_cast<int>((numerator + denominator - 1) / denominator);
}
}
