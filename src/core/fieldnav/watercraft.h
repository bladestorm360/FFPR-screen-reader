#pragma once
namespace ffpr::fieldnav::landing_spots {
// Extracted transportation masters: FF1-3 row 2 is the auto-boarded canoe.
// FF2 row 5 shares type 5 but is the snowcraft. FF4/6 have no freeform sailing.
inline bool watercraft(int game,int id,int type) {
    if(game!=1 && game!=2 && game!=3 && game!=5) return false;
    return type==2 || type==6 || (game<=3 && type==5 && id==2);
}
inline bool legal_shore(bool walking,bool riding,bool landing,bool requires_landing) {
    return walking && !riding && (!requires_landing || landing);
}
}
