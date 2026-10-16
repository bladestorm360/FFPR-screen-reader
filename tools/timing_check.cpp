// check-sources:
// Actual production timer and numeric logger queue, without disk or game APIs.
#include <windows.h>
#include <cstdio>
#include <stdexcept>
#include <thread>
#include <vector>
#include <string>
#include <cstring>
namespace fixture_io {
std::string output;
BOOL WINAPI write(HANDLE,LPCVOID bytes,DWORD size,LPDWORD count,LPOVERLAPPED){output.append(static_cast<const char*>(bytes),size);*count=size;return TRUE;}
}
#define WriteFile fixture_io::write
#include "../src/core/log.cpp"
#undef WriteFile
#include "../src/core/hooks.h"
namespace hooks { void note_fault(const char*) {} }
namespace fixture {
std::vector<timing::Event> events;
void capture(const timing::Event& event){events.push_back(event);}
__declspec(noinline) int baseline(int i){return i+1;}
__declspec(noinline) int measured(int i){timing::Scope scope("warm callback");return i+1;}
}
int main(){
    int failures=0;
    const auto expect=[&](bool ok,const char* why){if(!ok){++failures;std::printf("FAIL %s\n",why);}};
    timing::enable(&fixture::capture);timing::frame=123;
    {timing::Scope outer("reader");{timing::Scope native("Getter",timing::Kind::Native,"Value",0x1234);Sleep(12);}}
    expect(fixture::events.size()==2,"nested slow native/reader scopes are both recorded");
    if(fixture::events.size()==2){const auto& child=fixture::events[0];const auto& parent=fixture::events[1];
        expect(child.parent_span==parent.span&&child.frame==123&&child.code==0x1234,"numeric span/frame/native-code correlation is retained");
        expect(parent.self<parent.elapsed-child.elapsed+timing::frequency/1000&&child.self==child.elapsed,
            "exclusive parent time excludes its nested native call");}
    fixture::events.clear();
    hooks::guarded("throwing callback",[]{timing::Scope child("throwing native",timing::Kind::Native);Sleep(7);throw std::runtime_error("fixture");});
    expect(timing::current==nullptr&&fixture::events.size()==2,"exception handling restores the scope stack and retains slow attribution");
    fixture::events.clear();timing::mark("Battle action begin");timing::observe_gap(.75);
    expect(fixture::events.size()==2&&fixture::events[0].kind==timing::Kind::Marker&&fixture::events[1].kind==timing::Kind::Gap,
        "rare context markers and out-of-hook frame intervals are retained");
    fixture::events.clear();timing::summary_started=timing::ticks()-6*timing::frequency;timing::summarize();
    expect(!fixture::events.empty()&&fixture::events[0].kind==timing::Kind::Summary,"observed-scope counts/totals/maxima survive a quiet interval");
    fixture::events.clear();
    constexpr int count=1000000;
    volatile int result=0;
    auto started=timing::ticks();for(int i=0;i<count;++i)result=fixture::baseline(i);
    const double baseline_ms=(timing::ticks()-started)*1000.0/timing::frequency;
    started=timing::ticks();for(int i=0;i<count;++i)result=fixture::measured(i);
    const double measured_ms=(timing::ticks()-started)*1000.0/timing::frequency;
    expect(result==count,"instrumentation preserves returned values");
    std::printf("One million identical warm callbacks: baseline %.3fms, timed %.3fms, added %.3fus/call (offline)\n",
        baseline_ms,measured_ms,(measured_ms-baseline_ms)*1000/count);
    expect(measured_ms-baseline_ms<1000,"normal-path instrumentation has sub-microsecond average overhead in this fixture");
    fixture::events.clear();
    {timing::Scope waiting("selected wait",timing::Kind::Mod,nullptr,0,true);Sleep(100);}
    expect(fixture::events.size()==1&&fixture::events[0].cpu>=0&&fixture::events[0].cpu<400000,
        "selected sleeping owner scope records wall time with little owner CPU time");
    fixture::events.clear();
    {timing::Scope busy("selected CPU",timing::Kind::Mod,nullptr,0,true);
     const auto end=timing::ticks()+timing::frequency/10;while(timing::ticks()<end)result=fixture::baseline(result);}
    expect(fixture::events.size()==1&&fixture::events[0].cpu>=300000,
        "selected busy owner scope records CPU work without confusing it with elapsed wait");
    fixture::events.clear();
    timing::observe_gap(.001);Sleep(80);timing::observe_gap(.08);
    expect(fixture::events.size()==1&&fixture::events[0].cpu>=0&&fixture::events[0].cpu<400000,
        "input gaps retain owner CPU delta across the same observed boundaries");
    started=timing::ticks();for(int i=0;i<10000;++i) {
        timing::observe_gap(.016);
        {timing::Scope mod("frame",timing::Kind::Mod,nullptr,0,true);}
        {timing::Scope native("native",timing::Kind::Original,nullptr,0,true);}
    }
    const double coarse_us=(timing::ticks()-started)*1000000.0/timing::frequency/10000;
    std::printf("Selected per-input CPU sampling: %.3fus/frame offline average (gap plus mod/native scopes); CPU clock resolution is OS-dependent\n",coarse_us);
    expect(coarse_us<100,"selected coarse CPU samples keep bounded sub-0.1ms average fixture cost");
    // Hold the actual logger lock and fill its bounded ring: neither path may
    // wait for a logger/disk worker or allocate an unbounded diagnostic backlog.
    timing::enable(&emit_timing);g_ready=true;g_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);
    AcquireSRWLockExclusive(&g_lock);started=timing::ticks();
    for(int i=0;i<1000;++i)timing::mark("contended");
    const double contended_ms=(timing::ticks()-started)*1000.0/timing::frequency;
    ReleaseSRWLockExclusive(&g_lock);
    expect(g_count==0&&g_dropped.load()==1000&&contended_ms<20,"held logger lock drops/counts diagnostics without blocking the producer");
    std::string label(200,'x');timing::mark(label.c_str(),13);label.assign(200,'y');
    expect(g_queue[0].is_timing&&std::strlen(g_queue[0].timing.name)==95&&g_queue[0].timing.name[0]=='x'&&g_queue[0].timing.value==13,
        "logger owns bounded label copies after temporary context strings change");
    for(size_t i=0;i<kQueueSize+100;++i)timing::mark("full");
    expect(g_count==kQueueSize&&g_dropped.load()==1101,"full diagnostic queue stays bounded with explicit counted drops");
    expect(fixture_io::output.empty(),"diagnostic producers never perform file writes or formatting");
    g_file=reinterpret_cast<HANDLE>(123);write_timing(g_queue[0].timing);g_file=INVALID_HANDLE_VALUE;
    expect(fixture_io::output.find("[Timing] context ")!=std::string::npos&&fixture_io::output.find("value=13")!=std::string::npos,
        "the logger worker serializes the owned numeric record without reading game pointers");
    TimingLine cpu_row{};cpu_row.kind=timing::Kind::Mod;cpu_row.cpu=123456;std::strcpy(cpu_row.name,"selected");
    g_file=reinterpret_cast<HANDLE>(123);write_timing(cpu_row);g_file=INVALID_HANDLE_VALUE;
    expect(fixture_io::output.find("owner-cpu=12.346ms")!=std::string::npos,"worker serializes owned CPU duration in100ns units");
    timing::sink=nullptr;g_ready=false;CloseHandle(g_wake);g_wake=nullptr;
    std::thread worker([&]{timing::enable(&fixture::capture);{timing::Scope scope("worker");Sleep(7);}timing::sink=nullptr;});worker.join();
    expect(timing::current==nullptr,"worker timing never shares or corrupts the native-owner scope stack");
    std::printf("Timing: nested/native/original attribution, unwind, frame gaps, summaries, warm overhead, lock/full pressure: %d failures\n",failures);
    return failures?1:0;
}
