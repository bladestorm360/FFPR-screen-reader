#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace sound::cues {
enum class Id : uint8_t { Footstep, WallBump, WallNorth, WallSouth, WallEast, WallWest,
    Beacon, BeaconBlocked, Counter, Landing, NPC, Chest, Exit, Door, Event, Trap, Vehicle, SavePoint, Count };
constexpr size_t count = static_cast<size_t>(Id::Count);
constexpr size_t first_entity = static_cast<size_t>(Id::NPC);
constexpr size_t entity_count = count - first_entity;
constexpr int rate = 22050;
struct Definition { const char* file; const wchar_t* label; const char* toggle; const char* volume; };
extern const std::array<Definition, count> definitions;
struct Clip { std::vector<int16_t> mono; bool custom = false; };
// Worker-only, bounded WAV decoder. PCM16 mono/stereo, 8–96 kHz, <= 4 seconds.
bool decode(const std::string& bytes, Clip& clip);
using Bank = std::array<Clip, count>;
Bank load(const std::wstring& root);
std::wstring module_root();
}
