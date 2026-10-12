"""Fail the build if hid.dll could stop a game from starting.

Checks (BDFFHD's rule 2, for hid.dll):
  * every one of the 47 System32 hid.dll exports is exported by name;
  * the DLL imports only system libraries (Prism and SDL3 are dynamically loaded; MinHook is
    compiled in).
Usage: python tools/import_check.py out/hid.dll
"""
import os
import sys

import pefile

SYSTEM_OK = {
    "kernel32.dll", "user32.dll", "advapi32.dll", "ole32.dll", "oleaut32.dll", "shell32.dll",
    "shlwapi.dll", "winmm.dll", "ntdll.dll", "gdi32.dll", "bcrypt.dll", "ws2_32.dll",
}


def main(path):
    pe = pefile.PE(path)
    ours = {e.name.decode() for e in pe.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    real = pefile.PE(os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "System32", "hid.dll"))
    want = {e.name.decode() for e in real.DIRECTORY_ENTRY_EXPORT.symbols if e.name}
    missing = sorted(want - ours)
    bad = []
    for imp in getattr(pe, "DIRECTORY_ENTRY_IMPORT", []):
        name = imp.dll.decode().lower()
        if name.startswith("api-ms-win-") or name in SYSTEM_OK:
            continue
        bad.append(name)
    if missing:
        print("FAIL: hid.dll exports missing:", ", ".join(missing))
    if bad:
        print("FAIL: non-system imports:", ", ".join(bad))
    if missing or bad:
        return 1
    print(f"imports OK; {len(want)} of {len(want)} hid.dll exports present")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))
