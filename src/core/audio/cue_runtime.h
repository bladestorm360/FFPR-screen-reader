#pragma once
#include "soundscape.h"
struct SDL_AudioStream;
namespace sound::runtime {
// Main/native owner: copied observations only, bounded queues, no SDK/filesystem.
void configure(scape::Options options);
uint64_t epoch();
void cancel_field();
// Four worker-prepared buffers. Acquisition/return never allocate or destroy a
// large row collection on the native owner. No spare means defer capture.
std::unique_ptr<scape::Snapshot> acquire_snapshot();
void discard(std::unique_ptr<scape::Snapshot> snapshot);
bool publish(std::unique_ptr<scape::Snapshot>& snapshot);
void play(cues::Id cue,float x=0,float north=1,float gain=1,float fallback_pitch=1,float amplitude=1);
void walls(unsigned mask,float gain);
void counter(bool enabled,float gain=1);
void preview(cues::Id cue);
bool hrtf_active();
// Existing audio worker only.
void pump(SDL_AudioStream* stream);
void close();
}
