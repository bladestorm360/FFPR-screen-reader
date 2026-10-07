// Ports FF1 Field/NavigableEntity.cs (NavigableEntity and its TreasureChest, NPC, MapExit,
// SavePoint, Event, ScriptTrigger and Vehicle subclasses as one tagged struct).
//
// Vehicle names are game content, so each game keeps its own table (from that game's mod:
// FF1 NavigableEntity.cs, FF2 NavigableEntity.cs, FF3 NavigableEntity.cs, FF4 VehicleEntity.cs,
// FF5 NavigableEntity.cs, FF6 Field/NavigableEntity.cs). FF4, FF5 and FF6 prefer the vehicle's
// own message id.
#include <cmath>
#include <cstdio>
#include <cwctype>
#include "field_internal.h"
#include "../game.h"
#include "../modtext.h"
#include "../text.h"

namespace field {
namespace {

constexpr float kRad2Deg = 57.29578f;

/// .NET "F<n>" formatting: the exact value rounded half away from zero.
std::wstring fixed(float v, int decimals)
{
    double scale = decimals == 0 ? 1.0 : std::pow(10.0, decimals);
    double rounded = std::round(static_cast<double>(v) * scale) / scale;
    wchar_t buf[64];
    swprintf(buf, 64, L"%.*f", decimals, rounded);
    return buf;
}

std::wstring capitalize_first(std::wstring s)
{
    if (!s.empty() && std::iswlower(s[0])) s[0] = static_cast<wchar_t>(std::towupper(s[0]));
    return s;
}

std::wstring vehicle_name_ff1(int id)
{
    switch (id) {
    case 1: return T(L"Player");
    case 2: return T(L"Ship");
    case 3: return T(L"Airship");
    case 5: return T(L"Canoe");
    case 7: return T(L"LowFlying Airship");
    default: return TF(L"Vehicle {0}", {text::num(id)});
    }
}

/// FF2: null for anything that is not a vehicle (the caller falls back to the scanned name).
std::wstring vehicle_name_ff2(int id)
{
    switch (id) {
    case 2: return T(L"Ship");
    case 3:
    case 7:
    case 8: return T(L"Airship");
    case 5: return T(L"Canoe");
    case 6: return T(L"Submarine");
    case 9:
    case 10: return T(L"Chocobo");
    default: return L"";
    }
}

std::wstring vehicle_name_ff3(int id)
{
    switch (id) {
    case 1: return T(L"Player");
    case 2: return T(L"Ship");
    case 3:
    case 7:
    case 8: return T(L"Airship");
    case 6: return T(L"Submarine");
    case 0:
    case 4:
    case 5: return T(L"Vehicle");
    default: return TF(L"Vehicle {0}", {text::num(id)});
    }
}

std::wstring vehicle_name_ff4(int id)
{
    switch (id) {
    case 1: return T(L"Player");
    case 2: return T(L"Ship");
    case 3: return T(L"Enterprise");
    case 4: return T(L"Symbol");
    case 5: return T(L"Content");
    case 6: return T(L"Submarine");
    case 7: return T(L"Hovercraft");
    case 8: return T(L"Special Airship");
    case 9: return T(L"Yellow Chocobo");
    case 10: return T(L"Black Chocobo");
    case 11: return T(L"Boko");
    case 12: return T(L"Magical Armor");
    default: return TF(L"Vehicle {0}", {text::num(id)});
    }
}

std::wstring vehicle_name_ff5(int id)
{
    switch (id) {
    case 2: return T(L"ship");
    case 3: return T(L"airship");
    case 6: return T(L"submarine");
    case 7: return T(L"wind drake");
    case 9: return T(L"chocobo");
    case 10: return T(L"black chocobo");
    default: return T(L"Vehicle") + L" " + text::num(id);
    }
}

/// FF6 VehicleEntity.GetVehicleName's fallback table (MapConstants.TransportationType).
std::wstring vehicle_name_ff6(int id)
{
    switch (id) {
    case 2: return T(L"Ship");
    case 3: return T(L"Airship");
    case 6: return T(L"Submarine");
    case 7: return T(L"Low Flying Airship");
    case 8: return T(L"Special Airship");
    case 9: return T(L"Yellow Chocobo");
    case 10: return T(L"Black Chocobo");
    case 11: return T(L"Boko");
    case 12: return T(L"Magical Armor");
    default: return TF(L"Vehicle {0}", {text::num(id)});
    }
}

} // namespace

float vec_distance(unity::Vec3 a, unity::Vec3 b)
{
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return static_cast<float>(std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)));
}

/// NavigableEntity.GetDirection / FieldNavigationHelper.GetDirection.
std::wstring direction_name(unity::Vec3 from, unity::Vec3 to)
{
    float dx = to.x - from.x, dy = to.y - from.y;
    float angle = std::atan2(dx, dy) * kRad2Deg;
    if (angle < 0) angle += 360;
    if (angle >= 337.5 || angle < 22.5) return T(L"North");
    if (angle >= 22.5 && angle < 67.5) return T(L"Northeast");
    if (angle >= 67.5 && angle < 112.5) return T(L"East");
    if (angle >= 112.5 && angle < 157.5) return T(L"Southeast");
    if (angle >= 157.5 && angle < 202.5) return T(L"South");
    if (angle >= 202.5 && angle < 247.5) return T(L"Southwest");
    if (angle >= 247.5 && angle < 292.5) return T(L"West");
    if (angle >= 292.5 && angle < 337.5) return T(L"Northwest");
    return T(L"Unknown");
}

/// NavigableEntity.FormatSteps: "{steps:F1} step(s)", 16 units per step.
std::wstring format_steps(float distance)
{
    float steps = distance / 16.0f;
    std::wstring label = std::fabs(steps - 1.0f) < 0.1f ? T(L"step") : T(L"steps");
    return fixed(steps, 1) + L" " + label;
}

/// EventEntity.LocalizeTypeName: the raw English type is the internal key (ToLayerFilter
/// matches "ToLayer"); only the announcement is localized.
std::wstring localize_type_name(const std::wstring& type_name)
{
    if (type_name == L"Event") return T(L"Event");
    if (type_name == L"Interactive Object") return T(L"Interactive Object");
    if (type_name == L"Interactive") return T(L"Interactive");
    if (type_name == L"Warp Tile") return T(L"Warp Tile");
    if (type_name == L"ToLayer") return T(L"Layer transition");
    if (type_name == L"Hidden Trigger") return T(L"Hidden Trigger");
    return type_name;
}

/// VehicleEntity.GetVehicleName for the running game. Empty = no name (FF2's null).
std::wstring vehicle_name(int id, const std::wstring& message_id)
{
    switch (game::current()) {
    case game::Id::FF2: return vehicle_name_ff2(id);
    case game::Id::FF3: return vehicle_name_ff3(id);
    case game::Id::FF4:
    case game::Id::FF5:
    case game::Id::FF6: {
        if (!message_id.empty()) {
            std::wstring msg = get_message(message_id, true);
            if (!msg.empty()) return msg;
        }
        if (game::is(game::Id::FF6)) return vehicle_name_ff6(id);
        return game::is(game::Id::FF4) ? vehicle_name_ff4(id) : vehicle_name_ff5(id);
    }
    default: return vehicle_name_ff1(id);
    }
}

// --- Entity --------------------------------------------------------------------------------

Category Entity::category() const
{
    switch (kind) {
    case Kind::TreasureChest: return Category::Chests;
    case Kind::NPC: return Category::NPCs;
    case Kind::MapExit: return Category::MapExits;
    case Kind::Vehicle: return Category::Vehicles;
    default: return Category::Events;   // SavePoint, Event, ScriptTrigger
    }
}

int Entity::priority() const
{
    switch (kind) {
    case Kind::MapExit: return 1;
    case Kind::SavePoint: return 2;
    case Kind::TreasureChest: return 3;
    case Kind::NPC: return 4;
    case Kind::ScriptTrigger: return 5;
    case Kind::Event: return 8;
    case Kind::Vehicle: return 10;
    }
    return 8;
}

bool Entity::blocks_pathing() const
{
    return kind == Kind::TreasureChest || kind == Kind::NPC || kind == Kind::MapExit;
}

bool Entity::is_interactive() const
{
    return kind == Kind::TreasureChest ? !is_opened() : true;
}

/// IsAlive: the backing FieldEntity is still present (vehicle-only triggers stay listed while
/// the game hides them for another vehicle). A ScriptTrigger has no backing entity.
bool Entity::is_alive() const
{
    if (kind == Kind::ScriptTrigger) return true;
    if (!game || !live_entity(game)) return false;
    if (game_class && il2cpp::object_class(game) != game_class) return false;
    return entity_is_present(game);
}

/// Position: NPCs and vehicles read the live position (ReadLivePosition); the rest keep the
/// position from the scan.
unity::Vec3 Entity::position() const
{
    if ((kind == Kind::NPC || kind == Kind::Vehicle) && live_entity(game)) {
        if (void* tr = unity::transform(game)) return unity::local_position(tr);
    }
    return pos;
}

/// TreasureChestEntity.IsOpened: read live from the game entity.
bool Entity::is_opened() const
{
    if (live_entity(game)) return treasure_opened(game);
    return opened;
}

std::wstring Entity::display_name() const
{
    switch (kind) {
    case Kind::TreasureChest:
        return TF(is_opened() ? L"Opened {0}" : L"Unopened {0}", {name});
    case Kind::NPC:
        return shop ? name + L" (" + T(L"shop") + L")" : name;
    case Kind::Vehicle: {
        std::wstring n = vehicle_name(transport_id, message_id);
        if (n.empty()) return name;
        // FF5's names reuse lowercase keys that "On {0}" speaks mid-sentence; its entity list
        // capitalises the first letter for display only.
        return game::is(game::Id::FF5) ? capitalize_first(n) : n;
    }
    default:
        return name;
    }
}

std::wstring Entity::type_name() const
{
    switch (kind) {
    case Kind::TreasureChest: return T(L"Treasure Chest");
    case Kind::NPC: return T(L"NPC");
    case Kind::MapExit: return T(L"Map Exit");
    case Kind::SavePoint: return T(L"Save Point");
    case Kind::Vehicle: return T(L"Vehicle");
    default: return localize_type_name(event_type);   // Event, ScriptTrigger
    }
}

/// FormatDescription: "{name} ({steps} {direction}) - {type}".
std::wstring Entity::format_description(unity::Vec3 player_pos) const
{
    unity::Vec3 p = position();
    float distance = vec_distance(player_pos, p);
    std::wstring dir = direction_name(player_pos, p);
    return display_name() + L" (" + format_steps(distance) + L" " + dir + L") - " + type_name();
}

} // namespace field
