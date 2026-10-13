# Release procedure

When the user says **prepare release X.X**, complete this procedure through the public
GitHub release and uploaded ZIP. The user's October 13 instructions establish that
convention and authorize committing, pushing and publishing the requested release.
Repository renaming is a separate, one-time operation and is not part of this procedure.

Use the existing user-owned repository, branch and named remote. Check the worktree,
remote ownership/visibility, existing tags/releases and current project instructions
first. Preserve existing work/history and do not silently replace a published release.

1. Update the user-authorized player README from the actual keyboard/controller bindings.
   Keep the individual FF1-FF5 mods' concise Purpose, Known Issues, Install, Keys and
   Credits layout. Include I to describe mod-menu settings. Generate ReadMe.txt from the
   final Markdown and verify their content agrees.
2. Set FILEVERSION/PRODUCTVERSION and both strings in src/version.rc, and kBuild in
   src/dllmain.cpp. Preserve native architecture and dependency versions.
3. Run build.bat: imports/exports, all-six native-name checks, wiring and offline
   regressions must pass. Run smoketest.bat: isolated proxy/boot/logger must pass.
   Neither command launches a game. Gameplay confirmations remain separately recorded
   in TESTING.md. The October 13 user accepts the confirmed transition-lag fix as good
   enough and explicitly requests the V0.9 release; unreported cases remain unconfirmed.
4. Prepare a folder with the requested release name, containing the mod and all required
   runtime dependencies, notices and both README formats. Use the already-installed
   7-Zip through the complete-bundle command, substituting the requested version/name:

   python tools/make_release.py 0.9 --bundle-name "Final Fantasy Pixel Master Core Screen Reader V0.9" --include-prism --sevenzip "C:/Program Files/7-Zip/7z.exe"

5. The named folder and identically named .zip are created under releases. Existing
   artifacts are never overwritten. 7-Zip creates and tests the zip; byte/hash inspection
   verifies one top-level named folder and its exact payload.
6. Independently inspect ZIP CRCs, the exact file list, README/build-byte equality,
   SHA256SUMS.txt and pinned dependency hashes. Check x64 DLL version metadata. Record
   the results and DLL/archive hashes here. If artifacts must be regenerated, preserve
   the old folder/ZIP in a clearly named archive inside releases before rebuilding.
7. Review the intended source/documentation diff, commit it and push the existing branch
   to its named remote. Generated build directories, local logs, release files and raw
   commercial game investigation data remain ignored; distribute the ZIP as a release
   asset. Verify the pushed source commit and create/push an annotated vX.X tag at it.
8. Write concise release notes with installation instructions, README link and ZIP
   checksum. Create the GitHub release as a draft with the verified tag and attached ZIP.
   Check the draft's tag, title and asset name/size; download the uploaded ZIP into an
   ignored local verification directory and compare its SHA256 to the local archive.
   Publish the verified draft and check the public release URL and asset. If publication
   fails, preserve the verified local package and report the exact remaining step.
9. Record the release URL, tag/source commit, final hashes and verification here and link
   the current record from CODEX_MEMORY.md. Commit/push any final documentation record,
   then verify the worktree is clean and the remote branch contains the final commit.
   Give the user a short release link and completion report.

Build, smoke and packaging do not launch or control a game, deploy into game folders, or
install/upgrade dependencies. Gameplay confirmations and untested cases stay distinct in
TESTING.md. Use only the locked runtime files already present in the source tree.

## Complete V0.9 payload

- hid.dll: shared mod for all six games, including built-in MinHook and embedded map/data resources.
- SDL3.dll and SDL3_LICENSE.txt.
- MinHook_LICENSE.txt.
- ReadMe.md and its plain-text ReadMe.txt copy.
- .ffpr-deps/prism/prism.dll: locked official Prism 0.18.3 x64 runtime, unchanged.
- .ffpr-deps/prism/LICENSES: all eleven upstream license/notice files from prism.lock.json.
- .ffpr-deps/prism/SOURCE.txt: corresponding-source commit and upstream package attribution.
- SHA256SUMS.txt: checksums of every other payload file.

The user's explicit request to include all dependencies authorizes Prism redistribution
inside this release bundle. This supersedes the earlier mod-only packaging scope for this
release. It does not change normal build/deploy behavior: those verify Prism read-only and
never install/copy/repair/upgrade it. The separate install_prism.bat remains the only managed
installer. Packaging does not copy machine-specific installation receipts, settings, saves,
logs, game binaries, old Tolk wrappers or SDK/build tools. Speech loads the relative packaged
Prism path directly; consumer installation is copying the supplied contents into the game
folder, with no Python or separate dependency download.

The positional-only legacy make_release.py command remains mod-only for separately
installed prerequisites and preserves its offline separation regression. Complete player
releases use the explicit bundle command above.

## Initial V0.9 preparation record, October 13

Requested name: Final Fantasy Pixel Master Core Screen Reader V0.9.
Version resource: 0.9.0.0. Sol implements/reviews the release; no delegation or game launch.
Canonical V0.9 build and all offline regressions pass: 7,077 native-name checks/zero
missing, 122 used/327 defined action/query names/zero undefined, 47/47 proxy exports.
Isolated DLL smoke passes forwarding/boot/logger; no game was launched. PE metadata
verifies x64 and matching file/product 0.9.0.0, all-six description and Prism attribution.
The README keys were checked against current input registry/router/review/menu sources;
the plain-text conversion matches the final Markdown, including I and all credits.

Prepared folder: releases/Final Fantasy Pixel Master Core Screen Reader V0.9.
Archive: releases/Final Fantasy Pixel Master Core Screen Reader V0.9.zip.
7-Zip created and tested the 4,085,783-byte ZIP. Independent ZIP CRC, exact 20-file payload,
README/build-byte equality, manifest and every pinned Prism file hash all pass.

- DLL SHA256: `0aa1cc1578a2ffbb9ab82f1686360dc4637902fdbd6ff706dadfe85814cf110c`.
- ZIP SHA256: `ee4edadcf78b29c854b13a45d484cc3032695cb47adae74dd9727ddc433928dd`.

The release DLL differs from the user-confirmed installed 0e717ab build only in version/
build metadata, not feature behavior. This packaging operation did not modify game folders
or existing Prism installs. All-six direct gameplay coverage remains as recorded in TESTING.md.
Release assets are retained locally under the ignored releases folder; no public release
or upload was created. Source version/README/packaging changes are maintained in Git.

## One-time repository rename, October 13

The user subsequently requested public release publication and the repository name
FFPRCore-Screen-Reader. GitHub and the local source folder now use that name. GitHub
repository ID R_kgDOVFGmTg, public ownership bladestorm360, master history and source commit
22961bb were preserved. The named local remote is FFPRCore-Screen-Reader and points to
https://github.com/bladestorm360/FFPRCore-Screen-Reader.git. Git status and fsck confirmed
the completed local move; empty old-folder residue was removed after verifying it held
no files or reparse points. Active project/catalog links and player source URL were repaired.
Installed Prism receipt identity FFPR-screen-reader is deliberately unchanged for compatibility.

The old CMake cache/build output is retained in build_pre_repo_rename_20261013; a fresh
canonical build uses the new absolute source path. The initial folder/ZIP above is retained
in releases/archive_before_repo_rename_20261013. Rebuilt release assets use the corrected
README source link. These rename/recovery steps do not recur in the standard procedure.

## V0.9 rebuilt package and publication, October 13

The final folder/ZIP uses the requested name Final Fantasy Pixel Master Core Screen Reader
V0.9 under releases. Fresh canonical build and all offline regressions pass from the renamed
source: 7,077 native names/zero missing, 122 used/327 defined wiring names/zero undefined,
47/47 proxy exports. Isolated smoke passes forwarding/boot/logger. PE is x64 with file/product
version 0.9.0.0. No game launch, game-folder deployment or dependency install occurred.

7-Zip created and tested the 4,085,836-byte ZIP. Independent checks pass ZIP CRCs, the exact
20-file payload, source DLL/README bytes, the complete checksum manifest and pinned Prism
runtime/licenses/source notice. The corrected source URL and I setting help are in both
README formats; all credits are retained. Feature behavior matches the user-confirmed
0e717ab build, with release version/build metadata updated.

- DLL SHA256: `868dda2a88ab6382cf90dfe4e6b8d9e939b2eef0cfd1cef57abb26776bcca273`.
- ZIP SHA256: `c1e595403df3789e11a4fe2ace20217c075709c873bda739a723dc1aec8efb17`.

Publication is pending the verified source push/tag, draft asset download/hash check and
public release verification. Sol owns this work independently; no delegation.
