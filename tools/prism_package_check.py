"""Read-only ABI/package inspection; never loads the Prism DLL."""
from pathlib import Path
import hashlib
import json
import re
import struct
import pefile

ROOT = Path(__file__).resolve().parents[1]
package = ROOT / 'third_party/prism'
lock = json.loads((ROOT / 'prism.lock.json').read_text(encoding='utf-8'))
for entry in lock['files'].values():
    assert hashlib.sha256((package / entry['source']).read_bytes()).hexdigest() == entry['sha256']
for name, expected in lock['headers'].items():
    assert hashlib.sha256((package / 'include' / name).read_bytes()).hexdigest() == expected
assert '#define PRISM_VERSION_STRING "0.18.3"' in (package / 'include/prism_version.h').read_text()
pe = pefile.PE(str(package / 'bin/prism.dll'))
assert pe.FILE_HEADER.Machine == 0x8664
exports = {symbol.name.decode() for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols if symbol.name}
header = (ROOT / 'src/core/prism_backend.h').read_text(encoding='utf-8')
names = set(re.findall(r'X\((\w+)\)', header.split('struct Api')[0]))
assert len(names) == 14 and {'prism_' + name for name in names} <= exports
version = next(symbol for symbol in pe.DIRECTORY_ENTRY_EXPORT.symbols if symbol.name == b'prism_version_string')
# This locked x64 release returns a literal through LEA RAX,[RIP+disp32].
# Read the literal offline rather than initializing a real screen-reader backend.
assert pe.get_data(version.address, 3) == b'\x48\x8d\x05'
literal = version.address + 7 + struct.unpack('<i', pe.get_data(version.address + 3, 4))[0]
assert pe.get_data(literal, 16).split(b'\0', 1)[0] == b'0.18.3'
system = {'ole32.dll', 'kernel32.dll', 'rpcrt4.dll', 'user32.dll', 'oleaut32.dll',
          'shlwapi.dll', 'xmllite.dll', 'uiautomationcore.dll', 'advapi32.dll'}
assert all(row.dll.decode().lower() in system or row.dll.decode().lower().startswith('api-ms-win-')
           for row in pe.DIRECTORY_ENTRY_IMPORT)
assert not getattr(pe, 'DIRECTORY_ENTRY_DELAY_IMPORT', [])
print('Prism package: pinned x64 DLL/SDK/licenses, 14 API exports and system-only mandatory imports verified offline.')
