#pragma once
#include <functional>

namespace ffpr::battle::rendered {
// Native creation events supply only the renderer and displayed event context.
// Numeric values are decoded after the actual glyphs become visible.
void hit_count(void* renderer);
void damage(void* renderer, bool miss, std::function<void(int)> speak);
}
