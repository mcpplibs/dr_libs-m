# Test fixtures

Made for this repository from the signals below (no third-party content),
contributed under the wrapper's licence (Apache-2.0). `tools/run_example.py`
regenerates the FLAC signal and checks a decode against it.

| File | Bytes | sha256 | What it is |
|---|---|---|---|
| `tone.flac` | 3097 | `d34377568d5e8f3dcfe1adc886d4ffbb56aea6613ada5432f1101b4d0ad7a81b` | 2048 stereo frames, 44100 Hz, 16-bit; four 512-frame FLAC frames (fixed and LPC subframes, Rice residuals, stereo decorrelation); STREAMINFO MD5 `29022c3513e9a6fea76a960fd21470c2`; a VORBIS_COMMENT block with vendor `ffmpeg` and no comments |
| `tone.mp3` | 1227 | `180530c0241d2d5339348312c8ca5da331cf6374509a62d942acb6807abddb5b` | 4410 samples of a 440 Hz sine (amplitude 12000), 22050 Hz mono, MPEG-2 Layer III 32 kb/s CBR: a LAME Info frame (encoder delay and padding) and 10 audio frames; no ID3 |

## The signals

FLAC, per frame `n` and channel `c` (interleaved, 16-bit little-endian), with
`x` a 32-bit LCG starting at 12345 and stepped once per sample:

```
x     = x * 1664525 + 1013904223            (mod 2^32)
noise = ((x >> 16) & 15) - 8
p     = 400 + 100 * c;  t = n % p
tri   = t < p/2 ? (t*4*8000)/p - 8000 : 24000 - (t*4*8000)/p     (integer division)
s     = tri + noise
```

MP3: `s[n] = round(12000 * sin(2*pi*440*n/22050))`, `n` in `[0, 4410)`.

## How they were made

```sh
ffmpeg -f s16le -ar 44100 -ac 2 -i flac_src.s16le -c:a flac -compression_level 8 \
       -frame_size 512 -bitexact -map_metadata -1 tone.flac   # then the PADDING block dropped
ffmpeg -f s16le -ar 22050 -ac 1 -i mp3_src.s16le -c:a libmp3lame -b:a 32k -bitexact \
       -map_metadata -1 -id3v2_version 0 -write_id3v1 0 tone.mp3
```

`ffmpeg -i tone.flac -f s16le -` is byte-identical to the source signal.
