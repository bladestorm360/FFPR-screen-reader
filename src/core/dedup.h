#pragma once
#include <string_view>

// The central deduplicator, shared by every game (UNIFIED_MOD_PLAN.md, "Deduplication").
//
// It replaces FF3/FF4's AnnouncementDeduplicator and the scattered lastAnnounced* variables of
// all five mods. Each check names its CONTEXT and what it compares. Every call answers
// "is this new?": true means announce, false means it repeats the last value in that context
// and is suppressed. With logging on, every suppression is written to the log with its
// context, so double or missing speech can be traced back to the hook that caused it.
//
// Prefer identity. Where the game gives one (a cursor index, an object), compare that:
// identical text can belong to two different events (two goblins with the same name).
//
// TEXT CHECKS: the user's decision of 2026-10-05 covers centralising the EXISTING text-based
// checks. A NEW text-based check (a context that did not compare text in the C# mods) needs
// the user's OK first. docs/DEDUP_INVENTORY.md lists the existing ones.
//
// Reset points live here, not in the readers: the core calls reset_scope(Scope::Menu) when a
// menu opens or closes, Scope::Battle at battle end, Scope::Map on a map change.
namespace dedup {

enum class Scope : unsigned char {
    Menu,       // reset when a menu opens/closes
    Battle,     // reset at battle start and end
    Map,        // reset on map change
    Dialogue,   // reset when a message window closes
    Session,    // never reset automatically
};

/// Cursor index (or any integer identity).
bool index(const char* ctx, long long value, Scope s = Scope::Menu);
/// Object identity (a game object pointer used ONLY as an identity, never dereferenced).
bool object(const char* ctx, const void* identity, Scope s = Scope::Menu);
/// Spoken text (existing text-based checks only - see above).
bool text(const char* ctx, std::wstring_view t, Scope s = Scope::Menu);
/// Index plus text: new when either changed.
bool index_text(const char* ctx, long long value, std::wstring_view t, Scope s = Scope::Menu);

/// Forget one context (the next value is always new).
void reset(const char* ctx);
/// Forget every context of a scope.
void reset_scope(Scope s);
void reset_all();

/// Log suppressed announcements (defaults to the "DebugLogging" setting).
void set_logging(bool on);

} // namespace dedup
