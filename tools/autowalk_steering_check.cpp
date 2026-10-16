// Recovered native input integration, not a substitute for gameplay confirmation.
#include "../src/core/fieldnav/native_steering.h"
#include <cstdio>
#include <cmath>
#include <chrono>
int main() {
    int failures=0,tests=0,max_frames=0;
    for(float scale:{1.f,2.f,4.f})for(float initial:{-359.f,-180.f,-90.f,-1.f,0.f,1.f,90.f,180.f,359.f})
        for(float speed:{-2.f,-1.f,0.f,1.f,2.f})for(int direction=0;direction<4;++direction) {
        const float wx[]={0,1,0,-1},wy[]={1,0,-1,0};float angle=initial,a=speed;bool arrived=false;
        for(int frame=0;frame<900;++frame) {
            const auto input=ffpr::fieldnav::autowalk::steer(wx[direction],wy[direction],angle,a,scale,scale);
            const float error=std::remainder(std::atan2(-wx[direction],wy[direction])*57.29577951308232f-angle,360.f);
            if(input.y) { if(std::fabs(error)>.05f || std::fabs(a)>.05f)++failures;arrived=true;max_frames=std::max(max_frames,frame);break; }
            // Original native update: no position/camera write is made by the mod.
            a=std::clamp(a-input.x*.2f*scale,-2.f*scale,2.f*scale);
            angle+=a;a=std::copysign(std::max(std::fabs(a)-.1f,0.f),a);
        }
        ++tests;if(!arrived)++failures;
    }
    std::printf("Native steering: %d initial heading/momentum/direction/scale cases, max %d frames, %d failures\n",tests,max_frames,failures);
    return failures?1:0;
}
