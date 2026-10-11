#pragma once
#include <string>
#include <vector>

// Renderer-backed UI reads. Main thread, in a verified native screen context.
// Missing/hidden fields stay absent; never substitute character parameter data.
namespace displayed {
void* field(void* object, const char* name);
bool visible(void* component);
bool world_visible(void* entity);
/// A rendered UI Image with a live sprite (for digit/icon reconstruction).
bool image_visible(void* component);
void* image_sprite(void* component);
std::wstring text(void* text_component);
std::wstring text_field(void* view, const char* name);
std::vector<std::wstring> texts(void* root);
/// Native before/after row, including rendered multiplier/percentage fields.
std::wstring comparison(void* view);
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
}
