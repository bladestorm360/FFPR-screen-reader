#include "soundscape.h"
#include "../field/event_semantics.h"
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <unordered_set>
namespace sound::scape {
namespace {
struct Cell { int x,y,layer; bool operator==(const Cell&) const=default; };
struct Hash { size_t operator()(const Cell& c) const { return (size_t(uint32_t(c.x))*73856093u) ^ (size_t(uint32_t(c.y))*19349663u) ^ size_t(c.layer); } };
Cell cell(const Row& r) { return {int(std::lround(r.x)),int(std::lround(r.north)),r.layer}; }
bool coordinate(float value) { return std::isfinite(value) && std::abs(value)<=65536; }
}
float wrapped(float d,float width) { return width>0 ? d-std::round(d/width)*width : d; }
bool trap_hint(std::wstring hint) {
    return field::event_semantics::trap_identifier(hint);
}
std::vector<Emitter> classify(const Snapshot& s,const Options& o) {
    std::vector<Emitter> out;
    if(s.rows.size()>max_rows || !o.navigation || !o.soundscape || !coordinate(s.player_x) || !coordinate(s.player_north)
       || !coordinate(s.wrap_width) || !coordinate(s.wrap_height) || s.wrap_width<0 || s.wrap_height<0) return out;
    std::unordered_set<Cell,Hash> doors;
    for(const auto& row:s.rows) {
        if(!coordinate(row.x)||!coordinate(row.north)) continue;
        if(row.door && !trap_hint(row.hint)) doors.insert(cell(row));
    }
    auto adjacent=[&](const auto& cells,const Row& row) {
        const auto c=cell(row);
        for(int y=-1;y<=1;++y) for(int x=-1;x<=1;++x)
            if(cells.contains(Cell{c.x+x,c.y+y,c.layer})) return true;
        return false;
    };
    std::array<size_t,cues::entity_count> ranks{};
    for(const auto& r:s.rows) {
        if(!r.id || !coordinate(r.x)||!coordinate(r.north) || r.event_type==L"ToLayer") continue;
        const float x=wrapped(r.x-s.player_x,s.wrap_width),n=wrapped(r.north-s.player_north,s.wrap_height);
        const float distance=std::hypot(x,n);
        if(distance>std::clamp(o.range,5,25)) continue;
        std::optional<cues::Id> cue;
        switch(r.kind) {
        case Kind::Chest: if(!r.opened) cue=cues::Id::Chest; break;
        case Kind::NPC: cue=cues::Id::NPC; break;
        case Kind::Vehicle: cue=cues::Id::Vehicle; break;
        case Kind::SavePoint: cue=cues::Id::SavePoint; break;
        case Kind::Exit:
            if(trap_hint(r.hint)) cue=cues::Id::Trap;
            else if(r.verified_exit || r.destination>0) cue=(r.door || adjacent(doors,r))?cues::Id::Door:cues::Id::Exit;
            // A door guessed by the old scanner is not proof of a map exit.
            break;
        case Kind::Event: case Kind::Script:
            if(trap_hint(r.hint)) cue=cues::Id::Trap;
            else if(!r.door) cue=cues::Id::Event; // an adjacent exit owns the knock
            break;
        }
        if(!cue) continue;
        const size_t k=size_t(*cue)-cues::first_entity;
        if(!o.enabled[k]) continue;
        const size_t rank=ranks[k]++;
        const int slot=int(rank%13);
        const float detune=slot==0?1.f:1.f+(slot%2?1.f:-1.f)*float((slot+1)/2)*.05f;
        out.push_back({r.id,*cue,x,n,distance,detune,rank});
    }
    return out;
}
double cadence(const Emitter& e,double clip_seconds,float pitch) {
    const bool near=e.cue==cues::Id::Trap && e.distance<=2;
    return std::max(near?.60:1.0,clip_seconds/pitch+(near?.20:.35))+double(e.rank%13)*.10;
}
}
