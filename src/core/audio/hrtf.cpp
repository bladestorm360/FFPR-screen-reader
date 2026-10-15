#include "hrtf.h"
#include "../log.h"
#include <windows.h>
#include "../../../third_party/openal-soft/include/AL/al.h"
#include "../../../third_party/openal-soft/include/AL/alc.h"
#include "../../../third_party/openal-soft/include/AL/alext.h"
#include <cmath>

namespace sound {
struct Hrtf::State {
    HMODULE module=nullptr; ALCdevice* device=nullptr; ALCcontext* context=nullptr;
    bool ready=false;
#define AL_FUNCS(X) X(alcGetProcAddress) X(alcIsExtensionPresent) X(alcCreateContext) X(alcDestroyContext) X(alcCloseDevice) X(alcGetIntegerv) X(alGenBuffers) X(alDeleteBuffers) X(alBufferData) X(alGenSources) X(alDeleteSources) X(alSourcei) X(alSourcef) X(alSource3f) X(alSourcePlay) X(alSourceStop) X(alDistanceModel) X(alListenerf) X(alGetError)
#define DECL(n) decltype(&::n) n=nullptr;
    AL_FUNCS(DECL)
#undef DECL
    LPALCLOOPBACKOPENDEVICESOFT loopback=nullptr;
    LPALCISRENDERFORMATSUPPORTEDSOFT format=nullptr;
    LPALCRENDERSAMPLESSOFT render=nullptr;
    PFNALCSETTHREADCONTEXTPROC set_thread=nullptr;
    std::array<ALuint,cues::count> buffers{};
    std::array<ALuint,voices> sources{};
    std::array<ALuint,voices> generated{};
    ~State() {
        if(context) {
            if(set_thread && set_thread(context)) {
                if(sources[0]) alDeleteSources(ALsizei(sources.size()),sources.data());
                if(buffers[0]) alDeleteBuffers(ALsizei(buffers.size()),buffers.data());
                if(generated[0]) alDeleteBuffers(ALsizei(generated.size()),generated.data());
                set_thread(nullptr);
            }
            alcDestroyContext(context);
        }
        if(device) alcCloseDevice(device);
        if(module) FreeLibrary(module);
    }
};
Hrtf::Hrtf() = default;
Hrtf::~Hrtf() = default;
bool Hrtf::open(const std::wstring& runtime, const cues::Bank& bank,float output_gain) {
    state_=std::make_unique<State>(); auto& s=*state_;
    s.module=LoadLibraryExW(runtime.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if(!s.module) return false;
#define LOAD(n) s.n=reinterpret_cast<decltype(s.n)>(GetProcAddress(s.module,#n)); if(!s.n) return false;
    AL_FUNCS(LOAD)
#undef LOAD
    if(!s.alcIsExtensionPresent(nullptr,"ALC_SOFT_loopback") || !s.alcIsExtensionPresent(nullptr,"ALC_EXT_thread_local_context")) return false;
    s.loopback=reinterpret_cast<decltype(s.loopback)>(s.alcGetProcAddress(nullptr,"alcLoopbackOpenDeviceSOFT"));
    s.format=reinterpret_cast<decltype(s.format)>(s.alcGetProcAddress(nullptr,"alcIsRenderFormatSupportedSOFT"));
    s.render=reinterpret_cast<decltype(s.render)>(s.alcGetProcAddress(nullptr,"alcRenderSamplesSOFT"));
    s.set_thread=reinterpret_cast<decltype(s.set_thread)>(s.alcGetProcAddress(nullptr,"alcSetThreadContext"));
    if(!s.loopback || !s.format || !s.render || !s.set_thread) return false;
    s.device=s.loopback(nullptr);
    if(!s.device || !s.alcIsExtensionPresent(s.device,"ALC_SOFT_HRTF") || !s.format(s.device,cues::rate,ALC_STEREO_SOFT,ALC_SHORT_SOFT)) return false;
    const ALCint attrs[]={ALC_FREQUENCY,cues::rate,ALC_FORMAT_CHANNELS_SOFT,ALC_STEREO_SOFT,
        ALC_FORMAT_TYPE_SOFT,ALC_SHORT_SOFT,ALC_HRTF_SOFT,ALC_TRUE,ALC_MONO_SOURCES,ALCint(voices),ALC_STEREO_SOURCES,0,0};
    s.context=s.alcCreateContext(s.device,attrs);
    if(!s.context || !s.set_thread(s.context)) return false;
    ALCint enabled=0; s.alcGetIntegerv(s.device,ALC_HRTF_SOFT,1,&enabled);
    if(enabled!=ALC_TRUE) return false;
    s.alDistanceModel(AL_NONE); // shared range/gain owns falloff; do not attenuate twice
    // One master multiplier after source gains: every HRTF effect/preview gets
    // exactly 2x, including legacy cues. Stereo fallback never passes here.
    s.alListenerf(AL_GAIN,output_gain);
    s.alGenBuffers(ALsizei(s.buffers.size()),s.buffers.data());
    for(size_t i=0;i<cues::count;++i) {
        const auto& pcm=bank[i].mono;
        s.alBufferData(s.buffers[i],AL_FORMAT_MONO16,pcm.data(),ALsizei(pcm.size()*2),cues::rate);
    }
    s.alGenSources(ALsizei(s.sources.size()),s.sources.data());
    s.alGenBuffers(ALsizei(s.generated.size()),s.generated.data());
    if(s.alGetError()!=AL_NO_ERROR) return false;
    for(const auto source:s.sources) {
        s.alSourcei(source,AL_SOURCE_RELATIVE,AL_TRUE);
        // This pinned OpenAL Soft accepts finite max gain above one. Match the
        // soundscape master (2x) times individual volume (2x), without a 50% cap.
        s.alSourcef(source,AL_MAX_GAIN,4);
    }
    s.ready=true; return true;
}
bool Hrtf::active() const { return state_ && state_->ready; }
void Hrtf::stop(size_t v) { if(active() && v<voices) state_->alSourceStop(state_->sources[v]); }
void Hrtf::position(size_t v,float x,float north,float gain) {
    if(!active() || v>=voices) return;
    auto& s=*state_; const float length=std::hypot(x,north);
    if(length<.001f) { x=0; north=1; } else { x/=length; north/=length; }
    s.alSource3f(s.sources[v],AL_POSITION,x,0,-north); // map north is ahead; no head tracking
    s.alSourcef(s.sources[v],AL_GAIN,gain);
}
void Hrtf::play(size_t v,cues::Id cue,float x,float north,float gain,bool loop) {
    if(!active() || v>=voices) return;
    auto& s=*state_; stop(v);
    s.alSourcei(s.sources[v],AL_BUFFER,ALint(s.buffers[size_t(cue)]));
    s.alSourcei(s.sources[v],AL_LOOPING,loop?AL_TRUE:AL_FALSE);
    s.alSourcef(s.sources[v],AL_PITCH,1); // active HRTF never shifts category/directional pitch
    position(v,x,north,gain); s.alSourcePlay(s.sources[v]);
}
void Hrtf::render(int16_t* stereo,int frames) { if(active()) state_->render(state_->device,stereo,frames); }
void Hrtf::play_pcm(size_t v,const std::vector<int16_t>& mono,float x,float north,float gain,bool loop) {
    if(!active() || v>=voices || mono.empty()) return;
    auto& s=*state_; stop(v);
    s.alSourcei(s.sources[v],AL_BUFFER,0);
    s.alBufferData(s.generated[v],AL_FORMAT_MONO16,mono.data(),ALsizei(mono.size()*2),cues::rate);
    s.alSourcei(s.sources[v],AL_BUFFER,ALint(s.generated[v]));
    s.alSourcei(s.sources[v],AL_LOOPING,loop?AL_TRUE:AL_FALSE);
    s.alSourcef(s.sources[v],AL_PITCH,1);
    position(v,x,north,gain); s.alSourcePlay(s.sources[v]);
}
}
