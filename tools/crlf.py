"""Give batch files CRLF line endings (cmd.exe mis-parses labels in LF-only files).
Usage: python tools/crlf.py file.bat ...   Refuses to write an empty result."""
import sys

for path in sys.argv[1:]:
    with open(path, "rb") as f:
        data = f.read()
    if not data:
        print(f"skip {path}: empty")
        continue
    fixed = data.replace(b"\r\n", b"\n").replace(b"\n", b"\r\n")
    if fixed != data:
        with open(path, "wb") as f:
            f.write(fixed)
        print(f"crlf {path}")
