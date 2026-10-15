#include "cue_runtime.h"
#include "hrtf.h"
#include "legacy_cues.h"
#include "../nonblocking_queue.h"
#include "../sdl_dyn.h"
#include "../log.h"
#include <windows.h>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <bit>
#include <unordered_map>
#include <unordered_set>

namespace sound::runtime {
namespace {
struct Request { cues::Id cue{}; float x=0,north=1,gain=1,pitch=1; uint64_t generation=0,field_generation=0; bool preview=false; float amplitude=1; };
NonblockingQueue<Request,64> requests;
NonblockingQueue<scape::Options,8> configurations;
// Exactly four buffers, single producer/consumer per direction. No CAS/retry:
// returning one of the four buffers always has a free slot in a four-slot queue.
class Buffers {
    std::array<std::unique_ptr<scape::Snapshot>,5> values;
    std::atomic<size_t> head{0},tail{0};
public:
    bool push(std::unique_ptr<scape::Snapshot>&& value) {
        const auto at=tail.load(std::memory_order_relaxed),next=(at+1)%values.size();
        if(next==head.load(std::memory_order_acquire)) return false;
        values[at]=std::move(value); tail.store(next,std::memory_order_release); return true;
    }
    bool pop(std::unique_ptr<scape::Snapshot>& value) {
        const auto at=head.load(std::memory_order_relaxed);
        if(at==tail.load(std::memory_order_acquire)) return false;
        value=std::move(values[at]); head.store((at+1)%values.size(),std::memory_order_release); return true;
    }
};
Buffers observations,spares,retired;
std::atomic<uint64_t> field_epoch{1}, audio_epoch{1};
std::atomic<bool> nav_enabled{true}, active_hrtf{false};
std::atomic<unsigned> wall_mask{0};
std::atomic<float> wall_gain{1},counter_gain{1};
std::atomic<bool> counter_enabled{false};
double now() {
    static const double frequency=[] { LARGE_INTEGER f{}; QueryPerformanceFrequency(&f); return double(f.QuadPart); }();
    LARGE_INTEGER q{}; QueryPerformanceCounter(&q); return double(q.QuadPart)/frequency;
}
void recycle(std::unique_ptr<scape::Snapshot> snapshot) {
    if(!snapshot) return;
    snapshot->rows.clear(); // all row destruction stays on the audio worker
    snapshot->epoch=0; snapshot->wrap_width=0; snapshot->wrap_height=0;
    spares.push(std::move(snapshot));
}
struct Voice {
    bool on=false,loop=false,entity=false;
    uint64_t id=0; cues::Id cue{};
    float x=0,north=1,gain=1,pitch=1;
    double cursor=0;
    legacy::Pcm reference;
};
struct Clock { double next=0; };
class Player {
public:
    cues::Bank bank;
    Hrtf hrtf;
    scape::Options options;
    std::array<Voice,Hrtf::voices> voices{};
    std::unique_ptr<scape::Snapshot> snapshot;
    std::vector<scape::Emitter> emitters;
    std::unordered_map<uint64_t,Clock> clocks;
    uint64_t field_generation=0,audio_generation=0;
    size_t turn=0;
    bool wanted_hrtf=false, prepared=false;
    unsigned last_wall=0;
    uint64_t deferred=0;
    void stop(size_t i) { hrtf.stop(i); voices[i]={}; }
    void stop_all() { for(size_t i=0;i<voices.size();++i) stop(i); }
    void stop_field() {
        for(size_t i=0;i<voices.size();++i) if(i!=8 && i!=15) stop(i);
        recycle(std::move(snapshot)); emitters.clear(); clocks.clear(); last_wall=0;
    }
    void start(size_t i,cues::Id cue,float x,float north,float gain,float pitch,bool loop=false,uint64_t id=0,float amplitude=1) {
        stop(i);
        if(bank[size_t(cue)].mono.empty()) return;
        auto& v=voices[i];
        v={true,loop,id!=0,id,cue,x,north,std::clamp(gain*amplitude,0.f,4.f),hrtf.active()?1.f:std::clamp(pitch,.4f,2.f),0,{}};
        if(size_t(cue)<cues::first_entity && !bank[size_t(cue)].custom) {
            v.reference=legacy::make(cue,x,gain,pitch,amplitude,hrtf.active());
            v.pitch=1; // Original synthesis changes frequency without stretching envelopes.
            v.gain=(cue==cues::Id::Beacon || cue==cues::Id::BeaconBlocked)?1:gain;
            hrtf.play_pcm(i,v.reference.mono,x,north,v.gain,loop);
        } else hrtf.play(i,cue,x,north,v.gain,loop);
    }
    void configure(scape::Options next) {
        const bool change=next.hrtf!=wanted_hrtf;
        options=std::move(next);
        if(!prepared) { bank=cues::load(cues::module_root()); prepared=true; }
        if(change) {
            stop_all(); wanted_hrtf=options.hrtf;
            // A fresh instance destroys its thread-local context on this same worker.
            // Keep Hrtf non-movable; reset through open only, off uses an empty path.
            const bool enabled=hrtf.open(options.hrtf ? cues::module_root()+L".ffpr-deps\\openal-soft\\soft_oal.dll" : L"",bank);
            active_hrtf=enabled;
            if(options.hrtf) LOGF("[Audio] HRTF requested: %s",enabled?"active loopback":"unavailable; stereo/pitch fallback");
        }
        if(!options.navigation) stop_all();
        refresh();
    }
    void refresh() {
        emitters=snapshot?scape::classify(*snapshot,options):std::vector<scape::Emitter>{};
        std::unordered_set<uint64_t> ids;
        for(const auto& e:emitters) ids.insert(e.id);
        for(auto i=clocks.begin();i!=clocks.end();) if(!ids.contains(i->first)) i=clocks.erase(i); else ++i;
        for(size_t i=16;i<voices.size();++i) if(voices[i].on && !ids.contains(voices[i].id)) stop(i);
        for(const auto& e:emitters) {
            auto [clock,inserted]=clocks.try_emplace(e.id);
            if(inserted) clock->second.next=now()+double(e.rank%16)*.07;
            for(size_t i=16;i<voices.size();++i) if(voices[i].on && voices[i].id==e.id) {
                voices[i].x=e.x; voices[i].north=e.north;
                voices[i].gain=gain(e); hrtf.position(i,e.x,e.north,voices[i].gain);
            }
        }
    }
    float gain(const scape::Emitter& e) const {
        return options.master*options.gains[size_t(e.cue)]*std::clamp(1.f-.75f*e.distance/float(options.range),.25f,1.f);
    }
    float pitch(const scape::Emitter& e) const { return hrtf.active()?1.f:e.detune*(e.north<0?.7f:1.f); }
    bool accept(std::unique_ptr<scape::Snapshot> next,double at) {
        if(!next || next->epoch!=field_epoch.load() || next->rows.size()>scape::max_rows || at-next->captured_at>1.5 || at<next->captured_at) {
            recycle(std::move(next)); return false;
        }
        recycle(std::move(snapshot));
        snapshot=std::move(next); refresh(); return true;
    }
    void maintain(double at) {
        if(!nav_enabled.load() || !options.navigation) { stop_all(); return; }
        if(snapshot && at-snapshot->captured_at>1.5) {
            // Only obsolete entity observations expire here. Beacon, footsteps,
            // walls and landing pings have independent producers/lifetimes.
            for(size_t i=16;i<voices.size();++i) stop(i);
            recycle(std::move(snapshot)); emitters.clear(); clocks.clear();
        }
        const unsigned mask=wall_mask.load();
        const int count=std::popcount(mask&15u);
        for(unsigned d=0;d<4;++d) {
            const size_t slot=2+d;
            if(mask&(1u<<d)) {
                const float x=d==2?1.f:(d==3?-1.f:0.f),n=d==0?1.f:(d==1?-1.f:0.f);
                const float gain=wall_gain.load()*(count>1?float(1.0/std::sqrt(count)):1.f);
                if(!voices[slot].on) start(slot,cues::Id(size_t(cues::Id::WallNorth)+d),x,n,gain,1,true);
                else { voices[slot].gain=gain; hrtf.position(slot,x,n,gain); }
            } else if(voices[slot].on) stop(slot);
        }
        if(counter_enabled.load()) {
            if(!voices[8].on) start(8,cues::Id::Counter,0,1,counter_gain.load(),1,true);
            else { voices[8].gain=counter_gain.load(); hrtf.position(8,0,1,voices[8].gain); }
        } else if(voices[8].on) stop(8);
        // Every emitter retains an independent clock. Resource pressure defers
        // starts in rotating order; it never chooses a nearest-N subset.
        size_t starts=0;
        for(size_t checked=0;checked<emitters.size();++checked) {
            const size_t index=(turn+checked)%emitters.size(); const auto& e=emitters[index];
            auto& clock=clocks[e.id]; if(at<clock.next) continue;
            if(starts==8) { ++deferred; continue; }
            size_t slot=16; for(;slot<voices.size() && voices[slot].on;++slot) {}
            if(slot==voices.size()) { ++deferred; continue; }
            const float p=pitch(e);
            start(slot,e.cue,e.x,e.north,gain(e),p,false,e.id); ++starts;
            clock.next=at+scape::cadence(e,double(bank[size_t(e.cue)].mono.size())/cues::rate,p);
        }
        if(!emitters.empty()) turn=(turn+8)%emitters.size();
    }
    void render(int16_t* out,int frames) {
        std::fill(out,out+frames*2,int16_t(0));
        if(hrtf.active()) hrtf.render(out,frames);
        for(size_t i=0;i<voices.size();++i) {
            auto& v=voices[i]; if(!v.on) continue;
            const bool reference=!v.reference.mono.empty();
            const auto& pcm=reference?v.reference.mono:bank[size_t(v.cue)].mono;
            if(pcm.empty()) { stop(i); continue; }
            if(hrtf.active()) {
                v.cursor+=frames;
                // Let the loopback renderer finish its HRTF filter tail instead
                // of truncating short clicks at their dry PCM boundary.
                if(!v.loop && v.cursor>=pcm.size()+256) stop(i);
                continue;
            }
            const float length=std::hypot(v.x,v.north);
            const float pan=length>.001f?std::clamp(v.x/length,-1.f,1.f):0;
            float left=std::sqrt((1-pan)*.5f)*v.gain, right=std::sqrt((1+pan)*.5f)*v.gain;
            if(size_t(v.cue)<cues::first_entity) {
                const double angle=(std::clamp(v.x,-1.f,1.f)*.5f+.5f)*3.14159265358979323846/2;
                left=float(std::cos(angle))*v.gain; right=float(std::sin(angle))*v.gain;
                if(v.cue==cues::Id::Footstep || v.cue==cues::Id::WallBump) left=right=v.gain;
            }
            for(int f=0;f<frames;++f) {
                if(v.cursor>=pcm.size()) {
                    if(v.loop) v.cursor=std::fmod(v.cursor,double(pcm.size()));
                    else { stop(i); break; }
                }
                const size_t n=size_t(v.cursor), next=n+1<pcm.size()?n+1:(v.loop?0:n);
                const float fraction=float(v.cursor-n), sample=pcm[n]*(1-fraction)+pcm[next]*fraction;
                // Saturating sum, with low-amplitude defaults and user-controlled mix levels.
                const float l=reference?v.reference.stereo[n*2]*v.gain:sample*left;
                const float r=reference?v.reference.stereo[n*2+1]*v.gain:sample*right;
                out[f*2]=int16_t(std::clamp(float(out[f*2])+l,-32767.f,32767.f));
                out[f*2+1]=int16_t(std::clamp(float(out[f*2+1])+r,-32767.f,32767.f));
                v.cursor+=v.pitch;
            }
        }
    }
};
Player* player=nullptr; // constructed/destroyed exclusively on audio worker
}
void configure(scape::Options options) {
    static std::optional<scape::Options> previous; // boot, then the single native owner
    if(previous && *previous==options) return;
    const bool old=nav_enabled.exchange(options.navigation);
    if(old!=options.navigation) ++audio_epoch;
    // Latest configuration is retried by the native periodic publication if full.
    const auto desired=options;
    if(configurations.push(std::move(options))) previous=desired;
}
uint64_t epoch() { return field_epoch.load(); }
void cancel_field() { ++field_epoch; }
std::unique_ptr<scape::Snapshot> acquire_snapshot() {
    std::unique_ptr<scape::Snapshot> value; spares.pop(value); return value;
}
void discard(std::unique_ptr<scape::Snapshot> snapshot) {
    // At most four buffers exist. A returned buffer therefore always has a
    // reservation in this four-slot SPSC path; no game thread cleanup/wait.
    if(snapshot) retired.push(std::move(snapshot));
}
bool publish(std::unique_ptr<scape::Snapshot>& snapshot) { return observations.push(std::move(snapshot)); }
void play(cues::Id cue,float x,float north,float gain,float pitch,float amplitude) {
    if(!nav_enabled.load() || size_t(cue)>=cues::count || !std::isfinite(x) || !std::isfinite(north) || !std::isfinite(gain) || !std::isfinite(pitch) || !std::isfinite(amplitude)) return;
    Request request{cue,x,north,gain,pitch,audio_epoch.load(),field_epoch.load(),false,amplitude}; requests.push(std::move(request));
}
void preview(cues::Id cue) {
    if(!nav_enabled.load() || size_t(cue)>=cues::count) return;
    Request request{cue,0,1,1,1,audio_epoch.load(),field_epoch.load(),true}; requests.push(std::move(request));
}
void walls(unsigned mask,float gain) { wall_gain=gain; wall_mask=mask&15; }
void counter(bool enabled,float gain) { counter_gain=gain; counter_enabled=enabled; }
bool hrtf_active() { return active_hrtf.load(); }
void pump(SDL_AudioStream* stream) {
    if(!stream) return;
    if(!player) {
        player=new Player;
        for(int i=0;i<4;++i) {
            auto buffer=std::make_unique<scape::Snapshot>(); buffer->rows.reserve(scape::max_rows);
            spares.push(std::move(buffer));
        }
    }
    auto& p=*player;
    scape::Options next; bool updated=false;
    while(configurations.pop(next)) { updated=true; }
    if(updated) p.configure(std::move(next));
    if(!p.prepared) return;
    const auto generation=field_epoch.load(),audio=audio_epoch.load();
    if(generation!=p.field_generation || audio!=p.audio_generation) {
        p.stop_field(); if(audio!=p.audio_generation) p.stop_all();
        sdl::SDL_ClearAudioStream(stream);
        p.field_generation=generation; p.audio_generation=audio;
    }
    std::unique_ptr<scape::Snapshot> snapshot;
    while(retired.pop(snapshot)) recycle(std::move(snapshot));
    while(observations.pop(snapshot)) p.accept(std::move(snapshot),now());
    Request request;
    for(int n=0;n<64 && requests.pop(request);++n) {
        if(!nav_enabled.load() || request.generation!=audio_epoch.load()) continue;
        if(!request.preview && request.field_generation!=field_epoch.load()) continue;
        const size_t index=size_t(request.cue);
        const float gain=request.gain*(request.preview?p.options.gains[index]*(index>=cues::first_entity?p.options.master:1):1);
        const size_t slot=request.preview?15:(index==0?0:index==1?1:index==8?8:index==9?9:6);
        p.start(slot,request.cue,request.x,request.north,gain,request.pitch,false,0,request.amplitude);
    }
    if(!nav_enabled.load() || !p.options.navigation) { p.stop_all(); sdl::SDL_ClearAudioStream(stream); return; }
    int queued=std::max(0,sdl::SDL_GetAudioStreamQueued(stream));
    constexpr int frames=441,bytes=frames*4,target=bytes*3;
    for(int chunk=0;chunk<3 && queued<target;++chunk,queued+=bytes) {
        if(audio_epoch.load()!=p.audio_generation || field_epoch.load()!=p.field_generation) break;
        p.maintain(now());
        std::array<int16_t,frames*2> pcm{}; p.render(pcm.data(),frames);
        sdl::SDL_PutAudioStreamData(stream,pcm.data(),bytes);
        if(audio_epoch.load()!=p.audio_generation || field_epoch.load()!=p.field_generation) {
            sdl::SDL_ClearAudioStream(stream); break;
        }
    }
}
void close() { delete player; player=nullptr; active_hrtf=false; }
}
