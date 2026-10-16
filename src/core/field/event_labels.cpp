#include "event_labels.h"
#include "field_internal.h"
#include "../modtext.h"

namespace field {
std::wstring safe_event_label(void* entity, const std::wstring& type)
{
    if (type == L"ToLayer") return T(L"Layer transition");
    if (type == L"Warp Tile") return T(L"Warp Tile");
    void* property = entity_property(entity);
    const auto raw = property ? ref::rd_str(property, r::fPropName) : std::wstring();
    return safe_event_label(raw, type);
}
std::wstring safe_event_label(const std::wstring& raw, const std::wstring& type)
{
    if (type == L"ToLayer") return T(L"Layer transition");
    if (type == L"Warp Tile") return T(L"Warp Tile");
    const auto& labels = data::game_json("eventlabels");
    if (const auto* label = labels.get(raw); label && label->is_string() && !label->s.empty()) return T(label->s);
    // Unknown future identifiers never fall back to raw names or unrevealed dialogue.
    return type == L"Event" ? T(L"Event trigger") : T(L"Interactive object");
}
}
