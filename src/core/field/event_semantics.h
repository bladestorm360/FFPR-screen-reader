#pragma once
#include <string>
#include <string_view>
#include <cwctype>

namespace field::event_semantics {
// Reviewed developer identifiers. A message, battle, warp, generic actor fall or
// scene containing a broken bridge is not evidence of an environmental trap.
inline bool trap_identifier(std::wstring_view raw) {
    std::wstring name(raw);
    for (auto& c : name) c = static_cast<wchar_t>(std::towlower(c));
    for (const auto* token : {L"落とし穴",L"落し穴",L"穴に落ちる",L"地割れに落ちる",
            L"pitfall",L"trapdoor",L"floor_break",L"floorbreak",L"bridge_break",L"bridgebreak",
            L"floor_crack",L"crack_floor",L"cracked_floor",L"position_reset",L"reset_position",L"room_reset"})
        if (name.find(token) != name.npos) return true;
    auto body = std::wstring_view(name);
    if (body.starts_with(L"sc_") || body.starts_with(L"ev_")) {
        const auto colon = body.find_first_of(L":：");
        if (colon != body.npos) body.remove_prefix(colon+1);
    }
    if (body.starts_with(L"落ちる")) {
        body.remove_prefix(3);
        for (const auto c : body)
            if (!((c >= L'0' && c <= L'9') || (c >= L'０' && c <= L'９') ||
                (c >= 0x2460 && c <= 0x2473) || (c >= 0x3251 && c <= 0x325f) ||
                (c >= 0x32b1 && c <= 0x32bf))) return false;
        return true;
    }
    if (body.starts_with(L"底なし沼に落ち") || body.starts_with(L"滝に押し戻される")) return true;
    // FF5's floor-labelled falls and FF6's room/outdoor chutes relocate the
    // player. Keep narrative falls (Cid/Terra etc.) and progress dives separate.
    if ((body.starts_with(L"map_") || (!body.empty() && body[0] >= L'0' && body[0] <= L'9')) &&
        body.find(L"に落下") != body.npos) return true;
    return body == L"屋外に落っこちる" || body == L"部屋に落っこちる" ||
        body == L"crack" || body == L"fall" || body.starts_with(L"fall_") ||
        body == L"collapsing_floor" || body == L"collapsing_bridge";
}
}
