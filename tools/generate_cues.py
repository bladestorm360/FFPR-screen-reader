"""Deterministic original procedural defaults. No game/commercial audio assets.
PCM16 mono 22050 Hz. Build first, then python tools/generate_cues.py.
Legacy effects use the original C++ generator, including its seeded .NET noise,
float expression order, truncation, cycle alignment and envelopes.
"""
from pathlib import Path
import math
import random
import struct
import wave
import subprocess

RATE = 22050
OUT = Path(__file__).resolve().parents[1] / 'data' / 'sounds'

def tone(freq, seconds, gain, *, chirp=0, decay=1, overtone=0):
    n = round(seconds * RATE)
    return [gain * min(1, i / (RATE*.008)) * (1-i/n)**decay *
            (math.sin(2*math.pi*(freq*i/RATE+chirp*(i/RATE)**2/2)) +
             overtone*math.sin(2*math.pi*freq*2.01*i/RATE)) for i in range(n)]

def chain(*parts):
    return [s for part in parts for s in part]

def gap(s):
    return [0.] * round(s * RATE)

def write(name, samples):
    with wave.open(str(OUT / (name+'.wav')), 'wb') as f:
        f.setparams((1, 2, RATE, 0, 'NONE', 'not compressed'))
        f.writeframes(b''.join(struct.pack('<h', round(max(-1,min(1,v))*32767)) for v in samples))

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    generator=Path(__file__).resolve().parents[1] / 'out/checks/soundscape_check.exe'
    subprocess.run([str(generator), '--write-legacy', str(OUT)], check=True)
    write('npc',tone(220,.14,.18,chirp=700,decay=1.6))
    write('chest',tone(392,.24,.13,decay=2.5,overtone=.25))
    write('exit',chain(*(chain(tone(f,.09,.17),gap(.025)) for f in (196,247,294))))
    rng=random.Random(15)
    knock=[(.15*math.sin(2*math.pi*160*i/RATE)+.07*rng.uniform(-1,1))*math.exp(-i/(RATE*.022)) for i in range(round(RATE*.07))]
    write('door',chain(knock,gap(.075),knock,gap(.12),knock))
    write('event',chain(tone(330,.09,.16),gap(.07),tone(330,.09,.16)))
    write('trap',chain(tone(147,.11,.18,overtone=.18),gap(.065),tone(139,.11,.18,overtone=.18)))
    write('vehicle',chain(tone(175,.11,.17,chirp=350),gap(.055),tone(185,.11,.17,chirp=350)))
    write('save_point',chain(tone(440,.13,.13,overtone=.1),gap(.04),tone(587,.2,.12,overtone=.1)))

if __name__ == '__main__':
    main()
