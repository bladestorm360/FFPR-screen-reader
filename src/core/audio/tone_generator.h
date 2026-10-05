#pragma once
#include <cstdint>
#include <vector>

// Ports FF1 Utils/ToneGenerator.cs: the mod's procedural tones, 16-bit PCM at
// constants::SAMPLE_RATE. Audio-area internal.
//
// The C# returned WAV byte arrays and every caller skipped the 44-byte header before
// submitting; these return the bare samples (interleaved left/right when stereo), which is
// exactly what was submitted. Pan is the C# convention: 0.0 = left, 0.5 = centre, 1.0 = right.
namespace sound::tone {

using Pcm = std::vector<int16_t>;

/// .NET's seeded System.Random (the subtractive generator new Random(seed) uses in .NET 6),
/// so the noise in the thud and click tones is sample-for-sample the C# mod's.
class DotNetRandom {
public:
    explicit DotNetRandom(int seed);
    double next_double();

private:
    int sample();
    int seeds_[56] = {};
    int inext_ = 0;
    int inextp_ = 21;
};

/// Math.Round(double): round half to even.
double round_half_even(double x);

/// Mono "thud": soft quadratic attack, low-passed noise mix and linear decay (wall bump).
Pcm thud_tone(int frequency, int duration_ms, float volume);

/// Mono filtered-noise click with exponential decay (footstep).
Pcm click_tone(int frequency, int duration_ms, float volume);

/// Stereo sine with constant-power panning. sustain = false: attack + decay envelope
/// (one-shots). sustain = true: cycle-aligned flat amplitude for seamless looping.
Pcm stereo_tone(int frequency, int duration_ms, float volume, float pan, bool sustain = false);

/// Stereo: a short cycle-aligned ping, then silence; looped, it pulses / ticks.
Pcm landing_ping(int frequency, int total_duration_ms, int ping_duration_ms, float volume, float pan);

/// Mono to stereo by duplicating each sample into both channels.
Pcm mono_to_stereo(const Pcm& mono);

} // namespace sound::tone
