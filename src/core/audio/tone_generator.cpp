// Ports FF1 Utils/ToneGenerator.cs.
//
// The arithmetic keeps the C# expression order and types (float volumes, double samples,
// truncating (short) casts, Math.Round's half-to-even) so every tone matches the C# mod's.
#include "tone_generator.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "sound_constants.h"

namespace sound::tone {
namespace {

constexpr double kPi = 3.14159265358979323846;   // Math.PI
constexpr int kMBig = 2147483647;                // int.MaxValue
constexpr int kMSeed = 161803398;

/// C# (short)(double): truncate toward zero. Every value here is within +-32767.
inline int16_t to_short(double v)
{
    return static_cast<int16_t>(static_cast<int32_t>(v));
}

} // namespace

// --- System.Random(seed) ------------------------------------------------------------------

DotNetRandom::DotNetRandom(int seed)
{
    int subtraction = (seed == (-kMBig - 1)) ? kMBig : std::abs(seed);
    int mj = kMSeed - subtraction;
    seeds_[55] = mj;
    int mk = 1;
    for (int i = 1; i < 55; i++) {
        int ii = (21 * i) % 55;
        seeds_[ii] = mk;
        mk = mj - mk;
        if (mk < 0) mk += kMBig;
        mj = seeds_[ii];
    }
    for (int k = 1; k < 5; k++) {
        for (int i = 1; i < 56; i++) {
            seeds_[i] -= seeds_[1 + (i + 30) % 55];
            if (seeds_[i] < 0) seeds_[i] += kMBig;
        }
    }
    inext_ = 0;
    inextp_ = 21;
}

int DotNetRandom::sample()
{
    int loc_next = inext_;
    int loc_nextp = inextp_;
    if (++loc_next >= 56) loc_next = 1;
    if (++loc_nextp >= 56) loc_nextp = 1;
    int ret = seeds_[loc_next] - seeds_[loc_nextp];
    if (ret == kMBig) ret--;
    if (ret < 0) ret += kMBig;
    seeds_[loc_next] = ret;
    inext_ = loc_next;
    inextp_ = loc_nextp;
    return ret;
}

double DotNetRandom::next_double()
{
    return sample() * (1.0 / kMBig);
}

double round_half_even(double x)
{
    double f = std::floor(x);
    double d = x - f;
    if (d > 0.5) return f + 1.0;
    if (d < 0.5) return f;
    return std::fmod(f, 2.0) == 0.0 ? f : f + 1.0;
}

// --- tones --------------------------------------------------------------------------------

Pcm thud_tone(int frequency, int duration_ms, float volume)
{
    const int rate = constants::SAMPLE_RATE;
    int samples = (rate * duration_ms) / 1000;
    if (samples <= 0) return {};
    int attack_samples = samples / 4;
    DotNetRandom random(42);
    Pcm out(static_cast<size_t>(samples));

    double filtered_noise = 0;
    for (int i = 0; i < samples; i++) {
        double t = static_cast<double>(i) / rate;
        double attack_linear = std::min(1.0, static_cast<double>(i) / attack_samples);
        double attack = attack_linear * attack_linear;
        double decay = static_cast<double>(samples - i) / samples;
        double envelope = attack * decay;

        double sine = std::sin(2 * kPi * frequency * t);
        double raw_noise = (random.next_double() * 2 - 1);
        filtered_noise = filtered_noise * 0.9 + raw_noise * 0.1;
        double noise = filtered_noise * 0.3 * attack;
        double value = (sine * 0.7 + noise) * volume * envelope;

        out[static_cast<size_t>(i)] = to_short(value * 32767);
    }
    return out;
}

Pcm click_tone(int /*frequency*/, int duration_ms, float volume)
{
    // The C# takes a frequency and never uses it: the click is pure decaying noise.
    int samples = (constants::SAMPLE_RATE * duration_ms) / 1000;
    if (samples <= 0) return {};
    DotNetRandom random(42);
    Pcm out(static_cast<size_t>(samples));
    for (int i = 0; i < samples; i++) {
        double decay = std::exp(-10.0 * i / samples);
        double noise = (random.next_double() * 2 - 1) * volume * decay;
        out[static_cast<size_t>(i)] = to_short(noise * 32767);
    }
    return out;
}

Pcm stereo_tone(int frequency, int duration_ms, float volume, float pan, bool sustain)
{
    const int rate = constants::SAMPLE_RATE;
    if (frequency <= 0) return {};
    int samples;
    if (sustain) {
        // Cycle-aligned for seamless looping.
        double samples_per_cycle = static_cast<double>(rate) / frequency;
        int target_samples = (rate * duration_ms) / 1000;
        int num_cycles = static_cast<int>(round_half_even(target_samples / samples_per_cycle));
        if (num_cycles < 1) num_cycles = 1;
        samples = static_cast<int>(round_half_even(num_cycles * samples_per_cycle));
    } else {
        samples = (rate * duration_ms) / 1000;
    }
    if (samples <= 0) return {};

    double pan_angle = pan * kPi / 2;
    float left_vol = volume * static_cast<float>(std::cos(pan_angle));
    float right_vol = volume * static_cast<float>(std::sin(pan_angle));

    Pcm out(static_cast<size_t>(samples) * 2);
    int attack_samples = samples / 10;
    for (int i = 0; i < samples; i++) {
        double t = static_cast<double>(i) / rate;
        double sine_value;
        if (sustain) {
            sine_value = std::sin(2 * kPi * frequency * t);
        } else {
            double attack = std::min(1.0, static_cast<double>(i) / attack_samples);
            double decay = static_cast<double>(samples - i) / samples;
            sine_value = std::sin(2 * kPi * frequency * t) * attack * decay;
        }
        out[static_cast<size_t>(i) * 2] = to_short(sine_value * left_vol * 32767);
        out[static_cast<size_t>(i) * 2 + 1] = to_short(sine_value * right_vol * 32767);
    }
    return out;
}

Pcm landing_ping(int frequency, int total_duration_ms, int ping_duration_ms, float volume, float pan)
{
    const int rate = constants::SAMPLE_RATE;
    if (frequency <= 0) return {};
    int total_samples = (rate * total_duration_ms) / 1000;
    int ping_samples = (rate * ping_duration_ms) / 1000;

    double samples_per_cycle = static_cast<double>(rate) / frequency;
    int num_cycles = static_cast<int>(round_half_even(ping_samples / samples_per_cycle));
    if (num_cycles < 1) num_cycles = 1;
    ping_samples = static_cast<int>(round_half_even(num_cycles * samples_per_cycle));

    if (total_samples <= ping_samples)
        total_samples = ping_samples + (rate * 50) / 1000;

    double pan_angle = pan * kPi / 2;
    float left_vol = volume * static_cast<float>(std::cos(pan_angle));
    float right_vol = volume * static_cast<float>(std::sin(pan_angle));

    Pcm out(static_cast<size_t>(total_samples) * 2, 0);   // the tail stays silent
    int attack_samples = ping_samples / 8;
    int decay_samples = ping_samples / 4;
    int decay_start = ping_samples - decay_samples;

    for (int i = 0; i < ping_samples; i++) {
        double t = static_cast<double>(i) / rate;
        double envelope = 1.0;
        if (i < attack_samples)
            envelope = static_cast<double>(i) / attack_samples;
        else if (i >= decay_start)
            envelope = static_cast<double>(ping_samples - i) / decay_samples;

        double sine_value = std::sin(2 * kPi * frequency * t) * envelope;
        out[static_cast<size_t>(i) * 2] = to_short(sine_value * left_vol * 32767);
        out[static_cast<size_t>(i) * 2 + 1] = to_short(sine_value * right_vol * 32767);
    }
    return out;
}

Pcm mono_to_stereo(const Pcm& mono)
{
    Pcm out(mono.size() * 2);
    for (size_t i = 0; i < mono.size(); i++) {
        out[i * 2] = mono[i];
        out[i * 2 + 1] = mono[i];
    }
    return out;
}

} // namespace sound::tone
