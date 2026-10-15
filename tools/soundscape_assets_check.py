"""Verify actual embedded defaults, deterministic generation and private HRTF payload."""
from pathlib import Path
import json
import re
import pefile
import generate_cues
from make_release import markdown_to_text
from openal_dependency import payload

ROOT = Path(__file__).resolve().parents[1]

def main():
    pe = pefile.PE(str(ROOT / 'out/hid.dll'))
    entries = next(e.directory.entries for e in pe.DIRECTORY_ENTRY_RESOURCE.entries if e.id == 10)
    embedded = {str(e.name): e.directory.entries[0].data.struct for e in entries if e.name}
    sounds = sorted((ROOT / 'data/sounds').glob('*.wav'))
    if len(sounds) != 18:
        raise ValueError('expected all 18 mod cues')
    generate_cues.OUT = ROOT / 'build_check/soundscape/generated'
    generate_cues.main()
    readme = (ROOT / 'readme.md').read_text(encoding='utf-8')
    for sound in sounds:
        key = 'SOUND_' + sound.stem.upper()
        item = embedded[key]
        actual = pe.get_data(item.OffsetToData, item.Size)
        if actual != sound.read_bytes() or actual != (generate_cues.OUT / sound.name).read_bytes():
            raise ValueError('embedded/generated cue mismatch: ' + sound.name)
        if sound.name not in readme:
            raise ValueError('undocumented replacement cue: ' + sound.name)
    if (ROOT / 'ReadMe.txt').read_text(encoding='utf-8') != markdown_to_text(readme):
        raise ValueError('README plain-text companion differs')
    text = json.loads((ROOT / 'data/common/mod_text.json').read_text(encoding='utf-8'))
    locales = {'en','ja','fr','it','de','es','ko','zht','zhc','ru','th','pt'}
    keys = ['Game speech','Game speech help','Navigation audio master','Navigation audio master help',
            'HRTF','HRTF help','Soundscape','Soundscape help','Soundscape settings','Soundscape volume',
            'Soundscape volume help','Soundscape distance','Soundscape distance help','Soundscape cue help',
            'Soundscape cue volume help','Sound preview help','Sound previews','Soundscape distance: {0} steps','{0} steps',
            'Beacon mode','Follow route','Object location','Beacon mode help',
            'Object beacon requires a destination on the current floor',
            'On. Audio pings use the selected beacon mode. Enable Beacon with directions to hear directions as well.']
    bank = (ROOT / 'src/core/audio/cue_bank.cpp').read_text(encoding='utf-8')
    for _, label, toggle in re.findall(r'\{"([^"\n]+)",L"([^"\n]+)","([^"\n]+)"', bank):
        if toggle.startswith('Soundscape'):
            keys.extend([label, label+' volume', 'Preview '+label])
    for key in keys:
        if set(text[key]) != locales or not all(text[key].values()):
            raise ValueError('incomplete localization: ' + key)
    runtime = pefile.PE(str(ROOT / 'third_party/openal-soft/soft_oal.dll'))
    if runtime.FILE_HEADER.Machine != 0x8664:
        raise ValueError('HRTF runtime must be x64')
    imports = {entry.dll.decode().lower() for entry in runtime.DIRECTORY_ENTRY_IMPORT}
    if not imports <= {'avrt.dll','kernel32.dll','msvcrt.dll','ole32.dll','shell32.dll','user32.dll','winmm.dll'}:
        raise ValueError('unexpected HRTF dependency imports: '+repr(imports))
    files = payload(ROOT)
    print(f'Soundscape assets PASS: {len(sounds)} real embedded/deterministic WAVs, all filenames documented, '
          f'{len(keys)} labels in 12 locales, README/TXT agree, {len(files)} pinned HRTF files; x64/system-only imports')
    return 0

if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, StopIteration) as error:
        print('FAIL: '+str(error))
        raise SystemExit(1)
