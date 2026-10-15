#include "legacy_cues.h"
#include "sound_constants.h"
#include <algorithm>
#include <cmath>
namespace sound::legacy {
namespace {
tone::Pcm left(const tone::Pcm& stereo) {
    tone::Pcm mono(stereo.size()/2);
    for(size_t i=0;i<mono.size();++i) mono[i]=stereo[i*2];
    return mono;
}
tone::Pcm beacon(int frequency,float volume,float pan) {
    constexpr double pi=3.14159265358979323846;
    constexpr int n=constants::SAMPLE_RATE*constants::Beacon::DURATION_MS/1000;
    const double angle=pan*pi/2;
    const float l=volume*float(std::cos(angle)),r=volume*float(std::sin(angle));
    tone::Pcm out(n*2);
    for(int i=0;i<n;++i) {
        const double attack=std::min(1.0,double(i)/(n/10)),decay=double(n-i)/n;
        const double envelope=attack*decay;
        const double sample=std::sin(2*pi*frequency*(double(i)/constants::SAMPLE_RATE))*envelope;
        out[i*2]=int16_t(int32_t(sample*l*32767));out[i*2+1]=int16_t(int32_t(sample*r*32767));
    }
    return out;
}
}
Pcm make(cues::Id cue,float pan,float gain,float pitch,float amplitude,bool hrtf) {
    namespace C=constants;
    Pcm p;
    if(cue==cues::Id::Footstep || cue==cues::Id::WallBump) {
        p.mono=cue==cues::Id::Footstep ? tone::click_tone(C::Footstep::FREQUENCY,C::Footstep::DURATION_MS,C::Footstep::VOLUME)
            : tone::thud_tone(C::WallBump::FREQUENCY,C::WallBump::DURATION_MS,C::WallBump::VOLUME);
        p.stereo=tone::mono_to_stereo(p.mono); return p;
    }
    const float position=std::clamp(pan,-1.f,1.f)*.5f+.5f;
    if(cue==cues::Id::Beacon || cue==cues::Id::BeaconBlocked) {
        const int base=cue==cues::Id::Beacon?C::Beacon::FREQUENCY_NORTH:C::Beacon::FREQUENCY_NORTH/2;
        const int frequency=hrtf?base:int(std::lround(base*pitch));
        const float volume=C::Beacon::MAX_VOLUME*gain;
        p.stereo=beacon(frequency,volume,position);
        p.mono=left(beacon(frequency,volume,0)); return p;
    }
    if(cue==cues::Id::Landing) {
        const int frequency=hrtf?C::LandingPing::FREQUENCY:int(std::lround(C::LandingPing::FREQUENCY*pitch));
        const float volume=C::LandingPing::BASE_VOLUME*amplitude;
        p.stereo=tone::landing_ping(frequency,C::LandingPing::TOTAL_MS,C::LandingPing::PING_MS,volume,position);
        p.mono=left(tone::landing_ping(frequency,C::LandingPing::TOTAL_MS,C::LandingPing::PING_MS,volume,0)); return p;
    }
    if(cue==cues::Id::Counter) {
        p.stereo=tone::landing_ping(C::ExpCounter::FREQUENCY,C::ExpCounter::BEEP_MS+C::ExpCounter::SILENCE_MS,
            C::ExpCounter::BEEP_MS,C::ExpCounter::VOLUME,.5f);
        p.mono=left(tone::landing_ping(C::ExpCounter::FREQUENCY,C::ExpCounter::BEEP_MS+C::ExpCounter::SILENCE_MS,
            C::ExpCounter::BEEP_MS,C::ExpCounter::VOLUME,0)); return p;
    }
    const int d=int(cue)-int(cues::Id::WallNorth);
    if(d>=0 && d<4) {
        constexpr int frequencies[]={C::WallToneFrequencies::NORTH,C::WallToneFrequencies::SOUTH,C::WallToneFrequencies::EAST,C::WallToneFrequencies::WEST};
        constexpr float scales[]={C::WallToneVolumeMultipliers::NORTH,C::WallToneVolumeMultipliers::SOUTH,C::WallToneVolumeMultipliers::EAST,C::WallToneVolumeMultipliers::WEST};
        constexpr float pans[]={C::WallTonePan::NORTH,C::WallTonePan::SOUTH,C::WallTonePan::EAST,C::WallTonePan::WEST};
        const float volume=C::WallToneVolumeMultipliers::BASE_VOLUME*scales[d];
        p.stereo=tone::stereo_tone(frequencies[d],C::WallToneTiming::SUSTAIN_DURATION_MS,volume,pans[d],true);
        p.mono=left(tone::stereo_tone(frequencies[d],C::WallToneTiming::SUSTAIN_DURATION_MS,volume,0,true));
    }
    return p;
}
}
