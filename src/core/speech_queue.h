#pragma once
#include "text_util.h"
#include "speech_timing.h"
#include <cstdint>

// Backend-independent fixed queue. The caller supplies the short synchronization scope.
namespace speech {
enum class Kind { Speech, Braille };
struct Line { wchar_t text[1200]{}; bool interrupt = false, silence = false; Kind kind=Kind::Speech; uint64_t generation = 0; Timing timing; };
class Queue {
public:
    static constexpr int Capacity = 1024; // A complete 512-row review page plus follow-up speech.
private:
    Line rows_[Capacity];
    int head_ = 0, count_ = 0;
    void push(const wchar_t* text, size_t length, bool interrupt, bool silence,Kind kind=Kind::Speech, uint64_t generation=0, Timing timing={}) {
        if (count_ == Capacity) {
            const bool dropped_interrupt = rows_[head_].interrupt;
            head_ = (head_+1)%Capacity; --count_;
            if (dropped_interrupt) {
                for(int i=0;i<count_;++i) if(rows_[(head_+i)%Capacity].kind==Kind::Speech) {
                    rows_[(head_+i)%Capacity].interrupt=true; break;
                }
            }
        }
        Line& line = rows_[(head_+count_)%Capacity];
        text_util::copy(line.text,1200,text,length);
        line.interrupt = interrupt; line.silence = silence; line.kind=kind; ++count_;
        line.generation = generation;
        line.timing = timing;
    }
public:
    void say(const wchar_t* text, bool interrupt, uint64_t generation=0, Timing timing={}) {
        if (!text || !*text) return;
        if (interrupt) head_ = count_ = 0;
        const wchar_t* cursor = text;
        for (;;) {
            const auto chunk = text_util::next_chunk(cursor,511);
            if (!chunk.length) break;
            push(chunk.text,chunk.length,interrupt,false,Kind::Speech,generation,timing);
            interrupt = false;
            timing.first = false;
        }
    }
    void silence(uint64_t generation=0) { head_ = count_ = 0; push(nullptr,0,true,true,Kind::Speech,generation); }
    void braille(const wchar_t* text, uint64_t generation=0) {
        const wchar_t* cursor=text;
        for(;;) {
            const auto chunk=text_util::next_chunk(cursor,1199);
            if(!chunk.length) break;
            push(chunk.text,chunk.length,false,false,Kind::Braille,generation);
        }
    }
    bool pop(Line& out) {
        if (!count_) return false;
        out = rows_[head_]; head_ = (head_+1)%Capacity; --count_; return true;
    }
    int size() const { return count_; }
};
} // namespace speech
