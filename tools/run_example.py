#!/usr/bin/env python3
"""Build examples/dr_libs (written against import mcpplibs.mwrap.dr_libs) and
run it on the fixtures: describe them, decode the FLAC to a 16-bit WAV and
check every sample against the signal tests/data/README.md defines, decode
the MP3 to a float WAV of its gapless length, and describe the outputs.
"""
import argparse
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile


def flac_signal(frames: int, channels: int) -> list[int]:
    x, out = 12345, []
    for n in range(frames):
        for c in range(channels):
            x = (x * 1664525 + 1013904223) & 0xFFFFFFFF
            noise = ((x >> 16) & 15) - 8
            p = 400 + 100 * c
            t = n % p
            tri = (t * 4 * 8000) // p - 8000 if t < p // 2 else 24000 - (t * 4 * 8000) // p
            out.append(tri + noise)
    return out


def wav_data(path: Path) -> tuple[int, int, int, int, bytes]:
    """(format tag, channels, rate, bits, data) of a RIFF WAV."""
    b = path.read_bytes()
    assert b[:4] == b'RIFF' and b[8:12] == b'WAVE', path
    i, fmt, data = 12, None, None
    while i + 8 <= len(b):
        cid, size = b[i:i + 4], struct.unpack('<I', b[i + 4:i + 8])[0]
        body = b[i + 8:i + 8 + size]
        if cid == b'fmt ':
            tag, ch, rate, _, _, bits = struct.unpack('<HHIIHH', body[:16])
            fmt = (tag, ch, rate, bits)
        elif cid == b'data':
            data = body
        i += 8 + size + (size & 1)
    assert fmt and data is not None, path
    return (*fmt, data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mcpp', default='mcpp')
    parser.add_argument('--toolchain')
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    example, data = root / 'examples/dr_libs', root / 'tests/data'
    command = [args.mcpp, 'build'] + (['--toolchain', args.toolchain] if args.toolchain else [])
    print('+', ' '.join(command), flush=True)
    subprocess.run(command, cwd=example, check=True)
    suffix = '.exe' if os.name == 'nt' else ''
    found = sorted((p for p in (example / 'target').rglob('dr_libs' + suffix) if p.parent.name == 'bin' and p.is_file()),
                   key=lambda p: p.stat().st_mtime)
    if not found:
        sys.exit('examples/dr_libs was not built')
    tool = str(found[-1])

    def run(*argv: str) -> str:
        print('+ dr_libs', ' '.join(argv), flush=True)
        r = subprocess.run([tool, *argv], capture_output=True, text=True)
        print(r.stdout + r.stderr, end='', flush=True)
        if r.returncode != 0:
            sys.exit(f'dr_libs {" ".join(argv)}: exit {r.returncode}')
        return r.stdout

    out = run('info', str(data / 'tone.flac'), str(data / 'tone.mp3'))
    assert 'FLAC, 16-bit, 2 ch, 44100 Hz, 2048 frames' in out, out
    assert 'MP3, 10 MPEG frames, 1 ch, 22050 Hz, 4410 frames' in out, out
    with tempfile.TemporaryDirectory() as directory:
        pcm, flt = Path(directory) / 'flac.wav', Path(directory) / 'mp3.wav'
        run('decode', str(data / 'tone.flac'), str(pcm))
        tag, ch, rate, bits, body = wav_data(pcm)
        assert (tag, ch, rate, bits) == (1, 2, 44100, 16), (tag, ch, rate, bits)
        want = flac_signal(2048, 2)
        got = list(struct.unpack(f'<{len(body) // 2}h', body))
        if got != want:
            sys.exit(f'tone.flac decoded to {len(got)} samples, not the {len(want)} of its signal')
        print(f'ok   tone.flac -> 16-bit WAV: {len(want)} samples, every one the signal\'s', flush=True)
        run('decode', str(data / 'tone.mp3'), str(flt), '--float')
        tag, ch, rate, bits, body = wav_data(flt)
        assert (tag, ch, rate, bits, len(body)) == (3, 1, 22050, 32, 4410 * 4), (tag, ch, rate, bits, len(body))
        print('ok   tone.mp3 -> float WAV: 4410 frames, the LAME tag\'s gapless length', flush=True)
        out = run('info', str(pcm), str(flt))
        assert 'WAV, PCM 16-bit, 2 ch, 44100 Hz, 2048 frames' in out and 'WAV, IEEE float 32-bit, 1 ch, 22050 Hz, 4410 frames' in out
        r = subprocess.run([tool, 'info', str(Path(directory) / 'missing.flac')], capture_output=True, text=True)
        assert r.returncode == 1, r
    print('examples/dr_libs: ok', flush=True)


if __name__ == '__main__':
    main()
