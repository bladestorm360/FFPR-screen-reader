#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <cstring>

// Native copied text only. Bounded operations never invoke the CRT abort handler.
namespace text_util {
inline bool high_surrogate(wchar_t c) { return c >= 0xd800 && c <= 0xdbff; }
inline bool low_surrogate(wchar_t c) { return c >= 0xdc00 && c <= 0xdfff; }

inline void copy(wchar_t* out, size_t capacity, const wchar_t* text, size_t limit = SIZE_MAX) {
    if (!out || !capacity) return;
    size_t n = 0;
    if (text) while (n < capacity - 1 && n < limit && text[n]) ++n;
    if (n && text && high_surrogate(text[n-1]) && low_surrogate(text[n])) --n;
    if (n) std::memmove(out, text, n * sizeof(wchar_t));
    out[n] = 0;
}
template<size_t N> void copy(wchar_t (&out)[N], const wchar_t* text) { copy(out,N,text); }
template<size_t N> void copy(wchar_t (&out)[N], const wchar_t* text, size_t limit) { copy(out,N,text,limit); }

inline void append(wchar_t* out, size_t capacity, const wchar_t* text, size_t limit = SIZE_MAX) {
    if (!out || !capacity || !text) return;
    size_t n = 0;
    while (n < capacity && out[n]) ++n;
    if (n == capacity) { out[capacity-1] = 0; return; }
    copy(out+n,capacity-n,text,limit);
}
template<size_t N> void append(wchar_t (&out)[N], const wchar_t* text) { append(out,N,text); }
template<size_t N> void append(wchar_t (&out)[N], const wchar_t* text, size_t limit) { append(out,N,text,limit); }
inline void append(char* out, size_t capacity, const char* text) {
    if (!out || !capacity || !text) return;
    size_t n=0;
    while (n<capacity && out[n]) ++n;
    if (n==capacity) { out[capacity-1]=0; return; }
    while (n<capacity-1 && *text) out[n++]=*text++;
    out[n]=0;
}
template<size_t N> void append(char (&out)[N], const char* text) { append(out,N,text); }

template<typename... Args> int format(char* out, size_t capacity, const char* fmt, Args... args) {
    if (!out || !capacity || !fmt) return -1;
    return _snprintf_s(out,capacity,_TRUNCATE,fmt,args...);
}
template<size_t N, typename... Args> int format(char (&out)[N], const char* fmt, Args... args) {
    return format(out,N,fmt,args...);
}

template<typename... Args> int format(wchar_t* out, size_t capacity, const wchar_t* fmt, Args... args) {
    if (!out || !capacity || !fmt) return -1;
    const int result = _snwprintf_s(out,capacity,_TRUNCATE,fmt,args...);
    const size_t n = wcslen(out);
    if (n && high_surrogate(out[n-1])) out[n-1] = 0;
    return result;
}
template<size_t N, typename... Args> int format(wchar_t (&out)[N], const wchar_t* fmt, Args... args) {
    return format(out,N,fmt,args...);
}

struct Chunk { const wchar_t* text; size_t length; };
// Linear scan: do not repeatedly strlen the unconsumed tail. Keep UTF-16 pairs intact.
inline Chunk next_chunk(const wchar_t*& cursor, size_t limit) {
    if (!cursor || !limit) return {nullptr,0};
    while (*cursor == L'\r' || *cursor == L'\n') ++cursor;
    const wchar_t* start = cursor;
    size_t n = 0;
    while (n < limit && start[n] && start[n] != L'\r' && start[n] != L'\n') ++n;
    if (n == limit && start[n] && start[n] != L'\r' && start[n] != L'\n') {
        size_t split = n;
        while (split > limit/2 && start[split] != L' ') --split;
        if (split > limit/2) n = split;
    }
    if (n && high_surrogate(start[n-1]) && low_surrogate(start[n])) --n;
    // All callers have room for a pair. Fail closed for an unusably small limit.
    if (!n) return {nullptr,0};
    cursor += n;
    while (*cursor == L' ') ++cursor;
    return {start,n};
}

template<int Rows, int Chars> struct Page {
    static_assert(Rows > 1 && Chars > 2);
    wchar_t row[Rows][Chars]{};
    int count = 0;
    void add(const wchar_t* text) {
        const wchar_t* cursor = text;
        for (;;) {
            const auto chunk = next_chunk(cursor,Chars-1);
            if (!chunk.length) break;
            if (count == Rows) {
                copy(row[Rows-1],L"Further details did not fit in the reader.");
                break;
            }
            copy(row[count++],Chars,chunk.text,chunk.length);
        }
    }
};
} // namespace text_util
