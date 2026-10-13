# Release procedure

Release preparation ends at a verified local folder and zip; the user ships it.

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
4. Prepare the requested complete bundle with the already-installed 7-Zip:

   python tools/make_release.py 0.9 --bundle-name "Final Fantasy Pixel Master Core Screen Reader V0.9" --include-prism --sevenzip "C:/Program Files/7-Zip/7z.exe"

5. The named folder and identically named .zip are created under releases. Existing
   artifacts are never overwritten. 7-Zip creates and tests the zip; byte/hash inspection
   verifies one top-level named folder and its exact payload.
6. Record DLL/archive hashes and the package contents here. Do not publish a release,
   launch a game or install/update a dependency as part of packaging.

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

## V0.9 preparation record, October 13

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
