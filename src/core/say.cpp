#include <windows.h>
#include "say.h"
#include "speech.h"
#include "text.h"

namespace ffpr {
namespace {
std::wstring g_last;
SRWLOCK g_lock = SRWLOCK_INIT;
}

void say(std::wstring_view raw, bool interrupt)
{
    std::wstring line = text::clean(raw);
    if (line.empty()) return;
    AcquireSRWLockExclusive(&g_lock);
    g_last = line;
    ReleaseSRWLockExclusive(&g_lock);
    speech::request_load();
    speech::say(line.c_str(), interrupt);
}

void silence() { speech::silence(); }

const std::wstring& last_spoken() { return g_last; }

} // namespace ffpr
