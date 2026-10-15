// Ports FF1 Core/EntityNavigationManager.cs (EnsureFieldContext) and Field/FilterContext.cs
// (PlayerPosition = FieldPlayer.transform.localPosition) for the fieldnav area.
#include <cmath>
#include "fieldnav_util.h"
#include "../core.h"
#include "../modtext.h"
#include "../refs.h"
#include "../say.h"
#include "../field/field_api.h"
#include "../field/object_cache.h"

namespace ffpr::fieldnav {
namespace {
// Il2Cpp.FieldMap: the field sub-scene (global namespace in all five games).
ref::Class kFieldMap{"", "FieldMap"};
}

bool player_local_position(unity::Vec3& out)
{
    out = unity::Vec3{};
    void* player = ::field::player();
    if (!player) return false;
    void* tr = unity::transform(player);
    if (!tr) return false;
    out = unity::local_position(tr);
    return true;
}

float distance(unity::Vec3 a, unity::Vec3 b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;
    float dz = a.z - b.z;
    return static_cast<float>(std::sqrt(static_cast<double>(dx * dx + dy * dy + dz * dz)));
}

bool is_zero(unity::Vec3 v)
{
    // Vector3.operator== : sqrMagnitude of the difference < 9.99999944E-11f
    return (v.x * v.x + v.y * v.y + v.z * v.z) < 9.99999944E-11f;
}

bool ensure_field_context()
{
    if (queries::is("GameStatePatches.IsInputTransitionBlocked")) return false;
    if (queries::is("BattleStateHelper.IsInBattle") || !queries::is("GameStatePatches.IsFieldNavigationReady")) return false;
    if (menu_state::any_active())
        return false;

    void* field_map = field::cache::get(kFieldMap);
    if (!field_map || !unity::active_in_hierarchy(field_map)) {
        ffpr::say(T(L"Not on map"));
        return false;
    }

    if (!::field::player()) {
        ffpr::say(T(L"Not on map"));
        return false;
    }
    return true;
}

} // namespace ffpr::fieldnav
