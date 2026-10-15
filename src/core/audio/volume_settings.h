#pragma once
#include "../settings.h"
#include <algorithm>
#include <string>
namespace sound::levels {
// Native-owner settings only. The existing volume rows select their saved bank
// from the requested HRTF mode; toggles/range and the worker never read this map.
// Legacy unprefixed values remain stereo. An unset HRTF value uses the same 50%
// default, independently of subsequent edits to the stereo bank.
inline std::string key(const char* name) {
    return settings::get_bool("HRTF",false)?std::string("HRTF.")+name:std::string(name);
}
inline int get(const char* name,int fallback=50) {
    const auto selected=key(name);
    return std::clamp(settings::get_int(selected.c_str(),fallback),0,100);
}
inline void set(const char* name,int value) {
    const auto selected=key(name);
    settings::set_int(selected.c_str(),std::clamp(value,0,100));
}
}
