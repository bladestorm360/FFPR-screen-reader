"""Stage HEAD's pure router/bakes for a comparable offline world workload.

Does not alter the working source, Git state, installed games or dependency tools.
Run the generated batch from the repository after the main build is idle.
"""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build_check/exit_baseline"
for path in ("src/core/field/baked_map.cpp","src/core/field/baked_map.h",
             "src/core/field/baked_router.cpp","src/core/field/baked_router.h"):
    destination = OUT/path
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(subprocess.check_output(["git","show","HEAD:"+path],cwd=ROOT))
for game in range(1,7):
    path = f"data/ff{game}/maps.bin"
    destination=OUT/path
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_bytes(subprocess.check_output(["git","show","HEAD:"+path],cwd=ROOT))
(OUT/"navigation_perf_check.cpp").write_bytes((ROOT/"tools/navigation_perf_check.cpp").read_bytes())
lines = ["@echo off",'call "C:\\Program Files\\Microsoft Visual Studio\\18\\Community\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul 2>&1',
         f'cd /d "{OUT}"',
         'cl /nologo /std:c++20 /O2 /EHa /utf-8 /DWIN32_LEAN_AND_MEAN /DNOMINMAX /Isrc/core navigation_perf_check.cpp src/core/field/baked_map.cpp src/core/field/baked_router.cpp /Feexit_baseline.exe > compile.txt 2>&1',
         'if errorlevel 1 exit /b 1','exit_baseline.exe > world_baseline.txt','exit /b %errorlevel%']
(OUT/"run.bat").write_bytes(("\r\n".join(lines)+"\r\n").encode("utf-8"))
print("Staged baseline sources/data and build_check/exit_baseline/run.bat")
