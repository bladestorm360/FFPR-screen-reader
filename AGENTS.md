# Native FFPR accessibility mod

Active source for all six games is this repository, on `master`. FF1–FF5 belong to bladestorm360; FF6 retains ZKline/BlindGuyNW attribution and inclusion permission recorded in `../OPEN_ISSUES.md`. The older managed repositories are paused references. Preserve existing uncommitted work. User explicitly authorized 2026-10-11: after the full audit, deploy all six games, commit/push everything to the public user-owned `FFPR-screen-reader` remote (create if absent), and make the user-owned FF1–FF5 remotes private. Preserve old history and existing named remotes.

Read `CODEX_MEMORY.md`, the shared `../../../AGENTS.md`, and `../../../codex/MODEL_POLICY.md`. Sol owns research, implementation, review and verification. Use Luna only for an economical bounded mechanical handoff; Astra only for the documented emergency criteria. Summarize any delegation, including outcome and remaining issues.

Build with `build.bat`, then `smoketest.bat`; deployment is separate (`deploy.bat [ffN]`). These scripts must never launch a game. Gameplay testing and runtime probes are user-side unless explicitly authorized. Complete the current audit and offline checks before requesting gameplay tests.

Use name-resolved IL2CPP references and `hooks::attach`; verify signatures and folded native bodies against matching dumps/analysis. Preserve the custom MinHook coexistence changes. Hooks own UI timing; bounded deferrals start from native events. Game pointers need lifetime checks. Centralize services and use them at call sites; no new deduplication without explicit permission.

Speak game information only when displayed, or under an explicit scoped user approval. Read active rendered fields, labels, comparison arrows and bar values rather than every parameter in a shared data class. Do not infer approval from old implementation. Results follow native phases and current displayed details; no early disclosure of later rewards. Keep Autodetail persistent, default off, and manual details/review available.

Scoped user approval, 2026-10-11: retain the optional Enemy HP Display modes and saved/default preferences in every game, despite HP not being rendered in native target panels. Numbers adds `HP current/maximum`; Percentage adds `remaining%`; Hidden adds nothing and must not query parameters. This exception covers enemy HP only, not hidden enemy conditions or other statistics. F5 cycles these modes during battle; the Battle submenu explains the disclosure. Scan/Libra follows the ordinary displayed-message reader.

Settings must use logical submenus, direct F8 keyboard entry and controller mode/context input ownership. Never add keyboard mod mode. Navigation must cancel at battle transition, preserve live story gates and map state, route between floors to each transition, and explain blocked routes without filtering away every useful destination.

Authoritative task/evidence records: `docs/AUDIT_2026_10.md`, existing `docs/PORT_STATUS.md`, `docs/INPUT_BINDINGS.md`, `docs/TESTING.md`, and `../tools/mapdump/`. Keep evidence and unresolved issues current; distinguish implemented, offline verified and gameplay confirmed.
