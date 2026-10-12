#pragma once
#include <string>
#include <optional>
#include <vector>

// Renderer-backed UI reads. Main thread, in a verified native screen context.
// Missing/hidden fields stay absent; never substitute character parameter data.
namespace displayed {
void* field(void* object, const char* name);
bool visible(void* component);
/// An active UI container. A disabled controller script does not hide its graphics.
bool container_visible(void* component);
bool world_visible(void* entity);
/// A rendered UI Image with a live sprite (for digit/icon reconstruction).
bool image_visible(void* component);
void* image_sprite(void* component);
std::wstring text(void* text_component);
std::wstring text_field(void* view, const char* name);
std::vector<std::wstring> texts(void* root);
/// Native before/after row, including rendered multiplier/percentage fields.
std::wstring comparison(void* view);
struct Comparison { std::wstring line; bool changed = false; };
Comparison comparison_reading(void* view);
std::wstring comparison_arrow(void* view);
std::wstring pair(void* view, const char* label, const char* value);
std::wstring vitals(void* view);
std::vector<std::wstring> vital_lines(void* view);
std::wstring bar(void* component);
std::wstring character(void* view);
/// Read only condition icons whose native sprite is currently rendered.
std::wstring conditions(void* content_or_controller);
std::vector<std::wstring> condition_lines(void* content_or_controller);
/// A displayed targeting row, respecting its native HP/MP/icon visibility.
std::wstring target(void* content);
std::wstring spell_row(void* content);
std::wstring skill_row(void* content);
std::wstring skill_name(void* content);
std::optional<int> skill_percentage(void* content);
std::wstring skill_change(void* content);
void clear_skill_changes();
std::optional<float> bar_fraction(void* component);
/// Decode only visible digit sprites. Unknown/hidden/incomplete numbers stay absent.
std::optional<int> sprite_number(const std::vector<void*>& images);
}
