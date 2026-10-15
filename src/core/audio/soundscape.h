#pragma once
#include "cue_bank.h"
#include <memory>
#include <optional>

namespace sound::scape {
constexpr size_t max_rows = 8192; // invalid-state guard, never a nearest-entity selection
enum class Kind { Chest, NPC, Exit, SavePoint, Event, Script, Vehicle };
struct Row {
    uint64_t id=0; Kind kind=Kind::Event;
    float x=0,north=0; // copied map coordinates in tiles, not a native pointer
    int destination=-1, layer=1;
    bool door=false, opened=false;
    std::wstring hint, event_type;
    bool verified_exit=false;
};
struct Snapshot {
    uint64_t epoch=0; double captured_at=0;
    float player_x=0,player_north=0,wrap_width=0,wrap_height=0;
    std::vector<Row> rows; // pathfinder All view order, current floor only
};
struct Options {
    bool operator==(const Options&) const = default;
    bool navigation=true, soundscape=false, hrtf=false;
    int range=10; float master=1;
    std::array<bool,cues::entity_count> enabled{};
    std::array<float,cues::count> gains{};
};
struct Emitter {
    uint64_t id=0; cues::Id cue=cues::Id::Event;
    float x=0,north=0,distance=0,detune=1;
    size_t rank=0;
};
float wrapped(float delta,float width);
bool trap_hint(std::wstring hint);
std::vector<Emitter> classify(const Snapshot& snapshot,const Options& options);
double cadence(const Emitter& emitter,double clip_seconds,float pitch);
}
