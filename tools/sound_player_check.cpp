// Actual native cue front end and range actions. Only native/context/worker
// boundaries are fake; the mixer/bank/HRTF have their own production checks.
#include <cstdio>
#include <map>
#include <string>
#include "../src/core/audio/sound_player.cpp"
namespace fixture {
std::map<std::string,int> settings;
std::map<std::string,actions::Fn> actions;
std::map<events::Kind,std::vector<events::Handler>> handlers;
double at=1;bool ready=true,focused=true,active=true,dialogue=false,accept=true;
int map=1,captures=0,published=0,discarded=0,plays=0,previews=0,requests=0;
unsigned walls=0;bool counter=false;uint64_t epoch=1;
sound::scape::Options options;
std::vector<std::wstring> spoken;
}
namespace sound::cues {
// Same production metadata, without the filesystem bank adapter in this fixture.
}
// Pull in the actual shared definitions/decoder; load() remains unused.
#include "../src/core/audio/cue_bank.cpp"
namespace data { std::string resource(const char*) { return {}; } }
namespace log_ { void write(const char*,...) {} }
namespace settings {
bool get_bool(const char* key,bool def) { auto i=fixture::settings.find(key);return i==fixture::settings.end()?def:i->second!=0; }
int get_int(const char* key,int def) { auto i=fixture::settings.find(key);return i==fixture::settings.end()?def:i->second; }
void set_int(const char* key,int value) { fixture::settings[key]=value; }
}
namespace core { Install::Install(const char* n,int(*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g) {} }
namespace actions { void define(const char* key,Fn fn) { fixture::actions[key]=std::move(fn); } }
namespace events { void on(Kind kind,Handler handler){fixture::handlers[kind].push_back(std::move(handler));} void fire(Kind kind){for(auto& handler:fixture::handlers[kind])handler();} }
namespace queries { bool is(const char* key) {
    const std::string k=key;
    if(k=="ControllerRouter.IsFieldActive")return fixture::active;
    if(k=="MessageWindowPatches.IsInDialogue")return fixture::dialogue;
    return false;
} }
namespace frame { double now(){return fixture::at;}bool live(){return true;}bool on_main_thread(){return true;}void subscribe(void(*)(),const char*){} }
namespace input { bool game_window_focused(){return fixture::focused;} }
namespace input::physical { void request(){++fixture::requests;} }
namespace sound::engine { bool initialized(){return true;}void shutdown(){} }
namespace ffpr::system { bool is_field_navigation_ready(){return fixture::ready;} }
namespace field {
int current_map_id(){return fixture::map;}unity::Vec3 player_position(){return {16,32,0};}
int capture_soundscape(sound::scape::Snapshot& s,size_t& next,uint64_t& revision) {
    ++fixture::captures;++next;revision=1;
    s.rows.push_back({uint64_t(next),sound::scape::Kind::NPC,2,3});return next==2?1:0;
}
}
namespace field::baked { const Submap* submap_by_map_id(int){return nullptr;} }
namespace sound::runtime {
void configure(scape::Options o){fixture::options=o;}
uint64_t epoch(){return fixture::epoch;}void cancel_field(){++fixture::epoch;}
std::unique_ptr<scape::Snapshot> acquire_snapshot(){return std::make_unique<scape::Snapshot>();}
void discard(std::unique_ptr<scape::Snapshot> s){if(s)++fixture::discarded;}
bool publish(std::unique_ptr<scape::Snapshot>& s){if(!fixture::accept)return false;++fixture::published;s.reset();return true;}
void play(cues::Id,float,float,float,float,float){++fixture::plays;}
void walls(unsigned mask,float){fixture::walls=mask;}
void counter(bool on,float){fixture::counter=on;}
void preview(cues::Id){++fixture::previews;}
}
namespace ffpr { void say(std::wstring_view text,bool){fixture::spoken.emplace_back(text);} }
namespace modtext { std::wstring TF(std::wstring_view text,std::initializer_list<std::wstring> args) {
    std::wstring result(text); if(!args.size())return result;
    auto n=result.find(L"{0}");if(n!=result.npos)result.replace(n,3,*args.begin());return result;
} }
int main(){
    int failures=0;auto expect=[&](bool yes,const char* why){if(!yes){++failures;std::printf("FAIL %s\n",why);}};
    sound::install();
    expect(fixture::options.navigation&&!fixture::options.soundscape&&!fixture::options.hrtf&&fixture::options.range==10,"independent defaults");
    for(auto volume:{sound::Volume::WallBump,sound::Volume::Footstep,sound::Volume::WallTone,sound::Volume::Beacon,sound::Volume::ExpCounter,sound::Volume::LandingPing}) {
        sound::set_volume(volume,30);expect(sound::volume(volume)==30,"legacy stereo volume persists");
        fixture::settings["HRTF"]=1;expect(sound::volume(volume)==50,"unset HRTF volume defaults independently");
        sound::set_volume(volume,85);fixture::settings["HRTF"]=0;
        expect(sound::volume(volume)==30,"legacy volume producer restores stereo bank after switching");
    }
    sound::levels::set("SoundscapeVolume",60);sound::levels::set("SoundscapeNPCVolume",40);
    fixture::settings["HRTF"]=1;
    sound::levels::set("SoundscapeVolume",20);sound::levels::set("SoundscapeNPCVolume",80);
    sound::configure();
    expect(fixture::options.hrtf&&fixture::options.master==.4f&&fixture::options.gains[size_t(sound::cues::Id::NPC)]==1.6f&&
        fixture::options.gains[size_t(sound::cues::Id::Footstep)]==1.7f,"actual producer publishes HRTF master and legacy/entity volumes");
    fixture::settings["HRTF"]=0;sound::configure();
    expect(!fixture::options.hrtf&&fixture::options.master==1.2f&&fixture::options.gains[size_t(sound::cues::Id::NPC)]==.8f&&
        fixture::options.gains[size_t(sound::cues::Id::Footstep)]==.6f,"actual producer restores independent stereo master and cue levels");
    fixture::actions["Soundscape.RangeDown"]();fixture::actions["Soundscape.RangeUp"]();
    expect(!fixture::settings.contains("SoundscapeDistance")&&fixture::spoken.empty(),"both shifted range actions are complete no-ops while off");
    sound::play_wall_tones_looped({sound::Direction::East});sound::tick();
    expect(fixture::walls==4,"soundscape off never mutes independently enabled wall tones");
    fixture::settings["Soundscape"]=1;
    for(int i=0;i<10;++i)fixture::actions["Soundscape.RangeDown"]();
    expect(fixture::settings["SoundscapeDistance"]==5,"range downward clamp");
    for(int i=0;i<10;++i)fixture::actions["Soundscape.RangeUp"]();
    expect(fixture::settings["SoundscapeDistance"]==25,"range upward clamp");
    fixture::actions["Soundscape.RangeDown"]();expect(fixture::settings["SoundscapeDistance"]==20,"range five-step change");
    sound::tick();expect(fixture::captures==1&&!fixture::published,"native snapshot capture continues across frames");
    fixture::ready=false;sound::tick();
    expect(fixture::discarded==1&&fixture::epoch==2&&!fixture::published,"transition discards partial snapshot through worker return and cancels generation");
    fixture::ready=true;fixture::at+=.3;sound::tick();sound::tick();
    expect(fixture::published==1,"ready snapshot reaches the worker after bounded capture");
    fixture::at+=.3;fixture::dialogue=true;const auto before=fixture::captures;sound::tick();
    expect(fixture::captures==before&&fixture::epoch==3,"dialogue suspends capture and active field emitters");
    fixture::dialogue=false;fixture::focused=false;sound::tick();expect(fixture::captures==before,"background field performs no entity capture");
    fixture::settings["NavigationAudio"]=0;
    const auto plays=fixture::plays;sound::play_footstep();sound::play_wall_bump();sound::play_beacon(false,0,1);
    sound::play_exp_counter();sound::play_landing_ping(0,1,660);sound::play_wall_tones_looped({sound::Direction::North});
    expect(fixture::plays==plays&&!fixture::counter&&!fixture::walls,"navigation master gates every old sound path");
    fixture::settings["NavigationAudio"]=1;sound::preview(sound::cues::Id::NPC);
    expect(fixture::previews==1,"preview uses central cue path");
    sound::field_active=true;sound::capture=std::make_unique<sound::scape::Snapshot>();
    auto* retained=sound::capture.get();
    sound::play_wall_tones_looped({sound::Direction::North});
    const auto epoch_before=fixture::epoch;const int discarded_before=fixture::discarded;
    events::fire(events::Kind::BattleEntering);
    expect(fixture::epoch==epoch_before+1 && !fixture::walls && !sound::field_active &&
        sound::capture.get()==retained && fixture::discarded==discarded_before,
        "battle entry cancels playing field audio atomically without device calls, snapshot return or destruction");
    fixture::ready=false;sound::tick();
    expect(!sound::capture && fixture::discarded==discarded_before+1,
        "ordinary owner tick returns the retained snapshot after immediate audio cancellation");
    std::printf("%s cue producer/range/context: %d failures\n",failures?"FAIL":"PASS",failures);return failures?1:0;
}
