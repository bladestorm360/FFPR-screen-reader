"""Package one release zip for all six games (UNIFIED_MOD_PLAN.md, Phase 5).

    python tools/make_release.py 0.1.0

Runs nothing else: build.bat and smoketest.bat must have passed first (this script checks
that out\\hid.dll exists and is newer than every source file). The zip goes to
releases\\FFPR_ScreenReader_<version>.zip and holds, at its top level, the files that go in
each game folder:

    hid.dll (MinHook is built in), SDL3.dll, MinHook_LICENSE.txt, and readme.md (the user's
    combined readme, only if it exists). Prism v0.18.3 x64 is a separate prerequisite and is
    installed with install_prism.bat; it is not included in the mod release zip. uninstall.bat
    is NOT included (it targets this machine's Steam path).

The readme is the user's: this script never writes or edits it.
"""
import os
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RUNTIME = os.path.join(ROOT, "third_party", "runtime")


def newest_source_time():
    newest = 0.0
    for dp, _, fn in os.walk(os.path.join(ROOT, "src")):
        for f in fn:
            newest = max(newest, os.path.getmtime(os.path.join(dp, f)))
    for dp, _, fn in os.walk(os.path.join(ROOT, "data")):
        for f in fn:
            newest = max(newest, os.path.getmtime(os.path.join(dp, f)))
    return newest


def main():
    if len(sys.argv) != 2:
        print("usage: python tools/make_release.py <version>")
        return 2
    version = sys.argv[1]
    dll = os.path.join(ROOT, "out", "hid.dll")
    if not os.path.exists(dll):
        print("FAIL: out\\hid.dll missing - run build.bat and smoketest.bat first")
        return 1
    if os.path.getmtime(dll) < newest_source_time():
        print("FAIL: out\\hid.dll is older than the sources - run build.bat and smoketest.bat first")
        return 1
    files = [(dll, "hid.dll")]
    for name in ("SDL3.dll",):
        path = os.path.join(RUNTIME, name)
        if not os.path.exists(path):
            print(f"FAIL: third_party\\runtime\\{name} missing")
            return 1
        files.append((path, name))
    # MinHook is compiled into hid.dll: its BSD-2 license must ship with the binary.
    files.append((os.path.join(ROOT, "third_party", "minhook", "LICENSE.txt"), "MinHook_LICENSE.txt"))
    readme = os.path.join(ROOT, "readme.md")
    if os.path.exists(readme):
        files.append((readme, "readme.md"))
    else:
        print("note: no readme.md in the repo root (the combined readme is the user's to write)")
    out_dir = os.path.join(ROOT, "releases")
    os.makedirs(out_dir, exist_ok=True)
    out = os.path.join(out_dir, f"FFPR_ScreenReader_{version}.zip")
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for src, arc in files:
            z.write(src, arc)
    print(f"wrote {os.path.relpath(out, ROOT)} ({len(files)} files)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
