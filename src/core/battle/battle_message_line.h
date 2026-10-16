#pragma once
#include "../frame.h"
#include "../say.h"
#include <string>
#include <utility>

// Owner-thread action/message pairing. Opening conditions are produced synchronously
// by StartPreeMptiveMes in all six native bodies; they are not an action's name.
namespace ffpr::battle::message_line {
inline thread_local unsigned opening_depth = 0;
struct OpeningPresentation {
    OpeningPresentation() { ++opening_depth; }
    ~OpeningPresentation() { --opening_depth; }
    OpeningPresentation(const OpeningPresentation&) = delete;
    OpeningPresentation& operator=(const OpeningPresentation&) = delete;
};
constexpr int kPairFrames = 2;
struct PendingAction { bool set = false; std::wstring actor, action; int id = 0; };
struct PendingMessage { bool set = false; std::wstring text; bool interrupt = false; int id = 0; };
inline PendingAction pending_action;
inline PendingMessage pending_message;
inline int pair_id = 0;

inline void flush_action(int id) {
    if (!pending_action.set || pending_action.id != id) return;
    const auto a = std::move(pending_action); pending_action = {};
    ffpr::say(a.action.empty() ? a.actor : a.actor + L": " + a.action, false);
}
inline void flush_message(int id) {
    if (!pending_message.set || pending_message.id != id) return;
    const auto m = std::move(pending_message); pending_message = {};
    ffpr::say(m.text, m.interrupt);
}
inline void speak_paired(const std::wstring& actor, const std::wstring& text, bool interrupt) {
    ffpr::say(actor.empty() ? text : actor + L": " + text, interrupt);
}
inline void action_started(const std::wstring& actor, const std::wstring& action) {
    if (pending_message.set) {
        const auto m = std::move(pending_message); pending_message = {};
        speak_paired(actor, m.text, m.interrupt); return;
    }
    const int id = ++pair_id;
    pending_action = {true, actor, action, id};
    frame::after_frames(kPairFrames, [id] { flush_action(id); }, "Battle.ActionLine");
}
inline void game_message(const std::wstring& text, bool interrupt) {
    if (opening_depth) {
        // Queue at the native producer, before its postfix submits the roster.
        // This only classifies the source: no queued-speech scope wraps native code,
        // and explicit Control/silence or another manual action still interrupts.
        ffpr::say(text, interrupt); return;
    }
    if (pending_action.set) {
        const auto a = std::move(pending_action); pending_action = {};
        speak_paired(a.actor, text, interrupt); return;
    }
    const int id = ++pair_id;
    pending_message = {true, text, interrupt, id};
    frame::after_frames(kPairFrames, [id] { flush_message(id); }, "Battle.GameMessage");
}
}
