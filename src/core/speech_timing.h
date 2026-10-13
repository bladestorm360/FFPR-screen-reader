#pragma once
namespace speech {
enum class Feature { None, EntityBrowse, EntityCategory, Directions, ModMenu };
// Owned timestamps only (QPC seconds, same clock as frame::now). Capture time is
// the oldest relevant ready snapshot; zero means unknown rather than fresh.
struct Timing {
    Feature feature = Feature::None;
    double requested_at = 0, captured_at = 0, enqueued_at = 0;
    bool first = false; // zero-initialized unused ring rows stay in BSS, not the DLL image
};
}
