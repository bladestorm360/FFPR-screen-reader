// check-sources: audio/cue_bank.cpp audio/soundscape.cpp audio/hrtf.cpp audio/legacy_cues.cpp audio/tone_generator.cpp
// Production bank, classifier, scheduler, software mixer and actual OpenAL Soft
// loopback. Fake SDL queue only: no game, physical device or installed files.
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <filesystem>
#include <set>
#include <chrono>
#include <limits>
#include <thread>
#include "../src/core/audio/cue_runtime.cpp"
#include "../src/core/audio/sound_constants.h"
namespace sdl {
#define DEFINE(name) decltype(&::name) name=nullptr;
SDL_DYNAMIC_FUNCTIONS(DEFINE)
#undef DEFINE
}
namespace data {
std::string resource(const char* name) {
    std::string stem=name; stem=stem.substr(6); for(auto& c:stem)c=char(tolower(c));
    std::ifstream f(std::filesystem::path("data/sounds")/(stem+".wav"),std::ios::binary);
    return {std::istreambuf_iterator<char>(f),{}};
}
}
namespace log_ { void write(const char*,...) {} }
namespace fixture {
int queued=0,clears=0,puts=0; bool nonzero=false;
std::atomic<bool> block{false};HANDLE entered=nullptr,released=nullptr;
bool SDLCALL clear(SDL_AudioStream*) { queued=0; ++clears; return true; }
int SDLCALL get(SDL_AudioStream*) { return queued; }
bool SDLCALL put(SDL_AudioStream*,const void* buffer,int bytes) {
    if(block.exchange(false)) { SetEvent(entered); WaitForSingleObject(released,5000); }
    const auto* pcm=static_cast<const int16_t*>(buffer);
    for(int i=0;i<bytes/2;++i) nonzero|=pcm[i]!=0;
    queued+=bytes; ++puts; return true;
}
sound::scape::Options options() {
    sound::scape::Options o; o.soundscape=true; o.enabled.fill(true); o.gains.fill(1); return o;
}
double energy(const std::vector<int16_t>& s,int channel) { double n=0;for(size_t i=channel;i<s.size();i+=2)n+=double(s[i])*s[i];return n; }
}
int main(int argc,char** argv) {
    using namespace sound;
    if(argc==3 && std::string(argv[1])=="--write-legacy") {
        const auto root=std::filesystem::absolute(argv[2]);std::filesystem::create_directories(root);
        for(size_t i=0;i<cues::first_entity;++i) {
            const auto pcm=legacy::make(cues::Id(i)).mono;
            std::ofstream f(root/(std::string(cues::definitions[i].file)+".wav"),std::ios::binary);
            auto u16=[&](uint16_t v){f.put(char(v));f.put(char(v>>8));};
            auto u32=[&](uint32_t v){u16(uint16_t(v));u16(uint16_t(v>>16));};
            f.write("RIFF",4);u32(uint32_t(36+pcm.size()*2));f.write("WAVEfmt ",8);u32(16);
            u16(1);u16(1);u32(cues::rate);u32(cues::rate*2);u16(2);u16(16);
            f.write("data",4);u32(uint32_t(pcm.size()*2));
            for(auto sample:pcm)u16(uint16_t(sample));
            if(!f) return 1;
        }
        return 0;
    }
    int failures=0;
    auto expect=[&](bool yes,const char* message) { if(!yes) { ++failures; std::printf("FAIL %s\n",message); } };
    auto bank=cues::load(std::filesystem::current_path().wstring());
    for(size_t i=0;i<cues::count;++i) {
        expect(!bank[i].mono.empty(),cues::definitions[i].file);
        cues::Clip invalid; expect(!cues::decode("RIFFbad",invalid),"malformed WAV fails closed");
    }
    // Compare actual mixer output against the original, untouched tone generator.
    // This catches timbre/envelope, dual-mono attenuation, pan and duration changes.
    runtime::Player reference;reference.bank=bank;reference.prepared=true;
    auto parity=[&](cues::Id cue,const tone::Pcm& expected,float pan=0,float gain=1,float pitch=1,float amplitude=1) {
        reference.stop_all();reference.start(0,cue,pan,pan==0?1.f:0.f,gain,pitch,false,0,amplitude);
        std::vector<int16_t> rendered(expected.size()+100,0),wanted=expected;wanted.resize(rendered.size());
        reference.render(rendered.data(),int(rendered.size()/2));
        expect(rendered==wanted,"legacy effect actual mixer is sample-exact to pre-soundscape output");
    };
    namespace C=sound::constants;
    parity(cues::Id::Footstep,tone::mono_to_stereo(tone::click_tone(C::Footstep::FREQUENCY,25,C::Footstep::VOLUME)));
    parity(cues::Id::WallBump,tone::mono_to_stereo(tone::thud_tone(27,60,C::WallBump::VOLUME)));
    const int frequencies[]={294,165,247,196};const float scales[]={1,.70f,.85f,.85f},pans[]={.5f,.5f,1,0};
    for(int d=0;d<4;++d) parity(cues::Id(int(cues::Id::WallNorth)+d),tone::stereo_tone(frequencies[d],200,.12f*scales[d],pans[d],true));
    parity(cues::Id::Counter,tone::landing_ping(2000,100,50,.15f,.5f));
    for(int frequency:{660,440,550,520}) for(float pan:{-1.f,0.f,1.f})
        parity(cues::Id::Landing,tone::landing_ping(frequency,250,80,.132f*.85f,pan*.5f+.5f),pan,1,frequency/660.f,.85f);
    for(int frequency:{400,280,200,140}) for(float pan:{-1.f,-.5f,0.f,.5f,1.f}) for(float volume:{.10f,.25f,.50f}) {
        tone::Pcm old(1323*2);const double angle=(pan*.5f+.5f)*3.14159265358979323846/2;
        const float l=volume*float(std::cos(angle)),r=volume*float(std::sin(angle));
        for(int n=0;n<1323;++n) {
            const double attack=std::min(1.0,double(n)/132),decay=double(1323-n)/1323;
            const double envelope=attack*decay;
            const double value=std::sin(2*3.14159265358979323846*frequency*(double(n)/22050))*envelope;
            old[n*2]=int16_t(int32_t(value*l*32767));old[n*2+1]=int16_t(int32_t(value*r*32767));
        }
        const bool blocked=frequency<=200;const float pitch=(frequency==280||frequency==140)?.7f:1.f;
        parity(blocked?cues::Id::BeaconBlocked:cues::Id::Beacon,old,pan,volume/.5f,pitch);
    }
    for(size_t i=0;i<cues::first_entity;++i) expect(bank[i].mono==legacy::make(cues::Id(i)).mono,"embedded fallback is original full-amplitude legacy signal");
    reference.options=fixture::options();reference.field_generation=runtime::epoch();
    auto expired=std::make_unique<scape::Snapshot>();expired->epoch=runtime::epoch();expired->captured_at=runtime::now();
    expired->rows={{1,scape::Kind::NPC,1,1}};reference.accept(std::move(expired),runtime::now());
    reference.start(6,cues::Id::Beacon,0,1,1,1);reference.start(0,cues::Id::Footstep,0,1,1,1);
    reference.maintain(runtime::now()+2);
    expect(reference.emitters.empty()&&reference.voices[6].on&&reference.voices[0].on,"stale soundscape cannot cancel beacon or footsteps");
    reference.start(16,cues::Id::NPC,1,1,1,1,false,123);
    auto scape_off=fixture::options();scape_off.soundscape=false;reference.configure(scape_off);
    expect(reference.voices[6].on&&reference.voices[0].on&&!reference.voices[16].on,"soundscape master stops only entity cues while beacon/footsteps remain active");
    // Four stereo PCM16 frames at 44.1 kHz -> two centered 22.05 kHz
    // samples, proving both supported downmix and rate conversion.
    std::string stereo(60,'\0');
    auto put16=[&](size_t at,unsigned v){stereo[at]=char(v);stereo[at+1]=char(v>>8);};
    auto put32=[&](size_t at,unsigned v){put16(at,v&65535);put16(at+2,v>>16);};
    stereo.replace(0,4,"RIFF");put32(4,52);stereo.replace(8,8,"WAVEfmt ");put32(16,16);
    put16(20,1);put16(22,2);put32(24,44100);put32(28,176400);put16(32,4);put16(34,16);
    stereo.replace(36,4,"data");put32(40,16);
    for(size_t f=0;f<4;++f){put16(44+f*4,1200);put16(46+f*4,unsigned(uint16_t(int16_t(-400))));}
    cues::Clip converted;
    expect(cues::decode(stereo,converted)&&converted.mono==std::vector<int16_t>({400,400}),"custom PCM16 stereo downmix and 44.1kHz resampling");
    put16(20,3);expect(!cues::decode(stereo,converted),"float WAV encoding rejected without replacing fallback");
    put16(20,1);put16(34,8);expect(!cues::decode(stereo,converted),"unsupported bit depth rejected");
    put16(34,16);put32(40,5000);expect(!cues::decode(stereo,converted),"truncated chunk cannot read outside its file");
    const auto custom=std::filesystem::absolute("build_check/soundscape/custom");
    expect(custom.parent_path()==std::filesystem::absolute("build_check/soundscape"),"fixture writes stay under project evidence");
    std::filesystem::create_directories(custom/"sounds");
    std::filesystem::copy_file("data/sounds/npc.wav",custom/"sounds/npc.wav",std::filesystem::copy_options::overwrite_existing);
    std::ofstream(custom/"sounds/trap.wav",std::ios::binary)<<"invalid";
    auto overrides=cues::load(custom.wstring());
    expect(overrides[size_t(cues::Id::NPC)].custom && !overrides[size_t(cues::Id::Trap)].custom &&
        overrides[size_t(cues::Id::Trap)].mono==bank[size_t(cues::Id::Trap)].mono && !overrides[0].custom,
        "per-file overrides and malformed/missing fallback preserve embedded cues");
    auto o=fixture::options(); scape::Snapshot s;
    s.rows={{1,scape::Kind::NPC,2,1},{2,scape::Kind::NPC,3,1},{3,scape::Kind::NPC,4,1},
        {4,scape::Kind::Exit,5,0,123,1,false},{5,scape::Kind::Event,5,1,-1,1,true},
        {6,scape::Kind::Exit,-5,0,456},{7,scape::Kind::Exit,0,3,-1,1,true},
        {8,scape::Kind::Event,0,2,-1,1,false,false,L"crack_floor"},
        {9,scape::Kind::Event,0,-3,-1,1,false,false,L"teleport_trials_wrong_pad"},
        {10,scape::Kind::Event,0,4,-1,1,false,false,L"boss_battle"},
        {11,scape::Kind::Event,0,1,-1,1,false,false,L"",L"ToLayer"},
        {12,scape::Kind::Chest,1,0,-1,1,false,true},
        {13,scape::Kind::SavePoint,0,5},{14,scape::Kind::Vehicle,0,-5},
        {15,scape::Kind::NPC,11,0}};
    auto e=scape::classify(s,o);
    expect(e.size()==10,"only eligible in-range unopened non-ToLayer non-standalone-door entities emit");
    auto find=[&](uint64_t id)->const scape::Emitter* { for(const auto& x:e)if(x.id==id)return &x;return nullptr; };
    expect(find(1)&&find(1)->detune==1 && find(2)->detune>1 && find(3)->detune<1,"same-kind pitch follows pathfinder order");
    expect(find(4)&&find(4)->cue==cues::Id::Door && find(6)->cue==cues::Id::Exit,"adjacent door and lone exit differentiated");
    expect(find(8)&&find(8)->cue==cues::Id::Trap && find(9)->cue==cues::Id::Event && find(10)->cue==cues::Id::Event,"explicit trap identifiers; trial pads and battles remain events");
    expect(scape::cadence(*find(8),.25,1)<scape::cadence(*find(9),.25,1),"trap within two steps repeats faster");
    expect(scape::cadence(*find(8),4,.5)>=8.2,"long custom clip cadence prevents self-overlap");
    expect(!scape::trap_hint(L"waterfall")&&!scape::trap_hint(L"Warp_Bahamut_trials")&&scape::trap_hint(L"bridge_break"),"trap false positives excluded");
    o.range=5; expect(scape::classify(s,o).size()==10,"five-step boundary inclusive");
    o.range=15; expect(scape::classify(s,o).size()==11,"range expansion includes farther voice");
    o.soundscape=false;expect(scape::classify(s,o).empty(),"soundscape switch independent");o.soundscape=true;
    o.enabled[size_t(cues::Id::NPC)-cues::first_entity]=false;
    expect(scape::classify(s,o).size()==7,"individual category switch");
    expect(scape::wrapped(99,100)==-1,"world edge uses nearest wrapped position");
    scape::Snapshot unknown;
    unknown.rows={{20,scape::Kind::Exit,0,2}};unknown.rows[0].verified_exit=true;
    auto generic=scape::classify(unknown,fixture::options());
    expect(generic.size()==1&&generic[0].cue==cues::Id::Exit,"verified native exit with unavailable subtype/destination keeps a generic cue");
    unknown.player_x=std::numeric_limits<float>::quiet_NaN();
    expect(scape::classify(unknown,fixture::options()).empty(),"invalid copied position fails closed before spatial processing");
    unknown.player_x=0;unknown.rows[0].door=true;unknown.rows[0].hint=L"trapdoor_fall_down";
    generic=scape::classify(unknown,fixture::options());
    expect(generic.size()==1&&generic[0].cue==cues::Id::Trap,"explicit trapdoor hazard takes precedence over exit/door type");

    const auto dll=std::filesystem::absolute("third_party/openal-soft/soft_oal.dll").wstring();
    std::vector<int16_t> legacy_hrtf_pcm(cues::rate*2);
    const auto original_step=tone::click_tone(500,25,C::Footstep::VOLUME);
    const auto original_bump=tone::thud_tone(27,60,C::WallBump::VOLUME);
    expect(reference.hrtf.open(dll,bank),"actual production legacy voice HRTF opens");
    reference.start(0,cues::Id::Footstep,0,1,1,1);
    expect(reference.voices[0].reference.mono==original_step&&reference.voices[0].gain==1,"HRTF receives unaltered full-amplitude original footstep");
    reference.render(legacy_hrtf_pcm.data(),441);reference.render(legacy_hrtf_pcm.data(),441);
    expect(fixture::energy(legacy_hrtf_pcm,0)>0,"short footstep HRTF tail survives its dry sample boundary");
    reference.stop_all();
    reference.start(1,cues::Id::WallBump,0,1,1,1);
    expect(reference.voices[1].reference.mono==original_bump&&reference.voices[1].gain==1,"HRTF receives unaltered full-amplitude original wall bump");
    reference.render(legacy_hrtf_pcm.data(),441);
    expect(fixture::energy(legacy_hrtf_pcm,0)>0,"actual production HRTF legacy voice renders");reference.stop_all();
    reference.hrtf.open(L"",bank); // production has one loopback context per audio worker
    // Actual loopback output: the single HRTF master doubles amplitude (four
    // times energy), across all effects, without another per-source boost.
    Hrtf boosted;std::vector<int16_t> level_pcm(cues::rate*2);
    std::array<double,cues::count> unboosted{};
    expect(boosted.open(dll,bank,1),"HRTF reference output opens at unity master");
    for(size_t i=0;i<cues::count;++i) {
        boosted.play(0,cues::Id(i),0,1,.25f,false);boosted.render(level_pcm.data(),cues::rate);
        unboosted[i]=fixture::energy(level_pcm,0)+fixture::energy(level_pcm,1);
    }
    expect(boosted.open(dll,bank),"production HRTF opens with the 2x master default");
    for(size_t i=0;i<cues::count;++i) {
        boosted.play(0,cues::Id(i),0,1,.25f,false);boosted.render(level_pcm.data(),cues::rate);
        const double energy=fixture::energy(level_pcm,0)+fixture::energy(level_pcm,1);
        expect(unboosted[i]>0&&energy>unboosted[i]*3.9&&energy<unboosted[i]*4.1,"all 18 actual HRTF effects get exactly one 2x amplitude boost");
    }
    boosted.play(0,cues::Id::WallBump,0,1,0,false);boosted.render(level_pcm.data(),cues::rate);
    int silent_peak=0;for(auto sample:level_pcm)silent_peak=std::max(silent_peak,std::abs(int(sample)));
    std::printf("HRTF muted output: peak %d PCM16 units (one-unit quantization dither allowed)\n",silent_peak);
    expect(silent_peak<=1,"2x master preserves zero-volume mute apart from one-unit output dither");
    boosted.open(L"",bank);
    Hrtf hrtf;
    const double opening=runtime::now();
    expect(hrtf.open(dll,bank)&&hrtf.active(),"official x64 loopback actually enables HRTF without a physical device");
    const double opened=runtime::now();
    std::vector<int16_t> pcm(cues::rate*2);
    hrtf.play_pcm(0,original_step,0,1,1,false);hrtf.render(pcm.data(),cues::rate);
    expect(fixture::energy(pcm,0)>0&&fixture::energy(pcm,1)>0,"original footstep signal audible through actual HRTF");
    hrtf.play_pcm(0,original_bump,0,1,1,false);hrtf.render(pcm.data(),cues::rate);
    expect(fixture::energy(pcm,0)>0&&fixture::energy(pcm,1)>0,"original wall bump signal audible through actual HRTF");
    hrtf.play(0,cues::Id::NPC,-1,0,1,false); hrtf.render(pcm.data(),cues::rate);
    expect(fixture::energy(pcm,0)>fixture::energy(pcm,1)*1.2,"actual left HRTF favors left ear");
    const double reference_energy=fixture::energy(pcm,0);
    hrtf.play(0,cues::Id::NPC,-1,0,2,false); hrtf.render(pcm.data(),cues::rate);
    expect(fixture::energy(pcm,0)>reference_energy*3.5,"HRTF volume above fifty percent increases actual output gain");
    hrtf.play(0,cues::Id::NPC,1,0,1,false); hrtf.render(pcm.data(),cues::rate);
    expect(fixture::energy(pcm,1)>fixture::energy(pcm,0)*1.2,"actual right HRTF favors right ear");
    for(size_t n=0;n<Hrtf::voices;++n) hrtf.play(n,cues::Id::NPC,float(n%9)-4,float(n%7)-3,.01f,true);
    double hrtf_peak=0,hrtf_total=0;
    for(int n=0;n<100;++n) {
        const double begin=runtime::now();hrtf.render(pcm.data(),441);
        const double cost=runtime::now()-begin;hrtf_peak=std::max(hrtf_peak,cost);hrtf_total+=cost;
    }
    std::printf("Actual HRTF: cold open %.3f ms, 128 looping voices: mean/peak 20ms render %.3f/%.3f ms\n",
        (opened-opening)*1000,hrtf_total*10,hrtf_peak*1000);
    expect(!hrtf.open(L"Z:\\ffpr_missing_hrtf.dll",bank)&&!hrtf.active(),"missing HRTF is an explicit fallback");

    sdl::SDL_ClearAudioStream=&fixture::clear; sdl::SDL_GetAudioStreamQueued=&fixture::get; sdl::SDL_PutAudioStreamData=&fixture::put;
    runtime::configure(fixture::options()); auto* stream=reinterpret_cast<SDL_AudioStream*>(1);
    runtime::play(cues::Id::Footstep); runtime::pump(stream);
    expect(fixture::puts==3 && fixture::nonzero,"real software cue mixer reaches existing SDL stream");
    auto observation=runtime::acquire_snapshot(); observation->epoch=runtime::epoch(); observation->captured_at=runtime::now();
    observation->rows={{1,scape::Kind::NPC,2,0}};
    expect(runtime::publish(observation),"owned snapshot publication");fixture::queued=0;runtime::pump(stream);
    expect(runtime::player->emitters.size()==1,"production worker consumes native observation values");
    const double first_submission=runtime::now();
    std::printf("Owned snapshot to fake SDL: %.3f ms; physical audible latency not measured\n",(first_submission-runtime::player->snapshot->captured_at)*1000);
    runtime::cancel_field();fixture::queued=0;runtime::pump(stream);
    expect(runtime::player->emitters.empty()&&fixture::clears>0,"out-of-band context cancellation clears voices and queued mix");
    runtime::play(cues::Id::Beacon);runtime::cancel_field();fixture::queued=0;runtime::pump(stream);
    expect(!runtime::player->voices[6].on,"queued field ping cannot replay into a new context");
    observation=runtime::acquire_snapshot(); observation->epoch=runtime::epoch()-1; observation->captured_at=runtime::now();
    observation->rows={{1,scape::Kind::NPC,2,0}};
    runtime::publish(observation); fixture::queued=0;runtime::pump(stream);
    expect(runtime::player->emitters.empty(),"old map generation cannot restore canceled emitters");
    observation=runtime::acquire_snapshot();observation->epoch=runtime::epoch();observation->captured_at=runtime::now()-2;
    observation->rows={{1,scape::Kind::NPC,2,0}};runtime::publish(observation);fixture::queued=0;runtime::pump(stream);
    expect(runtime::player->emitters.empty(),"expired native snapshot cannot create emitters");
    auto off=fixture::options();off.navigation=false;runtime::configure(off);runtime::play(cues::Id::Footstep);runtime::pump(stream);
    expect(fixture::queued==0,"navigation master stops every mod cue and queued output");
    runtime::configure(fixture::options());runtime::pump(stream);
    const auto start=runtime::now();
    for(int i=0;i<10000;++i) runtime::play(cues::Id::Footstep);
    expect(runtime::now()-start<.1,"full request ring never waits for consumer");
    fixture::entered=CreateEventW(nullptr,TRUE,FALSE,nullptr);fixture::released=CreateEventW(nullptr,TRUE,FALSE,nullptr);
    fixture::queued=0;fixture::block=true;
    std::thread blocked([&]{runtime::pump(stream);});
    expect(WaitForSingleObject(fixture::entered,5000)==WAIT_OBJECT_0,"production cue pump reaches blocked fake driver");
    const double cancel_at=runtime::now();runtime::configure(off);runtime::cancel_field();
    for(int n=0;n<10000;++n)runtime::play(cues::Id::Beacon);
    expect(runtime::now()-cancel_at<.1,"master/context cancellation and pressure never wait on a blocked driver");
    SetEvent(fixture::released);blocked.join();
    expect(fixture::queued==0,"cancellation during device submission clears the just-submitted old mix immediately after driver return");
    CloseHandle(fixture::entered);CloseHandle(fixture::released);
    runtime::pump(stream);
    runtime::close();

    runtime::configure(fixture::options()); // restore the master for the pure dense scheduler
    runtime::Player dense; dense.bank=bank;dense.prepared=true; dense.options=fixture::options();
    auto many=std::make_unique<scape::Snapshot>();many->epoch=runtime::epoch();many->captured_at=runtime::now();
    for(uint64_t i=1;i<=400;++i)many->rows.push_back({i,scape::Kind::NPC,1,1});
    dense.accept(std::move(many),runtime::now()); std::set<uint64_t> seen;
    const double base=runtime::now();double longest=0;
    for(int n=0;n<200;++n) {
        dense.snapshot->captured_at=base+n*.02;
        const double begin=runtime::now();dense.maintain(dense.snapshot->captured_at);
        for(const auto& v:dense.voices)if(v.on&&v.entity)seen.insert(v.id);
        dense.render(pcm.data(),441);longest=std::max(longest,runtime::now()-begin);
    }
    expect(seen.size()==400&&dense.deferred>0,"busy overlap defers fairly; every entity sounds without nearest-N culling");
    std::printf("Dense 400: %zu heard, %llu deferred starts, max worker chunk %.3f ms\n",seen.size(),(unsigned long long)dense.deferred,longest*1000);
    std::printf("%s soundscape: %d failures\n",failures?"FAIL":"PASS",failures);
    return failures?1:0;
}
