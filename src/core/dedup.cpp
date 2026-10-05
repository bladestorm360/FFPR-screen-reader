#include <windows.h>
#include <string>
#include <unordered_map>
#include "dedup.h"
#include "log.h"
#include "text.h"

namespace dedup {
namespace {

enum class Kind : unsigned char { Index, Object, Text, IndexText };

struct Entry {
    Kind kind = Kind::Index;
    Scope scope = Scope::Menu;
    long long value = 0;
    const void* identity = nullptr;
    std::wstring text;
};

SRWLOCK g_lock = SRWLOCK_INIT;
std::unordered_map<std::string, Entry>* g_map = nullptr;
bool g_log = false;

std::unordered_map<std::string, Entry>& map()
{
    if (!g_map) g_map = new std::unordered_map<std::string, Entry>();
    return *g_map;
}

void note(const char* ctx, const char* what)
{
    if (g_log) LOGF("dedup: suppressed %s (%s)", ctx, what);
}

} // namespace

bool index(const char* ctx, long long value, Scope s)
{
    AcquireSRWLockExclusive(&g_lock);
    auto& m = map();
    auto it = m.find(ctx);
    bool fresh = it == m.end() || it->second.kind != Kind::Index || it->second.value != value;
    if (fresh) m[ctx] = Entry{Kind::Index, s, value, nullptr, {}};
    ReleaseSRWLockExclusive(&g_lock);
    if (!fresh) note(ctx, "same index");
    return fresh;
}

bool object(const char* ctx, const void* identity, Scope s)
{
    AcquireSRWLockExclusive(&g_lock);
    auto& m = map();
    auto it = m.find(ctx);
    bool fresh = it == m.end() || it->second.kind != Kind::Object || it->second.identity != identity;
    if (fresh) m[ctx] = Entry{Kind::Object, s, 0, identity, {}};
    ReleaseSRWLockExclusive(&g_lock);
    if (!fresh) note(ctx, "same object");
    return fresh;
}

bool text(const char* ctx, std::wstring_view t, Scope s)
{
    AcquireSRWLockExclusive(&g_lock);
    auto& m = map();
    auto it = m.find(ctx);
    bool fresh = it == m.end() || it->second.kind != Kind::Text || it->second.text != t;
    if (fresh) m[ctx] = Entry{Kind::Text, s, 0, nullptr, std::wstring(t)};
    ReleaseSRWLockExclusive(&g_lock);
    if (!fresh) note(ctx, "same text");
    return fresh;
}

bool index_text(const char* ctx, long long value, std::wstring_view t, Scope s)
{
    AcquireSRWLockExclusive(&g_lock);
    auto& m = map();
    auto it = m.find(ctx);
    bool fresh = it == m.end() || it->second.kind != Kind::IndexText ||
                 it->second.value != value || it->second.text != t;
    if (fresh) m[ctx] = Entry{Kind::IndexText, s, value, nullptr, std::wstring(t)};
    ReleaseSRWLockExclusive(&g_lock);
    if (!fresh) note(ctx, "same index and text");
    return fresh;
}

void reset(const char* ctx)
{
    AcquireSRWLockExclusive(&g_lock);
    map().erase(ctx);
    ReleaseSRWLockExclusive(&g_lock);
}

void reset_scope(Scope s)
{
    AcquireSRWLockExclusive(&g_lock);
    auto& m = map();
    for (auto it = m.begin(); it != m.end();) {
        if (it->second.scope == s) it = m.erase(it);
        else ++it;
    }
    ReleaseSRWLockExclusive(&g_lock);
}

void reset_all()
{
    AcquireSRWLockExclusive(&g_lock);
    map().clear();
    ReleaseSRWLockExclusive(&g_lock);
}

void set_logging(bool on) { g_log = on; }

} // namespace dedup
