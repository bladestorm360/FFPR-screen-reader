// Ports FF6 Patches/AirshipNavigationPatches.cs (heading and altitude speech),
// Utils/AirshipNavigationReader.cs (compass sector, altitude level), the airship half of
// Core/FFVI_ScreenReaderMod.cs (AnnounceAirshipOrCharacterStatus, AnnounceAirshipStatus: the H
// key) and MapUIManager_SwitchLandable_Patch.LastAnnouncementTime of
// Patches/VehicleLandingPatches.cs (the "recent landing" test the airship lines read).
//
// Events instead of the C#'s per-frame postfixes. The C# patched
// FieldPlayerKeyAirshipController.LateUpdateObserveInput (every frame on the airship) and
// UpdateFlightAltitudeAndFov (every frame the player flies) and compared the compass sector and
// the altitude level with the last ones spoken. In FF6's GameAssembly.dll those methods change
// the view only through the map handle: UpdateRotateAccelerator calls
// IMapAccessor.SetZAxisRotateBirdCamera(Get... + accelerator) and UpdateFlightAltitudeAndFov
// calls IMapAccessor.SetFlightAltitudeAndFieldOfViewRatio(...), each only while its accelerator
// is non-zero (offline disassembly, 2026-10-07). So this file hooks those two FieldController
// setters, the producers of the change (one RVA each in FF6's dump.cs, no direct callers).
// FieldPlayerKeyAirshipController.SwitchInputEnable(true) (its own override, one RVA) stands in
// for the C#'s first frame at the controls: a heading or altitude that changed while the player
// was not flying (boarding, takeoff, an event) is spoken then, as the C# did on its next frame.
//
// Gate: the C#'s own airship test (an active FieldPlayerKeyAirshipController with InputEnable,
// from AnnounceAirshipOrCharacterStatus), so camera moves the game makes while the player has no
// control (takeoff, landing, events) are not spoken; they are compared again when control
// returns. The last sector / level are the C#'s lastDirection / lastAltitudeLevel (state, not a
// text comparison), kept for the session as in the C# (its ResetState was never called).
//
// H key (keys.cpp binds it on the field): "Mod.AnnounceAirshipOrCharacterStatus" speaks the
// airship status when flying, else runs the core's "Mod.AnnounceCharacterStatus".
//
// The landing line itself is the core's (src/core/fieldnav/vehicle_landing.cpp, "Can land" when
// a vehicle enters a landable zone). This file adds a second SwitchLandable subscriber that only
// notes when the core spoke it, so a heading or altitude line within 0.5 s does not cut it off
// (the C#'s interrupt: !recentLanding).
#include <cmath>
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/frame.h"
#include "../../core/game.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"
#include "../../core/field/object_cache.h"
#include "../../core/fieldnav/move_state.h"

namespace ffpr::ff6 {
namespace {

namespace ms = ffpr::fieldnav::move_state;

constexpr uint8_t k6 = game::kFF6;

ref::Class  kFieldController{"Last.Map", "FieldController", k6};
ref::Method mSetZAxisRotateBirdCamera{kFieldController, "SetZAxisRotateBirdCamera", 1};
ref::Method mGetZAxisRotateBirdCamera{kFieldController, "GetZAxisRotateBirdCamera", 0};
ref::Method mSetFlightAltitudeAndFieldOfViewRatio{kFieldController, "SetFlightAltitudeAndFieldOfViewRatio", 1};
ref::Method mGetFlightAltitudeFieldOfViewRatio{kFieldController, "GetFlightAltitudeFieldOfViewRatio", 1};

ref::Class  kFieldMap{"", "FieldMap", k6};
ref::Field  fFieldController{kFieldMap, "fieldController"};

ref::Class  kFieldPlayerController{"Last.Map", "FieldPlayerController", k6};
ref::Field  fFieldPlayer{kFieldPlayerController, "fieldPlayer"};
ref::Field  fInputEnable{kFieldPlayerController, "<InputEnable>k__BackingField"};
ref::Class  kAirshipController{"Last.Map", "FieldPlayerKeyAirshipController", k6};
ref::Method mSwitchInputEnable{kAirshipController, "SwitchInputEnable", 1};

ref::Class  kMapUIManager{"Last.Map", "MapUIManager", k6};
ref::Method mSwitchLandable{kMapUIManager, "SwitchLandable", 1};

// The airship-controller search when nothing valid is cached, at most this often outside a key
// press (the setters run every frame while the view turns).
constexpr uint64_t kSearchIntervalFrames = 30;
// MapUIManager_SwitchLandable_Patch: a line within this time of "Can land" does not interrupt it.
constexpr double kRecentLandingSeconds = 0.5;

int g_last_direction = -1;   // compass sector last spoken (C# lastDirection; -1 = "")
int g_last_altitude = -1;    // altitude level last spoken (C# lastAltitudeLevel; -1 = "")

void* g_airship = nullptr;   // the FieldPlayerKeyAirshipController (re-checked on every use)
bool g_searched = false;
uint64_t g_search_frame = 0;

bool g_last_landable = false;
double g_last_landing_time = -1.0e9;

// --- AirshipNavigationReader -----------------------------------------------------------------

/// GetCompassDirection as a sector: 0 North, 1 Northwest, 2 West, 3 Southwest, 4 South,
/// 5 Southeast, 6 East, 7 Northeast (the game's rotation has east and west reversed).
int compass_sector(float rotation)
{
    float normalized = std::fmod(std::fmod(rotation, 360.0f) + 360.0f, 360.0f);
    if (normalized >= 337.5f || normalized < 22.5f) return 0;
    if (normalized < 67.5f) return 1;
    if (normalized < 112.5f) return 2;
    if (normalized < 157.5f) return 3;
    if (normalized < 202.5f) return 4;
    if (normalized < 247.5f) return 5;
    if (normalized < 292.5f) return 6;
    return 7;
}

std::wstring compass_name(int sector)
{
    switch (sector) {
    case 0: return T(L"North");
    case 1: return T(L"Northwest");
    case 2: return T(L"West");
    case 3: return T(L"Southwest");
    case 4: return T(L"South");
    case 5: return T(L"Southeast");
    case 6: return T(L"East");
    default: return T(L"Northeast");
    }
}

/// GetAltitudeDescription as a level (the C#'s GetAltitudeIndex order): 0 ground .. 4 maximum.
int altitude_level(float ratio)
{
    if (ratio <= 0.0f) return 0;
    if (ratio < 0.33f) return 1;
    if (ratio < 0.67f) return 2;
    if (ratio < 1.0f) return 3;
    return 4;
}

std::wstring altitude_name(int level)
{
    switch (level) {
    case 0: return T(L"Ground level");
    case 1: return T(L"Low altitude");
    case 2: return T(L"Cruising altitude");
    case 3: return T(L"High altitude");
    default: return T(L"Maximum altitude");
    }
}

// --- the airship controller ------------------------------------------------------------------

bool in_control(void* controller)
{
    return controller && unity::active_in_hierarchy(controller) && ref::rd<bool>(controller, fInputEnable);
}

/// The C#'s test: an active FieldPlayerKeyAirshipController whose InputEnable is set, or null.
/// `on_demand` (a key press) searches at once; the hooks search at most every 30 frames.
void* airship_in_control(bool on_demand)
{
    if (g_airship && !(unity::alive(g_airship) && kAirshipController.is(g_airship))) g_airship = nullptr;
    if (g_airship && in_control(g_airship)) return g_airship;
    const uint64_t f = frame::count();
    if (!on_demand && g_searched && f - g_search_frame < kSearchIntervalFrames) return nullptr;
    g_searched = true;
    g_search_frame = f;
    for (void* c : unity::find_objects(kAirshipController)) {
        if (in_control(c)) {
            g_airship = c;
            return c;
        }
    }
    return nullptr;
}

void* field_controller()
{
    void* map = field::cache::get_or_refresh(kFieldMap);
    return map ? ref::rd<void*>(map, fFieldController) : nullptr;
}

bool recent_landing() { return frame::now() - g_last_landing_time < kRecentLandingSeconds; }

// --- AirshipNavigationPatches ----------------------------------------------------------------

/// The LateUpdateObserveInput postfix: "Facing {0}" when the compass sector changed.
/// True when it spoke.
bool check_heading(void* fc, bool interrupt)
{
    int sector = compass_sector(ref::call<float>(mGetZAxisRotateBirdCamera, fc));
    if (sector == g_last_direction) return false;
    if (!airship_in_control(false)) return false;
    g_last_direction = sector;
    std::wstring name = compass_name(sector);
    LOGF("[Airship] Facing: %ls", name.c_str());
    ffpr::say(TF(L"Facing {0}", {name}), interrupt && !recent_landing());
    return true;
}

/// The UpdateFlightAltitudeAndFov postfix: the altitude level when it changed, with "Rising. "
/// or "Descending. " after the first one. True when it spoke.
bool check_altitude(void* fc)
{
    int level = altitude_level(ref::call<float>(mGetFlightAltitudeFieldOfViewRatio, fc, true));
    if (level == g_last_altitude) return false;
    if (!airship_in_control(false)) return false;
    std::wstring change;
    if (g_last_altitude >= 0) {
        if (level > g_last_altitude) change = T(L"Rising. ");
        else if (level < g_last_altitude) change = T(L"Descending. ");
    }
    g_last_altitude = level;
    std::wstring announcement = change + altitude_name(level);
    LOGF("[Airship] Altitude: %ls", announcement.c_str());
    ffpr::say(announcement, !recent_landing());
    return true;
}

using SetFloat_t = void (*)(void* self, float value, const void* mi);
SetFloat_t o_SetZAxisRotateBirdCamera = nullptr;
SetFloat_t o_SetFlightAltitude = nullptr;

void d_SetZAxisRotateBirdCamera(void* self, float deg_angle, const void* mi)
{
    o_SetZAxisRotateBirdCamera(self, deg_angle, mi);
    hooks::guarded("FF6.Airship.SetZAxisRotateBirdCamera", [&] { check_heading(self, true); });
}

void d_SetFlightAltitude(void* self, float ratio, const void* mi)
{
    o_SetFlightAltitude(self, ratio, mi);
    hooks::guarded("FF6.Airship.SetFlightAltitudeAndFieldOfViewRatio", [&] { check_altitude(self); });
}

using SwitchInputEnable_t = void (*)(void* self, bool enable, const void* mi);
SwitchInputEnable_t o_SwitchInputEnable = nullptr;

/// The player takes (or gives up) the airship's controls: remember the controller, and on
/// "enable" speak what changed meanwhile. The C# spoke both in one frame, altitude first; the
/// heading is queued after it here so neither cuts the other off.
void d_SwitchInputEnable(void* self, bool enable, const void* mi)
{
    o_SwitchInputEnable(self, enable, mi);
    hooks::guarded("FF6.Airship.SwitchInputEnable", [&] {
        if (!kAirshipController.is(self)) return;
        g_airship = self;
        if (!enable) return;
        void* fc = field_controller();
        if (!fc) return;
        bool spoke = check_altitude(fc);
        check_heading(fc, !spoke);
    });
}

using SwitchLandable_t = void (*)(void* self, bool landable, const void* mi);
SwitchLandable_t o_SwitchLandable = nullptr;

/// LastAnnouncementTime: when the core's SwitchLandable subscriber spoke "Can land" (in a
/// vehicle, false -> true; the same test as vehicle_landing.cpp).
void d_SwitchLandable(void* self, bool landable, const void* mi)
{
    o_SwitchLandable(self, landable, mi);
    hooks::guarded("FF6.Airship.SwitchLandable", [&] {
        if (ms::is_on_foot()) return;
        if (landable && !g_last_landable) g_last_landing_time = frame::now();
        g_last_landable = landable;
    });
}

// --- FFVI_ScreenReaderMod --------------------------------------------------------------------

/// AnnounceAirshipStatus: "Heading {0} degrees. <altitude>".
void announce_airship_status()
{
    bool ok = hooks::guarded_r("FF6.Airship.AnnounceAirshipStatus", false, [] {
        void* fc = field_controller();
        if (!fc) {
            ffpr::say(T(L"Airship status not available"));
            return true;
        }
        void* controller = airship_in_control(true);
        if (!controller || !ref::rd<void*>(controller, fFieldPlayer)) {
            ffpr::say(T(L"Not on airship"));
            return true;
        }
        float rotation = ref::call<float>(mGetZAxisRotateBirdCamera, fc);
        float normalized = std::fmod(std::fmod(rotation, 360.0f) + 360.0f, 360.0f);
        // Mirrored to match the compass's swapped east and west; "F0" rounds half away from zero.
        float mirrored = std::fmod(360.0f - normalized, 360.0f);
        long long degrees = std::llround(static_cast<double>(mirrored));
        std::wstring heading = TF(L"Heading {0} degrees", {std::to_wstring(degrees)});
        float ratio = ref::call<float>(mGetFlightAltitudeFieldOfViewRatio, fc, true);
        ffpr::say(heading + L". " + altitude_name(altitude_level(ratio)));
        return true;
    });
    if (!ok) ffpr::say(T(L"Error reading airship status"));
}

/// AnnounceAirshipOrCharacterStatus (H): the airship when flying, else the core's H.
void announce_airship_or_character_status()
{
    bool flying = hooks::guarded_r("FF6.Airship.AirshipInControl", false,
                                   [] { return airship_in_control(true) != nullptr; });
    if (flying) announce_airship_status();
    else actions::run("Mod.AnnounceCharacterStatus");
}

int install()
{
    int n = 0;
    if (hooks::attach(mSetZAxisRotateBirdCamera, &d_SetZAxisRotateBirdCamera, &o_SetZAxisRotateBirdCamera)) ++n;
    if (hooks::attach(mSetFlightAltitudeAndFieldOfViewRatio, &d_SetFlightAltitude, &o_SetFlightAltitude)) ++n;
    if (hooks::attach(mSwitchInputEnable, &d_SwitchInputEnable, &o_SwitchInputEnable)) ++n;
    if (hooks::attach(mSwitchLandable, &d_SwitchLandable, &o_SwitchLandable)) ++n;
    if (n < 4) LOGF("[Airship] %d of 4 airship hooks installed", n);
    actions::define("Mod.AnnounceAirshipOrCharacterStatus", [] { announce_airship_or_character_status(); });
    // The landing state is not reset here: it mirrors the core's, which keeps it across maps.
    events::on(events::Kind::SceneLoaded, [] {
        g_airship = nullptr;
        g_searched = false;
    });
    return n;
}

core::Install s_install{"FF6.Airship", &install, 510, game::kFF6};

} // namespace
} // namespace ffpr::ff6
