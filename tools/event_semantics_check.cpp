// check-sources: field/baked_map.cpp audio/soundscape.cpp json.cpp text.cpp
// All-six extracted identifiers: Python baker, router and entity soundscape agree.
#include "field/baked_map.h"
#include "field/event_semantics.h"
#include "audio/soundscape.h"
#include "json.h"
#include <fstream>
#include <cstdio>

std::string read(const std::string& path) { std::ifstream f(path,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}}; }
int main() {
    int failures=0,total=0;
    const auto expect=[&](bool ok,const char* message){if(!ok){++failures;std::printf("FAIL %s\n",message);}};
    for(int n=1;n<=6;++n) {
        const auto game="ff"+std::to_string(n);
        json::Value audit,labels;
        expect(json::parse(read("docs/re/"+game+"_event_label_audit.json"),audit),"reviewed all-six identifier audit parses");
        expect(json::parse(read("data/"+game+"/eventlabels.json"),labels),"all-six label table parses");
        int hazards=0,metadata=0,conversations=0;
        for(const auto& row:audit.arr) {
            const auto& raw=row.get(L"raw")->s;
            const bool hazard=row.get(L"trap_hint")->b;
            expect(field::event_semantics::trap_identifier(raw)==hazard,"native identifier classifier matches label-baker corpus");
            expect(sound::scape::trap_hint(raw)==hazard,"soundscape and router use the same environmental evidence");
            hazards+=hazard;++total;
            if(row.get(L"type")->n==20) {++metadata;expect(row.get(L"label")->s!=L"Warp Tile","Teleport-cache metadata is not labelled a playable warp");}
            if(row.get(L"label")->s==L"Conversation") {
                ++conversations;
                expect(row.get(L"reason")->s==L"conversation identifier; actors/outcome withheld","Msg alone never creates a Conversation label");
            }
        }
        const auto bytes=read("data/"+game+"/maps.bin");
        expect(field::baked::load(bytes.data(),bytes.size()),"matching bake loads");
        int transitions=0,traps=0;
        for(int i=0;i<field::baked::submap_count();++i)for(const auto& object:field::baked::grids(*field::baked::submap_at(i)).objects) {
            if(object.type!=field::baked::ObjType::Warp)continue;
            ++transitions;traps+=field::baked::is_trap(object);
            expect(field::baked::is_trap(object)==(object.d==1&&field::event_semantics::trap_identifier(field::baked::str(object.name))),
                "actual script travel is a trap only with step-on environmental failure evidence");
        }
        std::printf("FF%d: %zu identifiers, %d environmental hints, %d cache markers excluded, %d explicit conversations; %d script transitions, %d trap arcs\n",n,audit.arr.size(),hazards,metadata,conversations,transitions,traps);
    }
    for(const auto* identifier:{L"ワープ",L"2F①へワープ",L"sc_e_0041:バッツが転送された",L"ティナが洞窟に落ちる",L"sc_e_0935:橋を渡ると崩れて、ドマ城へ。カイエンの家族とのイベント",L"Forced battle",L"sc_e_0155:カロフィステリと戦闘(穴が開く)"})
        expect(!sound::scape::trap_hint(identifier),"trial/progress warps, actor falls and battles remain ordinary events");
    std::printf("Event semantics: %d extracted candidates; %d failures\n",total,failures);
    return failures?1:0;
}
