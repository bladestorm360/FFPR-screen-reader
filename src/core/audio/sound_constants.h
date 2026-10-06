#pragma once
// Ports FF1 Utils/SoundConstants.cs (audio constants for tone generation and playback).
//
// The C# tones were WAV byte arrays whose 44-byte header every consumer skipped before
// submitting (WAV_HEADER_SIZE). The native tones are bare PCM, so that constant has no use here.
// Pans in this file are the C# convention: 0.0 = left, 0.5 = centre, 1.0 = right.
namespace sound::constants {

/// Sample rate for all generated audio (Hz).
constexpr int SAMPLE_RATE = 22050;

/// Cap (bytes) for the reusable beacon scratch buffer in the engine.
constexpr int CHANNEL_BUFFER_SIZE = 32768;

/// Wall tone frequencies per direction (Hz): an E minor 7th chord, E3-G3-B3-D4.
/// NORTH = D4 (highest) and SOUTH = E3 (lowest) are the outer voices; WEST = G3 (left) and
/// EAST = B3 (right) are spread around the centre so left and right are easy to tell apart.
namespace WallToneFrequencies {
constexpr int NORTH = 294;
constexpr int SOUTH = 165;
constexpr int EAST = 247;
constexpr int WEST = 196;
}

/// Fletcher-Munson equal-loudness multipliers per direction, applied to BASE_VOLUME.
namespace WallToneVolumeMultipliers {
constexpr float BASE_VOLUME = 0.12f;
constexpr float NORTH = 1.00f;
constexpr float SOUTH = 0.70f;
constexpr float EAST = 0.85f;
constexpr float WEST = 0.85f;
}

/// Pan per direction (0.0 = left, 0.5 = centre, 1.0 = right).
namespace WallTonePan {
constexpr float NORTH = 0.5f;
constexpr float SOUTH = 0.5f;
constexpr float EAST = 1.0f;
constexpr float WEST = 0.0f;
}

/// Wall bump: a deep thud.
namespace WallBump {
constexpr int FREQUENCY = 27;
constexpr int DURATION_MS = 60;
constexpr float VOLUME = 0.506f;
}

/// Footstep: a light click.
namespace Footstep {
constexpr int FREQUENCY = 500;
constexpr int DURATION_MS = 25;
constexpr float VOLUME = 0.1436f;   // lowered ~57.5% from the original 0.338
}

/// Audio beacon.
namespace Beacon {
constexpr int FREQUENCY_NORTH = 400;
constexpr int FREQUENCY_SOUTH = 280;
constexpr int DURATION_MS = 60;
constexpr float MIN_VOLUME = 0.10f;
constexpr float MAX_VOLUME = 0.50f;
}

/// Wall tone timing.
namespace WallToneTiming {
constexpr int SUSTAIN_DURATION_MS = 200;
}

/// EXP counter beep (battle results rolling EXP): a short high beep then silence; looped, it
/// becomes a rapid tick.
namespace ExpCounter {
constexpr int FREQUENCY = 2000;
constexpr int BEEP_MS = 50;
constexpr int SILENCE_MS = 50;
constexpr float VOLUME = 0.15f;
}

/// Vehicle landing ping (sound::play_landing_ping). FF1 has no landing ping; these are FF5's
/// SoundConstants (LandingPingFrequencies.NORTH, LandingPingVolumeMultipliers.BASE_VOLUME,
/// LandingPingTiming), the only old mod that played one.
// FF5 SoundConstants.LandingPingFrequencies: one pitch per direction (north and south share
// the centre pan, so pitch is what tells them apart).
namespace LandingPingFrequencies {
constexpr int NORTH = 660;
constexpr int SOUTH = 440;
constexpr int EAST = 550;
constexpr int WEST = 520;
}

namespace LandingPing {
constexpr int FREQUENCY = 660;
constexpr float BASE_VOLUME = 0.132f;
constexpr int TOTAL_MS = 250;
constexpr int PING_MS = 80;
}

} // namespace sound::constants
