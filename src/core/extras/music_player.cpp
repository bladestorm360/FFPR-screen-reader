// Ports FF1 Patches/MusicPlayerPatches.cs (MusicPlayerStateTracker, MusicPlayerManualPatches) and
// Menus/MusicPlayerReader.cs.
//
// The Extra Sound music player: entry ("Music Player" then the focused song), song navigation,
// the Play All toggle and the original/arrangement switch. SubSceneManagerExtraSound
// (Last.Management) and the KeyInput ExtraSound* classes have the same names and fields in all
// six games; no hooked body is folded.
//
// Cross-game difference: ExtraSoundListContentInfo.playTime is an int in FF1, FF2, FF3, FF5 and
// FF6 but a float in FF4 (KeyInput class, dump.cs). FF4 reads the float and truncates it, as the old
// FF4 mod did.
#include "extras.h"
#include <cstdlib>
#include "../core.h"
#include "../frame.h"
#include "../game.h"
#include "../hooks.h"
#include "../log.h"
#include "../modtext.h"
#include "../say.h"
#include "../text.h"
#include "../unity.h"
#include "../field/object_cache.h"

namespace ffpr::extras {
namespace {

ref::Class  kSubSceneSound{"Last.Management", "SubSceneManagerExtraSound"};
ref::Method mChangeState{kSubSceneSound, "ChangeState", 1};

ref::Class  kListContent{"Last.UI.KeyInput", "ExtraSoundListContentController"};
ref::Method mSetFocus{kListContent, "SetFocus", 1};
ref::Field  fContentInfo{kListContent, "<ContentInfo>k__BackingField"};
ref::Field  fIndex{kListContent, "<Index>k__BackingField"};

ref::Class  kContentInfo{"Last.UI.KeyInput", "ExtraSoundListContentInfo"};
ref::Field  fMusicName{kContentInfo, "musicName"};
ref::Field  fBgmId{kContentInfo, "bgmId"};
ref::Field  fPlayTime{kContentInfo, "playTime"};

ref::Class  kSoundController{"Last.UI.KeyInput", "ExtraSoundController"};
ref::Method mInitialize{kSoundController, "Initialize", 0};
using Initialize = void (*)(void*, const void*);
Initialize o_initialize;
void d_initialize(void* self, const void* mi) {
    hooks::guarded("Music.Owner", [self] {
        if (kSoundController.is(self)) field::cache::register_object(kSoundController, self);
    });
    o_initialize(self, mi);
}
ref::Method mChangeKeyHelpPlaybackIcon{kSoundController, "ChangeKeyHelpPlaybackIcon", 1};
ref::Field  fPlayerList{kSoundController, "<PlayerList>k__BackingField"};

ref::Class  kSoundList{"Last.UI.KeyInput", "ExtraSoundListController"};
ref::Method mSwitchOriginalArrangeList{kSoundList, "SwitchOriginalArrangeList", 0};
ref::Field  fCurrentListType{kSoundList, "currentListType"};

ref::Class  kSoundPlayerList{"Last.Data.Master", "SoundPlayerList"};
ref::Method mGetTime{kSoundPlayerList, "get_Time", 0};

// SubSceneManagerExtraSound.State
constexpr int STATE_VIEW = 1, STATE_GOTO_TITLE = 2;

// MusicPlayerStateTracker
bool g_in_music_player = false;
bool g_suppress_content_change = false;
void* g_cached_focused = nullptr;   // ExtraSoundListContentController; re-checked before use

// Entry announce (FF1 round 2, 2026-09-24 - replaces a 2-second poll). "Music Player" is spoken
// one frame after entry; the focused song follows as soon as SetFocus has reported it.
bool g_entry_header_spoken = false;
bool g_entry_pending = false;

// --- MusicPlayerReader -------------------------------------------------------------------------

/// C# "D{width}" for an int.
std::wstring pad(int v, int width)
{
    std::wstring digits = std::to_wstring(std::abs(static_cast<long long>(v)));
    while (static_cast<int>(digits.size()) < width) digits.insert(digits.begin(), L'0');
    return v < 0 ? L"-" + digits : digits;
}

struct Song {
    std::wstring name;
    int bgm_id = 0;
    int index = 0;
    int play_time = 0;
};

int read_play_time(void* info)
{
    if (game::is(game::Id::FF4)) {
        float f = ref::rd<float>(info, fPlayTime);
        return f > 0.0f ? static_cast<int>(f) : 0;
    }
    return ref::rd<int32_t>(info, fPlayTime);
}

/// ReadContentFromPointer: false when the content info or the name is missing.
bool read_content(void* content, Song& out)
{
    out = Song{};
    if (!content) return false;
    out.index = ref::rd<int32_t>(content, fIndex);
    void* info = ref::rd<void*>(content, fContentInfo);
    if (!info) return false;
    out.bgm_id = ref::rd<int32_t>(info, fBgmId);
    out.play_time = read_play_time(info);
    out.name = ref::rd_str(info, fMusicName);
    return !out.name.empty();
}

/// LookupDuration: SoundPlayerList.Time from ExtraSoundController.PlayerList, or 0.
int lookup_duration(int bgm_id)
{
    return hooks::guarded_r("MusicPlayerReader.LookupDuration", 0, [&] {
        void* controller = field::cache::get(kSoundController);
        if (!controller) return 0;
        void* dict = ref::rd<void*>(controller, fPlayerList);
        if (!dict) return 0;
        void* value = unity::dict_reference(dict, bgm_id);
        return value ? static_cast<int>(ref::call<int32_t>(mGetTime, value)) : 0;
    });
}

/// FormatPlayTime: "M:SS".
std::wstring format_play_time(int total_seconds)
{
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;
    return std::to_wstring(minutes) + L":" + pad(seconds, 2);
}

/// ReadSongEntry: "01: Main Theme, 1:30".
std::wstring song_entry(const Song& s)
{
    if (s.name.empty()) return {};
    std::wstring number = pad(s.index + 1, 2);
    // playTime from the content info (most reliable), else the master data.
    int duration = s.play_time > 0 ? s.play_time : lookup_duration(s.bgm_id);
    return number + L": " + s.name + L", " + format_play_time(duration);
}

bool cached_focus_alive()
{
    return g_cached_focused && kListContent.is(g_cached_focused) && unity::alive(g_cached_focused);
}

// --- entry -------------------------------------------------------------------------------------

void try_announce_entry_song_body()
{
    if (!g_entry_pending || !g_entry_header_spoken || !g_suppress_content_change) return;
    Song s;
    if (!cached_focus_alive() || !read_content(g_cached_focused, s)) return;

    g_entry_pending = false;
    g_suppress_content_change = false;
    std::wstring entry = song_entry(s);
    if (!entry.empty()) ffpr::say(entry, false);
}

/// Speaks the cached focused song after the "Music Player" header, then ends the entry
/// suppression. No-op until an entry can be read.
void try_announce_entry_song()
{
    bool completed = false;
    hooks::guarded("MusicPlayerPatches.TryAnnounceEntrySong", [&] {
        try_announce_entry_song_body();
        completed = true;
    });
    if (!completed) {   // the C# catch
        g_entry_pending = false;
        g_suppress_content_change = false;
    }
}

void start_entry_announce()
{
    g_entry_header_spoken = false;
    g_entry_pending = true;
    frame::next_frame([] {
        hooks::guarded("MusicPlayerPatches.Entry", [] {
            ffpr::say(T(L"Music Player"), true);
            g_entry_header_spoken = true;
            try_announce_entry_song();
        });
    }, "MusicPlayer.Entry");
}

// --- SubSceneManagerExtraSound.ChangeState(State) postfix -------------------------------------
using ChangeState_t = void (*)(void* self, int32_t state, const void* mi);
ChangeState_t o_ChangeState = nullptr;

void d_ChangeState(void* self, int32_t state, const void* mi)
{
    o_ChangeState(self, state, mi);
    hooks::guarded("MusicPlayerPatches.ChangeState", [&] {
        switch (state) {
        case STATE_VIEW:   // entering the music player
            g_in_music_player = true;
            g_suppress_content_change = true;
            menu_state::set_active_exclusive(menu_state::MUSIC_PLAYER);
            start_entry_announce();
            break;
        case STATE_GOTO_TITLE:   // leaving the music player
            clear_music_player_state();
            break;
        default:
            break;
        }
    });
}

// --- ExtraSoundListContentController.SetFocus(bool) postfix: song navigation -------------------
using SetFocus_t = void (*)(void* self, bool is_focus, const void* mi);
SetFocus_t o_SetFocus = nullptr;

void d_SetFocus(void* self, bool is_focus, const void* mi)
{
    o_SetFocus(self, is_focus, mi);
    hooks::guarded("MusicPlayerPatches.SetFocus", [&] {
        if (!is_focus) return;
        if (!g_in_music_player) return;
        if (g_suppress_content_change) {
            if (self) g_cached_focused = self;
            // Entry: once "Music Player" has been spoken, this read ends the entry suppression.
            try_announce_entry_song();
            return;
        }
        if (!self) return;

        Song s;
        if (!read_content(self, s)) return;
        std::wstring entry = song_entry(s);
        if (!entry.empty()) ffpr::say(entry, true);
    });
}

// --- ExtraSoundController.ChangeKeyHelpPlaybackIcon(LoopKeys) postfix: Play All toggle --------
using PlaybackIcon_t = void (*)(void* self, int32_t key, const void* mi);
PlaybackIcon_t o_PlaybackIcon = nullptr;

void d_PlaybackIcon(void* self, int32_t key, const void* mi)
{
    o_PlaybackIcon(self, key, mi);
    hooks::guarded("MusicPlayerPatches.ChangeKeyHelpPlaybackIcon", [&] {
        if (!g_in_music_player) return;
        // LoopKeys.PlaybackOn = 0 in all six games.
        ffpr::say(key == 0 ? T(L"Play All On") : T(L"Play All Off"), true);
    });
}

// --- ExtraSoundListController.SwitchOriginalArrangeList: prefix + postfix -----------------------
// The prefix holds the SetFocus announcements while the list swaps (SetFocus only caches the new
// focus); the postfix speaks the list name and the focused song, then releases the hold.
using SwitchList_t = void (*)(void* self, const void* mi);
SwitchList_t o_SwitchList = nullptr;

void switch_list_postfix(void* self)
{
    if (!g_in_music_player) return;
    if (!self) return;

    int list_type = ref::rd<int32_t>(self, fCurrentListType);
    ffpr::say(list_type == 1 ? T(L"Original") : T(L"Arrangement"), true);

    Song s;
    if (cached_focus_alive() && read_content(g_cached_focused, s)) {
        std::wstring entry = song_entry(s);
        if (!entry.empty()) ffpr::say(entry, false);
    }
}

void d_SwitchList(void* self, const void* mi)
{
    hooks::guarded("MusicPlayerPatches.SwitchOriginalArrangeList.Prefix", [] {
        if (g_in_music_player) g_suppress_content_change = true;
    });
    o_SwitchList(self, mi);
    hooks::guarded("MusicPlayerPatches.SwitchOriginalArrangeList", [&] { switch_list_postfix(self); });
    g_suppress_content_change = false;   // the C# finally
}

int install()
{
    queries::define("MusicPlayerStateTracker.IsInMusicPlayer", [] { return g_in_music_player; });

    int n = 0;
    n += hooks::attach(mInitialize, &d_initialize, &o_initialize) ? 1 : 0;
    n += hooks::attach(mChangeState, &d_ChangeState, &o_ChangeState) ? 1 : 0;
    n += hooks::attach(mSetFocus, &d_SetFocus, &o_SetFocus) ? 1 : 0;
    n += hooks::attach(mChangeKeyHelpPlaybackIcon, &d_PlaybackIcon, &o_PlaybackIcon) ? 1 : 0;
    n += hooks::attach(mSwitchOriginalArrangeList, &d_SwitchList, &o_SwitchList) ? 1 : 0;
    LOGF("[MusicPlayer] %d hooks", n);
    return n;
}

core::Install s_install{"MusicPlayerPatches", &install, 100};

} // namespace

bool is_in_music_player() { return g_in_music_player; }

void clear_music_player_state()
{
    g_in_music_player = false;
    g_suppress_content_change = false;
    g_cached_focused = nullptr;
    menu_state::reset(menu_state::MUSIC_PLAYER);
}

} // namespace ffpr::extras
