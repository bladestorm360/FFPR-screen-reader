// Ports FF1 Patches/PuzzlePatches.cs and Patches/PuzzleGameState.cs.
//
// The 15-puzzle minigame (Serial.FF1.MiniGame.MiniGamePuzzleController). The puzzle has its own
// cursor (not Last.UI.Cursor) and draws tile numbers as sprites, so neither the generic cursor
// reader nor text discovery can read it. The cursor-move methods are hooked; one frame later
// the tile under the cursor is spoken: its number ("7") or "empty".
// The on-demand position key (I / right stick up) runs "PuzzlePatches.AnnouncePosition".
//
// PuzzleGameState.IsActive is menu_state PUZZLE_GAME (query "PuzzleGameState.IsActive"). While
// it is on, menu_state::any_active() is true, which keeps field hotkeys off during the puzzle.
// IL2CppOffsets.Puzzle offsets are ref::Fields by name here.
#include <cstdint>
#include <string>
#include "../../core/core.h"
#include "../../core/dedup.h"
#include "../../core/frame.h"
#include "../../core/hooks.h"
#include "../../core/log.h"
#include "../../core/modtext.h"
#include "../../core/refs.h"
#include "../../core/say.h"
#include "../../core/text.h"
#include "../../core/unity.h"

namespace ffpr::ff1 {
namespace {

ref::Class kPuzzle{"Serial.FF1.MiniGame", "MiniGamePuzzleController", game::kFF1};
ref::Field fPieceList{kPuzzle, "pieceList"};
ref::Field fCursorPos{kPuzzle, "cursorPos"};
ref::Field fEmptyPiece{kPuzzle, "emptyPiece"};
ref::Method mShow{kPuzzle, "Show", 0};
ref::Method mClose{kPuzzle, "Close", 0};
ref::Method mNextPiece{kPuzzle, "NextPiece", 0};
ref::Method mPrevPiece{kPuzzle, "PrevPiece", 0};
ref::Method mSkipNextPeice{kPuzzle, "SkipNextPeice", 0};   // the game's spelling
ref::Method mSkipPrevPeice{kPuzzle, "SkipPrevPeice", 0};   // the game's spelling
ref::Method mTouchPiece{kPuzzle, "TouchPiece", 1};

ref::Class kPuzzlePiece{"Serial.FF1.MiniGame", "PuzzlePiece", game::kFF1};
ref::Field fPieceIndex{kPuzzlePiece, "index"};

constexpr int kBoardSize = 4;                      // MiniGamePuzzleController.boardSize
constexpr const char* kCtxTile = "Puzzle.Tile";    // lastAnnouncedCursorPos

// activeControllerPtr: the live controller, for AnnouncePosition. Cleared by the reset handler
// (Close and menu_state::reset_all), re-checked with unity::alive on every use.
void* g_controller = nullptr;
bool g_board_logged = false;   // one board snapshot per puzzle session

bool puzzle_active() { return menu_state::is_active(menu_state::PUZZLE_GAME); }

bool controller_alive(void* ctrl) { return ctrl && kPuzzle.is(ctrl) && unity::alive(ctrl); }

void reset_local_state()
{
    g_controller = nullptr;
    dedup::reset(kCtxTile);
    g_board_logged = false;
}

/// PieceIndexToLabel: PuzzlePiece.index + 1 is the printed number (tiles 1-15).
std::wstring piece_label(int piece_index) { return text::num(piece_index + 1); }

void log_snapshot(void* ctrl, int cursor_pos, int empty_pos)
{
    void* list = ref::rd<void*>(ctrl, fPieceList);
    int count = unity::list_count(list);
    LOGF("[Puzzle] SNAPSHOT cursorPos=%d emptyPiece=%d count=%d", cursor_pos, empty_pos, count);
    for (int i = 0; i < count; ++i) {
        void* p = unity::list_at(list, i);
        int idx = p ? ref::rd<int32_t>(p, fPieceIndex) : -999;
        LOGF("[Puzzle]  slot=%d index=%d", i, idx);
    }
}

/// AnnounceTile, one frame after a cursor move.
void announce_tile(void* ctrl)
{
    if (!puzzle_active() || !controller_alive(ctrl)) return;

    int cursor_pos = ref::rd<int32_t>(ctrl, fCursorPos);
    int empty_pos = ref::rd<int32_t>(ctrl, fEmptyPiece);

    if (!g_board_logged) {
        g_board_logged = true;
        hooks::guarded("Puzzle.Snapshot", [&] { log_snapshot(ctrl, cursor_pos, empty_pos); });
    }

    // Two move methods firing, or fast arrowing, must not repeat the same cell.
    if (!dedup::index(kCtxTile, cursor_pos, dedup::Scope::Session)) return;

    std::wstring label;
    if (cursor_pos == empty_pos) {
        label = T(L"empty");   // true however pieceList is ordered
    } else {
        void* piece = unity::list_at(ref::rd<void*>(ctrl, fPieceList), cursor_pos);
        if (!piece) return;
        label = piece_label(ref::rd<int32_t>(piece, fPieceIndex));
    }
    ffpr::say(label, true);
}

/// AnnouncePosition: the cursor's row and column (I key / right stick up).
void announce_position()
{
    void* ctrl = g_controller;
    if (!puzzle_active() || !controller_alive(ctrl)) return;
    int cursor_pos = ref::rd<int32_t>(ctrl, fCursorPos);
    int row = cursor_pos / kBoardSize + 1;
    int col = cursor_pos % kBoardSize + 1;
    ffpr::say(TF(L"Row {0}, Column {1}", {text::num(row), text::num(col)}), true);
}

// --- postfixes -------------------------------------------------------------------------------
void on_show(void* self)
{
    g_controller = self;
    dedup::reset(kCtxTile);
    g_board_logged = false;
    menu_state::set_active(menu_state::PUZZLE_GAME, true);
    LOGF("[Puzzle] Show - puzzle active");
}

void on_close()
{
    menu_state::reset(menu_state::PUZZLE_GAME);   // the reset handler clears the cached state
    LOGF("[Puzzle] Close - puzzle inactive");
}

void on_move(void* self)
{
    if (!self) return;
    g_controller = self;
    // One frame lets cursorPos and the move animation settle (TouchPiece, the move coroutine).
    frame::next_frame([self] { announce_tile(self); }, "Puzzle.AnnounceTile");
}

// --- detours ---------------------------------------------------------------------------------
using Void_t = void (*)(void* self, const void* mi);
using VoidInt_t = void (*)(void* self, int32_t arg, const void* mi);

Void_t o_Show, o_Close, o_NextPiece, o_PrevPiece, o_SkipNextPeice, o_SkipPrevPeice;
VoidInt_t o_TouchPiece;

void d_Show(void* self, const void* mi)
{
    o_Show(self, mi);
    hooks::guarded("Puzzle.Show", [&] { on_show(self); });
}

void d_Close(void* self, const void* mi)
{
    o_Close(self, mi);
    hooks::guarded("Puzzle.Close", [&] { on_close(); });
}

void d_NextPiece(void* self, const void* mi)
{
    o_NextPiece(self, mi);
    hooks::guarded("Puzzle.NextPiece", [&] { on_move(self); });
}

void d_PrevPiece(void* self, const void* mi)
{
    o_PrevPiece(self, mi);
    hooks::guarded("Puzzle.PrevPiece", [&] { on_move(self); });
}

void d_SkipNextPeice(void* self, const void* mi)
{
    o_SkipNextPeice(self, mi);
    hooks::guarded("Puzzle.SkipNextPeice", [&] { on_move(self); });
}

void d_SkipPrevPeice(void* self, const void* mi)
{
    o_SkipPrevPeice(self, mi);
    hooks::guarded("Puzzle.SkipPrevPeice", [&] { on_move(self); });
}

void d_TouchPiece(void* self, int32_t touch_pos, const void* mi)
{
    o_TouchPiece(self, touch_pos, mi);
    hooks::guarded("Puzzle.TouchPiece", [&] { on_move(self); });
}

int install()
{
    // The C# static constructor: fires on Close and on menu_state::reset_all (scene change,
    // abort), so the cached controller can never stick across sessions.
    menu_state::register_reset(menu_state::PUZZLE_GAME, [] { reset_local_state(); });

    int n = 0;
    n += hooks::attach(mShow, &d_Show, &o_Show) ? 1 : 0;
    n += hooks::attach(mClose, &d_Close, &o_Close) ? 1 : 0;
    n += hooks::attach(mNextPiece, &d_NextPiece, &o_NextPiece) ? 1 : 0;
    n += hooks::attach(mPrevPiece, &d_PrevPiece, &o_PrevPiece) ? 1 : 0;
    n += hooks::attach(mSkipNextPeice, &d_SkipNextPeice, &o_SkipNextPeice) ? 1 : 0;
    n += hooks::attach(mSkipPrevPeice, &d_SkipPrevPeice, &o_SkipPrevPeice) ? 1 : 0;
    n += hooks::attach(mTouchPiece, &d_TouchPiece, &o_TouchPiece) ? 1 : 0;
    LOGF("[Puzzle] %d of 7 hooks queued", n);

    queries::define("PuzzleGameState.IsActive", [] { return puzzle_active(); });
    actions::define("PuzzlePatches.AnnouncePosition", [] { announce_position(); });
    return n;
}

core::Install s_install{"FF1.Puzzle", &install, 502, game::kFF1};

} // namespace
} // namespace ffpr::ff1
