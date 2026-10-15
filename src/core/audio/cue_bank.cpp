#include "cue_bank.h"
#include "../modtext.h"
#include "../log.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <filesystem>

namespace sound::cues {
const std::array<Definition, count> definitions = {{
    {"footstep",L"Footstep",nullptr,"FootstepVolume"}, {"wall_bump",L"Wall bump",nullptr,"WallBumpVolume"},
    {"wall_north",L"North wall",nullptr,"WallToneVolume"}, {"wall_south",L"South wall",nullptr,"WallToneVolume"},
    {"wall_east",L"East wall",nullptr,"WallToneVolume"}, {"wall_west",L"West wall",nullptr,"WallToneVolume"},
    {"beacon",L"Beacon",nullptr,"BeaconVolume"}, {"beacon_blocked",L"Blocked beacon",nullptr,"BeaconVolume"},
    {"exp_counter",L"EXP counter",nullptr,"ExpCounterVolume"}, {"landing",L"Landing ping",nullptr,"LandingPingVolume"},
    {"npc",L"NPCs","SoundscapeNPC","SoundscapeNPCVolume"},
    {"chest",L"Chests","SoundscapeChest","SoundscapeChestVolume"},
    {"exit",L"Map exits","SoundscapeExit","SoundscapeExitVolume"},
    {"door",L"Exit doors","SoundscapeDoor","SoundscapeDoorVolume"},
    {"event",L"Events","SoundscapeEvent","SoundscapeEventVolume"},
    {"trap",L"Traps","SoundscapeTrap","SoundscapeTrapVolume"},
    {"vehicle",L"Vehicles","SoundscapeVehicle","SoundscapeVehicleVolume"},
    {"save_point",L"Save points","SoundscapeSavePoint","SoundscapeSavePointVolume"}
}};
namespace {
uint16_t u16(const std::string& s,size_t n) { return uint8_t(s[n]) | (uint16_t(uint8_t(s[n+1]))<<8); }
uint32_t u32(const std::string& s,size_t n) { return u16(s,n) | (uint32_t(u16(s,n+2))<<16); }
}
bool decode(const std::string& bytes, Clip& clip) {
    constexpr size_t maximum = 4*96000*4+65536;
    if(bytes.size()<44 || bytes.size()>maximum || bytes.compare(0,4,"RIFF") || bytes.compare(8,4,"WAVE")) return false;
    const size_t end=size_t(u32(bytes,4))+8;
    if(end>bytes.size() || end<12) return false;
    unsigned channels=0, hz=0, align=0; size_t data=0,length=0;
    for(size_t at=12; at+8<=end;) {
        const size_t size=u32(bytes,at+4), start=at+8;
        if(size>end-start) return false;
        if(!bytes.compare(at,4,"fmt ")) {
            if(size<16 || u16(bytes,start)!=1 || u16(bytes,start+14)!=16) return false;
            channels=u16(bytes,start+2); hz=u32(bytes,start+4); align=u16(bytes,start+12);
            if((channels!=1 && channels!=2) || hz<8000 || hz>96000 || align!=channels*2 || u32(bytes,start+8)!=hz*align) return false;
        } else if(!bytes.compare(at,4,"data")) { data=start; length=size; }
        at=start+size+(size&1);
    }
    if(!data || !length || !align || length%align || length/align>size_t(hz)*4) return false;
    const size_t frames=length/align, output=frames*rate/hz;
    if(!output) return false;
    std::vector<int16_t> mono(output);
    auto sample=[&](size_t i) {
        i=std::min(i,frames-1); const size_t p=data+i*align;
        const int a=static_cast<int16_t>(u16(bytes,p));
        return channels==2 ? (a+static_cast<int16_t>(u16(bytes,p+2)))*.5f : float(a);
    };
    for(size_t i=0;i<output;++i) {
        const double p=double(i)*hz/rate; const size_t n=size_t(p); const float f=float(p-n);
        mono[i]=static_cast<int16_t>(sample(n)*(1-f)+sample(n+1)*f);
    }
    clip.mono=std::move(mono); return true;
}
std::wstring module_root() {
    HMODULE self=nullptr; wchar_t path[32768]{};
    if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&module_root),&self)) return {};
    const auto n=GetModuleFileNameW(self,path,32768);
    if(!n || n>=32768) return {};
    std::wstring result(path,n); return result.substr(0,result.find_last_of(L"\\/")+1);
}
Bank load(const std::wstring& root) {
    Bank result;
    for(size_t i=0;i<count;++i) {
        std::string resource="SOUND_"; for(const char c:std::string(definitions[i].file)) resource+=char(toupper(c));
        if(!decode(data::resource(resource.c_str()),result[i])) LOGF("[Audio] invalid embedded cue %s",definitions[i].file);
        const auto path=std::filesystem::path(root)/L"sounds"/(std::string(definitions[i].file)+".wav");
        std::error_code error; const auto size=std::filesystem::file_size(path,error);
        if(error) continue;
        if(size>4*96000*4+65536) { LOGF("[Audio] custom cue too large: %s",definitions[i].file); continue; }
        std::ifstream in(path,std::ios::binary); std::string bytes(size_t(size),'\0');
        if(!in.read(bytes.data(),std::streamsize(size))) continue;
        Clip custom;
        if(decode(bytes,custom)) { custom.custom=true; result[i]=std::move(custom); LOGF("[Audio] custom cue %s",definitions[i].file); }
        else LOGF("[Audio] invalid custom cue %s; using embedded default",definitions[i].file);
    }
    return result;
}
}
