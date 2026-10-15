#pragma once
#include "cue_bank.h"
#include "tone_generator.h"
namespace sound::legacy {
// Audio-worker-only reference synthesis. Stereo preserves the pre-soundscape
// samples/pans; mono is the same unpanned signal for optional HRTF processing.
struct Pcm { tone::Pcm stereo, mono; };
Pcm make(cues::Id cue,float pan=0,float gain=1,float pitch=1,float amplitude=1,bool hrtf=false);
}
