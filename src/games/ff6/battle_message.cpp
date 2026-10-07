// Ports FF6 Patches/BattleMessagePatches.cs (BattleCommandMessageController_KeyInput_SetSpeaker_Patch):
// the speaker's name in a battle conversation, which FF6 shows above the battle message box
// (BattleCommandMessageController.SetSpeaker; FF1-FF5 have no such method). The line itself is the
// core's battle message (src/core/battle/battle_message.cpp, SetMessage).
//
// The rest of that C# file is covered elsewhere, not here: damage, misses and healing
// (CreateDamageView), the hit count (CreateHitCount), statuses (BattleConditionController.Add),
// and the message box (BattleMenuController.SetCommadnMessage / BattleUIManager.SetCommandText, both
// of which only forward to BattleCommandMessageController.SetMessage in FF6) are the core's
// battle_message.cpp; dialogue and scroll messages are src/core/dialogue; the turn, target and
// battle-exit hooks are the core's battle_command.cpp / battle_state.cpp.
//
// Folded body: KeyInput and Touch BattleCommandMessageController.SetSpeaker share one RVA in FF6
// (0x493220, x2), so the detour class-checks self (the C# patched the KeyInput class only).
//
// The C# lastSpeaker check moves to dedup:: as it was: an empty speaker forgets the last one, the
// same speaker again is not repeated, and nothing else resets it (Scope::Session).
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/game.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"

namespace ffpr::ff6 {
namespace {

ref::Class  kCmdMsgKey{"Last.UI.KeyInput", "BattleCommandMessageController", game::kFF6};
ref::Method mSetSpeaker{kCmdMsgKey, "SetSpeaker", 1};

constexpr const char* kCtxSpeaker = "Battle.Speaker";   // lastSpeaker

void speaker_set(const std::wstring& raw)
{
    if (text::is_blank(raw)) {
        dedup::reset(kCtxSpeaker);
        return;
    }
    const std::wstring speaker = text::trim(raw);
    if (!dedup::text(kCtxSpeaker, speaker, dedup::Scope::Session)) return;   // skip duplicates
    LOGW(L"[Battle Speaker] ", speaker.c_str());
    ffpr::say(speaker, false);
}

using SetSpeaker_t = void (*)(void* self, void* speaker_name, const void* mi);
SetSpeaker_t o_SetSpeaker;

void d_SetSpeaker(void* self, void* speaker_name, const void* mi)
{
    o_SetSpeaker(self, speaker_name, mi);
    hooks::guarded("FF6 BattleCommandMessageController.SetSpeaker", [&] {
        if (!self || !kCmdMsgKey.is(self)) return;   // folded with the Touch class's SetSpeaker
        speaker_set(ref::str(speaker_name));
    });
}

int install()
{
    return hooks::attach(mSetSpeaker, &d_SetSpeaker, &o_SetSpeaker) ? 1 : 0;
}

core::Install s_install{"FF6.BattleMessage", &install, 500, game::kFF6};

} // namespace
} // namespace ffpr::ff6
