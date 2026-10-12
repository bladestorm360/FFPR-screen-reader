#pragma once
// Rotating file logger: formatting/nonblocking enqueue on producers, I/O on its worker.
//
// On startup the previous run's log is preserved: ffpr_screen_reader.log becomes .1, .1 becomes
// .2, and so on up to kKeep, with the oldest discarded. During a run the file also rotates
// once it passes kMaxBytes, so a runaway log can never fill the disk. That means the log
// from the run BEFORE a crash is still on disk after the game is restarted — which is
// exactly when you need it.
namespace log_ {

constexpr int    kKeep     = 5;              // ffpr_screen_reader.1.log .. .5.log
constexpr size_t kMaxBytes = 4u * 1024 * 1024;

void init();
void write(const char* fmt, ...);
void writeW(const wchar_t* prefix, const wchar_t* text);   // UTF-16 payloads (game strings)
void shutdown();

} // namespace log_

#define LOGF(...)     ::log_::write(__VA_ARGS__)
#define LOGW(p, t)    ::log_::writeW(p, t)
