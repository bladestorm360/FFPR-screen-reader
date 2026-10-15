"""Package one release zip for all six games (UNIFIED_MOD_PLAN.md, Phase 5).

    python tools/make_release.py 0.1.0

Explicit complete-bundle packaging (user-authorized dependency redistribution):
    python tools/make_release.py 0.9 --bundle-name "Final Fantasy Pixel Master Core Screen Reader V0.9" --include-prism
creates a named folder plus a 7-Zip-created .zip, with the pinned Prism runtime/notices,
SDL3 license, both README formats and a byte-verified SHA256 manifest. It never installs
or modifies dependencies in a game folder. The legacy positional-only command below
remains mod-only for separately installed prerequisites.

Runs nothing else: build.bat and smoketest.bat must have passed first (this script checks
that out\\hid.dll exists and is newer than every source file). The zip goes to
releases\\FFPR_ScreenReader_<version>.zip and holds, at its top level, the files that go in
each game folder:

    hid.dll (MinHook is built in), SDL3.dll, MinHook_LICENSE.txt, readme.md, and the pinned
    private .ffpr-deps/openal-soft runtime/notices. Prism v0.18.3 x64 is a separate prerequisite and is
    installed with install_prism.bat; it is not included in the mod release zip. uninstall.bat
    is NOT included (it targets this machine's Steam path).

The readme is the user's: this script never writes or edits it.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import subprocess
import sys
import zipfile
from openal_dependency import payload as openal_payload

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


def markdown_to_text(value):
    """Plain text companion to the simple, list-based player README."""
    value = re.sub(r"^#{1,6} +", "", value, flags=re.MULTILINE)
    value = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"\1: \2", value)
    return value.replace("**", "").replace("`", "")


def make_bundle(name, sevenzip):
    root = Path(ROOT)
    release_root = (root / "releases").resolve()
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9 ._-]{0,179}", name) or name.endswith((" ", ".")):
        raise ValueError("release name must be a single Windows folder name")
    folder = (release_root / name).resolve()
    archive = (release_root / (name + ".zip")).resolve()
    # Verify both absolute targets before any filesystem mutation. Keep prior releases.
    if folder.parent != release_root or archive.parent != release_root:
        raise ValueError("release paths must remain inside releases")
    if folder.exists() or archive.exists():
        raise ValueError("release folder/archive already exists; it will not be overwritten")
    sevenzip = sevenzip or shutil.which("7z") or r"C:\Program Files\7-Zip\7z.exe"
    if not Path(sevenzip).is_file():
        raise ValueError("7-Zip not found; provide --sevenzip with its installed executable")
    readme = root / "readme.md"
    companion = root / "ReadMe.txt"
    if companion.read_text(encoding="utf-8") != markdown_to_text(readme.read_text(encoding="utf-8")):
        raise ValueError("ReadMe.txt is out of date; convert the final README first")
    files = [
        (root / "out/hid.dll", "hid.dll"),
        (Path(RUNTIME) / "SDL3.dll", "SDL3.dll"),
        (root / "third_party/minhook/LICENSE.txt", "MinHook_LICENSE.txt"),
        (root / "third_party/sdl3/LICENSE.txt", "SDL3_LICENSE.txt"),
        (readme, "ReadMe.md"), (companion, "ReadMe.txt"),
    ]
    files.extend(openal_payload(Path(ROOT)))
    lock = json.loads((root / "prism.lock.json").read_text(encoding="utf-8"))
    prism_root = (root / "third_party/prism").resolve()
    for relative, entry in lock["files"].items():
        source = (prism_root / entry["source"]).resolve()
        part = PurePosixPath(relative)
        if not source.is_relative_to(prism_root) or part.is_absolute() or ".." in part.parts:
            raise ValueError("invalid Prism source/package path")
        if hashlib.sha256(source.read_bytes()).hexdigest() != entry["sha256"]:
            raise ValueError("pinned Prism file mismatch: " + relative)
        files.append((source, ".ffpr-deps/prism/" + relative))
    for source, _ in files:
        if not source.is_file():
            raise ValueError("required release file missing: " + str(source))
    release_root.mkdir(parents=True, exist_ok=True)
    folder.mkdir()
    for source, relative in files:
        target = folder / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
    notice = (
        "Prism " + lock["version"] + " Windows x64 runtime, redistributed unchanged.\n"
        "Source: " + lock["source_url"] + "/tree/" + lock["source_commit"] + "\n"
        "Official package: " + lock["release_asset"] + "\n"
        "Package SHA256: " + lock["archive_sha256"] + "\n"
        "Prism and bundled third-party licenses: LICENSES/\n"
    )
    (folder / ".ffpr-deps/prism/SOURCE.txt").write_bytes(notice.encode("utf-8"))
    payload = sorted((p for p in folder.rglob("*") if p.is_file()), key=lambda p: p.relative_to(folder).as_posix())
    manifest = "".join(hashlib.sha256(p.read_bytes()).hexdigest() + "  " + p.relative_to(folder).as_posix() + "\n" for p in payload)
    (folder / "SHA256SUMS.txt").write_bytes(manifest.encode("utf-8"))
    for command in ([sevenzip, "a", "-tzip", "-mx=9", archive.name, folder.name],
                    [sevenzip, "t", archive.name]):
        completed = subprocess.run(command, cwd=release_root, capture_output=True, text=True, errors="replace")
        if completed.returncode:
            raise ValueError("7-Zip failed:\n" + completed.stdout + completed.stderr)
    # Inspect the real 7-Zip output: one named folder, exact file list and bytes.
    expected = {name + "/" + p.relative_to(folder).as_posix(): p for p in folder.rglob("*") if p.is_file()}
    with zipfile.ZipFile(archive) as zipped:
        actual = {entry.filename for entry in zipped.infolist() if not entry.is_dir()}
        if actual != set(expected):
            raise ValueError("archive file list differs from the prepared release folder")
        for relative, source in expected.items():
            if hashlib.sha256(zipped.read(relative)).digest() != hashlib.sha256(source.read_bytes()).digest():
                raise ValueError("archive content mismatch: " + relative)
    print("Prepared " + str(folder))
    print("7-Zip archive verified: " + str(archive) + " (" + str(len(expected)) + " files)")
    print("Archive SHA256: " + hashlib.sha256(archive.read_bytes()).hexdigest())
    return 0


def main():
    parser = argparse.ArgumentParser(description="Package a verified FFPR build without launching games.")
    parser.add_argument("version")
    parser.add_argument("--bundle-name", help="Prepare this named folder and zip using 7-Zip.")
    parser.add_argument("--include-prism", action="store_true", help="Explicitly include the pinned prerequisite in the named bundle.")
    parser.add_argument("--sevenzip", help="Installed 7z.exe path.")
    args = parser.parse_args()
    version = args.version
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_.-]{0,63}", version):
        print("FAIL: invalid release version")
        return 1
    if args.bundle_name and not args.include_prism:
        print("FAIL: a complete named bundle requires explicit --include-prism")
        return 1
    if args.include_prism and not args.bundle_name:
        print("FAIL: --include-prism requires --bundle-name")
        return 1
    dll = os.path.join(ROOT, "out", "hid.dll")
    if not os.path.exists(dll):
        print("FAIL: out\\hid.dll missing - run build.bat and smoketest.bat first")
        return 1
    if os.path.getmtime(dll) < newest_source_time():
        print("FAIL: out\\hid.dll is older than the sources - run build.bat and smoketest.bat first")
        return 1
    if args.bundle_name:
        try:
            return make_bundle(args.bundle_name, args.sevenzip)
        except (OSError, ValueError) as error:
            print("FAIL: " + str(error))
            return 1
    files = [(dll, "hid.dll")]
    try:
        files.extend(openal_payload(Path(ROOT)))
    except (OSError, ValueError) as error:
        print("FAIL: " + str(error))
        return 1
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
