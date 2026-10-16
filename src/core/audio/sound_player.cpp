// Shared cue front end. Native callbacks publish values; the existing audio
// worker owns embedded/custom WAV loading, synthesis assets, spatial DSP/mixing.
#include "sound.h"
#include "audio_engine.h"
#include "cue_runtime.h"
#include "sound_constants.h"
#include "volume_settings.h"
#include "../core.h"
#include "../frame.h"
#include "../settings.h"
#include "../modtext.h"
#include "../say.h"
#include "../input/physical_gamepad.h"
#include "../input/input.h"
#include "../field/field_api.h"
#include "../field/baked_map.h"
#include "../system/system.h"
#include <algorithm>
#include <cmath>

namespace sound {
namespace {
bool stopped=false, wall_active=false, exp_active=false, field_active=false;
int last_map=-1;
double next_options=0,next_capture=0;
size_t capture_next=0; uint64_t capture_revision=0;
std::unique_ptr<scape::Snapshot> capture;

bool ensure_open() {
    if(stopped) return false;
    if(engine::initialized()) return true;
    if(frame::live() && frame::on_main_thread()) input::physical::request();
    return false;
}
bool enabled() { return settings::get_bool("NavigationAudio",true); }
void configure() {
    scape::Options o;
    o.navigation=enabled(); o.soundscape=settings::get_bool("Soundscape",false);
    o.hrtf=settings::get_bool("HRTF",false);
    o.range=std::clamp(settings::get_int("SoundscapeDistance",10)/5*5,5,25);
    o.master=levels::get("SoundscapeVolume")/50.f;
    for(size_t i=0;i<cues::count;++i) {
        const auto& d=cues::definitions[i];
        o.gains[i]=levels::get(d.volume)/50.f;
        if(i>=cues::first_entity) o.enabled[i-cues::first_entity]=settings::get_bool(d.toggle,true);
    }
    runtime::configure(o);
}
void range(int delta) {
    if(!settings::get_bool("Soundscape",false)) return;
    const int current=std::clamp(settings::get_int("SoundscapeDistance",10)/5*5,5,25);
    const int value=std::clamp(current+delta*5,5,25);
    settings::set_int("SoundscapeDistance",value); configure();
    ffpr::say(TF(L"Soundscape distance: {0} steps",{std::to_wstring(value)}));
}
void cancel() {
    if(field_active) runtime::cancel_field();
    field_active=false; runtime::discard(std::move(capture)); capture_next=0; capture_revision=0;
}
void tick() {
    const double at=frame::now();
    if(at>=next_options) { configure(); next_options=at+.10; }
    if(!enabled()) { cancel(); return; }
    const bool ready=ffpr::system::is_field_navigation_ready() && input::game_window_focused()
        && queries::is("ControllerRouter.IsFieldActive") && !queries::is("ControllerRouter.SuppressGameInput")
        && !queries::is("MessageWindowPatches.IsInDialogue");
    if(!ready) { cancel(); return; }
    const int map=field::current_map_id();
    if(map!=last_map) { cancel(); last_map=map; next_capture=0; }
    field_active=true;
    if(!settings::get_bool("Soundscape",false)) { runtime::discard(std::move(capture)); return; }
    ensure_open();
    if(!capture && at<next_capture) return;
    if(!capture) {
        capture=runtime::acquire_snapshot();
        if(!capture) return;
        capture->epoch=runtime::epoch(); capture->captured_at=at;
        capture_next=0; capture_revision=0;
        if(const auto* sm=field::baked::submap_by_map_id(map); sm && sm->world) {
            capture->wrap_width=float(sm->width); capture->wrap_height=float(sm->height);
        }
    }
    if(at-capture->captured_at>1.0) { cancel(); next_capture=at+.20; return; }
    const int result=field::capture_soundscape(*capture,capture_next,capture_revision);
    if(result<0) { runtime::discard(std::move(capture)); next_capture=at+.05; return; }
    if(result==0) return;
    const auto position=field::player_position();
    capture->player_x=position.x/16; capture->player_north=position.y/16;
    if(runtime::publish(capture)) next_capture=at+.20;
}
int install() {
    configure(); frame::subscribe(&tick,"audio.cues.capture");
    events::on(events::Kind::BattleEntering, [] {
        // Atomic cancellation only. Keep the owner snapshot for the next normal
        // tick to return, without touching the mixer/device or freeing rows here.
        runtime::cancel_field();
        field_active=false;
        wall_active=false;
        runtime::walls(0,0);
    });
    actions::define("Soundscape.RangeDown",[]{range(-1);});
    actions::define("Soundscape.RangeUp",[]{range(1);});
    actions::define("Audio.RefreshSettings",[]{configure();});
    actions::define("Settings.Changed.Soundscape",[]{configure();});
    actions::define("Settings.Changed.ExpCounter",[]{if(!settings::get_bool("ExpCounter",true)) stop_exp_counter();});
    return 0;
}
core::Install install_cues{"SoundPlayer",&install,20};
}
const char* volume_key(Volume v) {
    switch(v) {
    case Volume::WallBump:return "WallBumpVolume";
    case Volume::Footstep:return "FootstepVolume";
    case Volume::WallTone:return "WallToneVolume";
    case Volume::Beacon:return "BeaconVolume";
    case Volume::ExpCounter:return "ExpCounterVolume";
    case Volume::LandingPing:return "LandingPingVolume";
    }
    return "WallToneVolume";
}
int volume(Volume v) { return levels::get(volume_key(v)); }
void set_volume(Volume v,int value) { levels::set(volume_key(v),value); }
void initialize() { stopped=false; ensure_open(); }
void shutdown() { stopped=true; cancel(); engine::shutdown(); }
bool available() { return ensure_open(); }
void play_wall_bump() { if(enabled() && ensure_open()) runtime::play(cues::Id::WallBump,0,1,volume(Volume::WallBump)/50.f); }
void play_footstep() { if(enabled() && ensure_open()) runtime::play(cues::Id::Footstep,0,1,volume(Volume::Footstep)/50.f); }
void play_wall_tones_looped(const std::vector<Direction>& directions) {
    unsigned mask=0; for(auto d:directions) mask|=1u<<unsigned(d);
    if(!enabled() || !ensure_open()) mask=0;
    runtime::walls(mask,volume(Volume::WallTone)/50.f); wall_active=mask!=0;
}
void stop_wall_tone() { wall_active=false; runtime::walls(0,0); }
bool wall_tone_playing() { return wall_active; }
void play_beacon(bool south,float pan,float volume_scale,bool low_pitch) {
    if(!enabled() || !ensure_open()) return;
    pan=std::clamp(pan,-1.f,1.f);
    const float n=(south?-1.f:1.f)*std::sqrt(std::max(0.f,1-pan*pan));
    const float preference=volume(Volume::Beacon)/50.f;
    runtime::play(low_pitch?cues::Id::BeaconBlocked:cues::Id::Beacon,pan,n,
        preference==0?0:std::clamp(volume_scale*preference,
            constants::Beacon::MIN_VOLUME,constants::Beacon::MAX_VOLUME)/constants::Beacon::MAX_VOLUME,south?.7f:1.f);
}
void play_exp_counter() {
    exp_active=enabled() && ensure_open();
    runtime::counter(exp_active,volume(Volume::ExpCounter)/50.f);
}
void top_up_exp_counter() { runtime::counter(exp_active && enabled(),volume(Volume::ExpCounter)/50.f); }
void stop_exp_counter() { exp_active=false; runtime::counter(false); }
void play_landing_ping(float pan,float volume_scale,int frequency) {
    if(!enabled() || !ensure_open()) return;
    pan=std::clamp(pan,-1.f,1.f);
    // Existing landing callers use a lower pitch for south. Active HRTF keeps
    // the original clip pitch and uses position instead.
    const bool south=frequency==440;
    runtime::play(cues::Id::Landing,pan,(south?-1.f:1.f)*std::sqrt(std::max(0.f,1-pan*pan)),
        volume(Volume::LandingPing)/50.f,frequency>0?frequency/660.f:1.f,std::clamp(volume_scale,0.f,1.f));
}
void preview(cues::Id cue) { configure(); ensure_open(); runtime::preview(cue); }
}
