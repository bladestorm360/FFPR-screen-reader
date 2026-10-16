#pragma once
#include <algorithm>
#include <cmath>
namespace ffpr::fieldnav::autowalk {
struct Steering { float x=0, y=0; };
// Native bird steering: a -= x*.2*scale; cap 2*scale; apply a;
// then decay by .1. Countersteer before moving so a corner cannot drift a tile.
inline Steering steer(float world_x,float world_y,float angle,float acceleration,float left_scale=1,float right_scale=1) {
    const float target=std::atan2(-world_x,world_y)*57.29577951308232f;
    const float error=std::remainder(target-angle,360.f);
    if(std::fabs(error)<.05f && std::fabs(acceleration)<.05f)return {0,1};
    const float desired=std::clamp(error,-2.f,2.f);
    const float n=acceleration-desired;
    const float scale=std::max(.01f,n<0?left_scale:right_scale);
    return {std::clamp(n/(.2f*scale),-1.f,1.f),0};
}
}
