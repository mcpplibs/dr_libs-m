# dr_libs

> dr_wav, dr_flac and dr_mp3 — WAV/AIFF, FLAC and MP3 decoding and WAV writing — through C++23 modules.

**English** · [简体中文](README.zh.md) · [Upstream](https://github.com/mackron/dr_libs) · [Releases](https://github.com/mcpplibs/dr_libs-m/releases) · [mcpp](https://github.com/mcpp-community/mcpp) · [Index](https://github.com/mcpplibs/mcpp-index)

[![C++23](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://en.cppreference.com/w/cpp/23)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![dr_libs](https://img.shields.io/badge/dr__libs-MIT--0%20OR%20Unlicense-blue.svg)](upstream/dr_libs/LICENSE)
[![mcpp-community](https://img.shields.io/badge/mcpp--community-mwrap-blue)](https://github.com/mcpp-community)
[![CI](https://github.com/mcpplibs/dr_libs-m/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/mcpplibs/dr_libs-m/actions/workflows/ci.yml)

dr_libs are David Reid's single-file audio libraries in C, without
dependencies: dr_wav reads WAV, RF64, Wave64 and AIFF (PCM, float, A-law,
mu-law, ADPCM) and writes WAV; dr_flac decodes FLAC, native and in Ogg;
dr_mp3 decodes MP3, gapless when a LAME tag says so. They are public domain or
MIT-0, at your choice. The upstream C API is unchanged: `drwav_init_file`,
`drflac_open_memory` and `drmp3_read_pcm_frames_f32` are the globals of
`<dr_wav.h>`, `<dr_flac.h>` and `<dr_mp3.h>`. The library is
`compat.dr_libs` 2026.03.03 (dr_wav 0.14.5, dr_flac 0.13.3, dr_mp3 0.7.3, one
upstream commit). Each header is its own module, and every macro is a
constant checked against the header. miniaudio's renamed copies coexist with
it in one program.

## Modules

- `import mcpplibs.mwrap.dr_libs;`: all three, re-exporting the modules below
- `import mcpplibs.mwrap.dr_libs.wav;`: `<dr_wav.h>` (`drwav`, `drwav_init_*`, `drwav_read_pcm_frames_*`, `drwav_init_write*`, metadata, conversions)
- `import mcpplibs.mwrap.dr_libs.flac;`: `<dr_flac.h>` (`drflac`, `drflac_open_*`, `drflac_read_pcm_frames_*`, metadata blocks and their iterators)
- `import mcpplibs.mwrap.dr_libs.mp3;`: `<dr_mp3.h>` (`drmp3`, `drmp3_init_*`, `drmp3_read_pcm_frames_*`, seek tables, the frame decoder `drmp3dec`)
- `import mcpplibs.mwrap.dr_libs.macros;`: `DRWAV_SUCCESS`, `DR_WAVE_FORMAT_PCM`, `DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO`, `DRMP3_MAX_SAMPLES_PER_FRAME` … in `mcpplibs::mwrap::dr_libs::macros` (each header module re-exports it)

> More Modules API -> [Usage and API](docs/usage.md)

## Getting started

```sh
mcpp new tone --template mcpplibs.mwrap.dr_libs@2026.03.03
cd tone
mcpp run
```

The default template writes `tone.wav` with dr_wav, reads it back sample-exact
and describes it; `mcpp run -- song.flac` describes your own WAV, AIFF, FLAC or
MP3 files.

```toml
[dependencies]
"mcpplibs.mwrap.dr_libs" = "2026.03.03"
```

```cpp
import std;
import mcpplibs.mwrap.dr_libs;

int main(int argc, char** argv) {
    if (argc < 2) return 2;
    drflac* flac = drflac_open_file(argv[1], nullptr);
    if (flac == nullptr) return 1;
    std::vector<float> pcm(flac->totalPCMFrameCount * flac->channels);
    drflac_uint64 frames = drflac_read_pcm_frames_f32(flac, flac->totalPCMFrameCount, pcm.data());
    std::println("{} frames, {} ch, {} Hz", frames, flac->channels, flac->sampleRate);
    drflac_close(flac);
}
```

## Macros

Constants such as `DRWAV_SUCCESS` and `DR_WAVE_FORMAT_IEEE_FLOAT` keep the
type their macro has (`int`); the version strings are constexpr char arrays.
Every enum of the headers is named by its typedef, so its enumerators
(`drwav_container_riff`, `DRWAV_SEEK_SET` …) are exported as they are. The
`*_API` attributes, the stringification helpers and dr_flac's platform
detection stay macros in `<mwrap/dr_libs/macros.hpp>`, included before the
import. Details, and three upstream behaviors worth knowing (wide paths
outside Windows, dr_mp3 seek tables, `drwav_f32_to_s16`):
[docs/usage.md](docs/usage.md).

## Examples

- [examples/dr_libs](examples/dr_libs): an audio file tool written against the module — `info` describes WAV, AIFF,
  FLAC and MP3 files (format, length, peak and RMS level); `decode` turns any of them into a 16-bit or float WAV,
  a 16-bit source sample-exact. [tools/run_example.py](tools/run_example.py) runs it on the fixtures in CI.

## Build and testing

Linux (x86_64 GCC and LLVM, arm64 GCC), macOS and Windows CI build the
modules, then run the tests and the export audit
([tools/check_exports.py](tools/check_exports.py)). The audit checks that the
generated sources are fresh, that every declaration of each header is exported
by its module and that every macro is mapped or listed with its reason. The
tests decode a FLAC fixture sample-exact against the signal its MD5 belongs to
and an MP3 fixture to its gapless length, round-trip WAV in every container
dr_wav writes, and link miniaudio beside dr_libs to decode the same fixtures
to the same samples. A separate job scaffolds the default template as an
installed package and runs it.

## License

[Apache-2.0](LICENSE) for the wrapper, by mcpp-community ([NOTICE](NOTICE)).
dr_libs is public domain (Unlicense) or MIT No Attribution, at your choice
([upstream/dr_libs/LICENSE](upstream/dr_libs/LICENSE)). Its README and LICENSE
are deployed beside every program that depends on this package.
