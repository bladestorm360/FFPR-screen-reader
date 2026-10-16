#pragma once
// Automatic diagnostics only. No engine hooks, allocations, text extraction or
// I/O here. The existing logger copies slow numeric records without waiting.
#include <windows.h>
#include <atomic>
#include <cstdint>

namespace timing {
enum class Kind : unsigned { Mod, Native, Original, Gap, Marker, Summary };
struct Event {
    Kind kind;
    const char* name;
    const char* detail = nullptr;
    const char* parent = nullptr;
    uint64_t frame = 0, span = 0, parent_span = 0;
    int64_t started = 0, elapsed = 0, self = 0;
    int64_t cpu = -1; // optional owner CPU time in100ns units; -1 means unmeasured
    uintptr_t code = 0;
    DWORD thread = 0;
    uint64_t count = 1;
    int value = 0;
};
using Sink = void (*)(const Event&);
inline std::atomic<Sink> sink{nullptr};
inline std::atomic<uint64_t> frame{0};
inline int64_t frequency = 0; // immutable before enabling the sink
inline std::atomic<uintptr_t> game_base{0}, game_size{0};
inline int64_t ticks() { LARGE_INTEGER value{}; QueryPerformanceCounter(&value); return value.QuadPart; }
inline int64_t cpu_ticks() {
    FILETIME created{}, exited{}, kernel{}, user{};
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) return -1;
    return (static_cast<int64_t>(kernel.dwHighDateTime) << 32) + kernel.dwLowDateTime +
           (static_cast<int64_t>(user.dwHighDateTime) << 32) + user.dwLowDateTime;
}
inline void enable(Sink target) {
    LARGE_INTEGER value{}; QueryPerformanceFrequency(&value); frequency = value.QuadPart;
    sink.store(target, std::memory_order_release);
}
struct Total { uint64_t count = 0; int64_t elapsed = 0, self = 0, maximum = 0; };
inline thread_local Total totals[4]{};
inline thread_local int64_t summary_started = 0;
inline thread_local uint64_t serial = 0;
class Scope;
inline thread_local Scope* current = nullptr;

class Scope {
    Sink output_;
    Scope* parent_ = nullptr;
    Event event_{};
    int64_t children_ = 0;
    int64_t cpu_started_ = -1;
public:
    explicit Scope(const char* name, Kind kind = Kind::Mod, const char* detail = nullptr, uintptr_t code = 0, bool cpu = false)
        : output_(sink.load(std::memory_order_acquire)) {
        if (!output_) return;
        event_.kind=kind;event_.name=name;event_.detail=detail;event_.code=code;
        event_.thread=GetCurrentThreadId();event_.frame=frame.load(std::memory_order_relaxed);
        event_.span=++serial; // thread ID + span is unique; long runs do not wrap at32 bits
        parent_=current;
        if (parent_) {event_.parent=parent_->event_.name;event_.parent_span=parent_->event_.span;}
        current=this;event_.started=ticks();
        if(cpu)cpu_started_=cpu_ticks(); // only explicitly selected coarse paths
    }
    ~Scope() {
        if (!output_) return;
        event_.elapsed=ticks()-event_.started;
        if(cpu_started_>=0) { const auto end=cpu_ticks(); if(end>=cpu_started_)event_.cpu=end-cpu_started_; }
        event_.self=event_.elapsed>children_ ? event_.elapsed-children_ : 0;
        current=parent_;
        if (parent_) parent_->children_+=event_.elapsed;
        const auto index=static_cast<unsigned>(event_.kind);
        if (index<4) {
            auto& total=totals[index];++total.count;total.elapsed+=event_.elapsed;total.self+=event_.self;
            if(event_.elapsed>total.maximum)total.maximum=event_.elapsed;
        }
        if(event_.elapsed>=frequency/200)output_(event_); //5ms; fast calls never enqueue
    }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

inline void mark(const char* name, int value = 0) {
    if(const auto output=sink.load(std::memory_order_acquire)) {
        Event event{};event.kind=Kind::Marker;event.name=name;event.value=value;event.started=ticks();
        event.frame=frame.load(std::memory_order_relaxed);event.thread=GetCurrentThreadId();output(event);
    }
}
inline void observe_gap(double seconds) {
    if(const auto output=sink.load(std::memory_order_acquire)) {
        // One sample at the same input boundary each frame. This is owner CPU time,
        // not process-wide work, wall-time attribution or an engine/GC profiler.
        static thread_local int64_t previous_cpu = -1;
        const auto cpu=cpu_ticks(), start_cpu=previous_cpu;previous_cpu=cpu;
        const auto elapsed=static_cast<int64_t>(seconds*frequency);
        auto& total=totals[3];++total.count;total.elapsed+=elapsed;total.self+=elapsed;
        if(elapsed>total.maximum)total.maximum=elapsed;
        if(seconds>=.05) {
            Event event{};event.kind=Kind::Gap;event.name="Input update interval";event.started=ticks();
            event.elapsed=elapsed;event.self=event.elapsed;
            if(start_cpu>=0 && cpu>=start_cpu)event.cpu=cpu-start_cpu;
            event.frame=frame.load(std::memory_order_relaxed);event.thread=GetCurrentThreadId();output(event);
        }
    }
}
// Called by the existing owner tick. Totals describe observed scopes only;
// exclusive times avoid summing nested mod/native spans twice.
inline void summarize() {
    const auto output=sink.load(std::memory_order_acquire);if(!output)return;
    const auto now=ticks();if(!summary_started){summary_started=now;return;}
    if(now-summary_started<5*frequency)return;
    const char* labels[]={"Mod callbacks","Native reads","Native originals","Input intervals"};
    for(unsigned i=0;i<4;++i) {
        const auto total=totals[i];if(!total.count)continue;
        Event event{};event.kind=Kind::Summary;event.name=labels[i];event.started=summary_started;
        event.elapsed=total.elapsed;event.self=total.self;event.code=static_cast<uintptr_t>(total.maximum);
        event.count=total.count;event.frame=frame.load(std::memory_order_relaxed);event.thread=GetCurrentThreadId();output(event);
        totals[i]={};
    }
    summary_started=now;
}
}
