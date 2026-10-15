// check-sources: fieldnav/move_state.cpp
// Actual explicit vehicle query with fake ready native transportation snapshots.
#include <cstdio>
#include "core.h"
#include "field/field_api.h"
#include "fieldnav/move_state.h"
#include "fieldnav/watercraft.h"
namespace fixture {
bool ready=false;
int reads=0;
field::VehicleState vehicle;
std::wstring name;
}
namespace core { Install::Install(const char* n,int (*f)(),int o,uint8_t g):name(n),fn(f),order(o),games(g){} }
namespace queries {
void define(const char*,std::function<bool()>) {}
void define_text(const char*,std::function<std::wstring()>) {}
}
namespace ffpr::system { bool is_field_navigation_ready() { return fixture::ready; } }
namespace field {
VehicleState current_vehicle() { ++fixture::reads; return fixture::vehicle; }
std::wstring vehicle_display_name(int id,int type,const std::wstring& message) {
    if(id!=fixture::vehicle.id || type!=fixture::vehicle.type || message!=fixture::vehicle.message_id) std::abort();
    return fixture::name;
}
}
namespace modtext { std::wstring T(std::wstring_view value) { return std::wstring(value); }
std::wstring TF(std::wstring_view,std::initializer_list<std::wstring>) { return {}; } }
namespace ffpr { void say(std::wstring_view,bool) { std::abort(); } }
int main() {
    using namespace ffpr::fieldnav::move_state;
    int failures=0;
    auto expect=[&](bool ok,const char* reason) { if(!ok) { ++failures; std::printf("FAIL %s\n",reason); } };
    using namespace ffpr::fieldnav::landing_spots;
    for(int game:{1,2,3,5}) expect(watercraft(game,4,2),"freeform ships supported in FF1/2/3/5");
    for(int game:{1,2,3}) expect(watercraft(game,2,5),"actual canoe master supported in FF1/2/3");
    expect(!watercraft(2,5,5)&&!watercraft(3,3,3)&&!watercraft(4,4,2)&&!watercraft(6,4,2),"snowcraft, aircraft and FF4/6 excluded");
    expect(legal_shore(true,false,true,true)&&legal_shore(true,false,false,false),"valid dock and canoe shore");
    expect(!legal_shore(false,false,true,true)&&!legal_shore(true,false,false,true)&&!legal_shore(true,true,true,true),"wall, forbidden ship shore and same water rejected");
    reset(); fixture::ready=true; fixture::vehicle={true,3,2,L"ShipName"}; fixture::name=L"Ship";
    expect(current_name()==L"Ship" && fixture::reads==1,"loaded boarded ship is named without a GetOn event");
    fixture::vehicle={true,5,5,L""}; fixture::name=L"Snowcraft";
    expect(current_name()==L"Snowcraft","actual transportation id/name survives a shared canoe type");
    fixture::vehicle={true,9,3,L"AirshipName"}; fixture::name=L"Airship";
    expect(current_name()==L"Airship","loaded airship uses the current native vehicle");
    set_vehicle_state(2,L"Old ship"); fixture::vehicle={true,1,1,L""}; fixture::name.clear();
    expect(current_name()==L"Walking","loaded native player row overrides an earlier boarding cache");
    fixture::vehicle={true,3,2,L""}; fixture::name.clear();
    expect(current_name()==L"Ship","unnamed current vehicle retains the known movement-type fallback");
    fixture::ready=false; int reads=fixture::reads;
    expect(current_name()==L"Old ship" && fixture::reads==reads,"unready/menu context performs no native transport read and retains existing behavior");
    fixture::ready=true; fixture::vehicle={};
    expect(current_name()==L"Old ship","missing native ownership preserves the prior fallback without manufacturing state");
    std::printf("Vehicle query: boarded loads, native walking replacement, shared type/name and readiness; %d failures\n",failures);
    return failures?1:0;
}
