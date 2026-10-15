#pragma once
#include "cue_bank.h"
#include <memory>
namespace sound {
// Audio-worker-only OpenAL Soft loopback. Never opens a physical device or changes
// a process-wide OpenAL context. SDL remains the sole physical playback owner.
class Hrtf {
public:
    static constexpr size_t voices = 128;
    Hrtf(); ~Hrtf();
    bool open(const std::wstring& runtime, const cues::Bank& bank,float output_gain=2.f);
    bool active() const;
    void stop(size_t voice);
    void play(size_t voice, cues::Id cue, float x, float north, float gain, bool loop);
    void play_pcm(size_t voice,const std::vector<int16_t>& mono,float x,float north,float gain,bool loop);
    void position(size_t voice, float x, float north, float gain);
    void render(int16_t* stereo, int frames);
private:
    struct State; std::unique_ptr<State> state_;
};
}
