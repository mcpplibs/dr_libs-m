# Using mcpplibs.mwrap.dr_libs

`import mcpplibs.mwrap.dr_libs;` declares the API of dr_wav 0.14.5, dr_flac 0.13.3 and dr_mp3 0.7.3 as `<dr_wav.h>`, `<dr_flac.h>` and `<dr_mp3.h>` do. Types, enumerators and functions are the upstream globals, the very same entities as the headers', so a translation unit may include the headers too. The library is `compat.dr_libs` 2026.03.03: the three headers of upstream commit `fa931f3` (the `wav-0.14.5` tag, whose `dr_flac.h` and `dr_mp3.h` are byte for byte the `flac-0.13.3` and `mp3-0.7.3` tags), compiled once by one generated implementation TU with upstream's default configuration.

## Modules

| Module | What it holds |
|---|---|
| `mcpplibs.mwrap.dr_libs` | re-exports the four modules below |
| `mcpplibs.mwrap.dr_libs.wav` | `<dr_wav.h>`: 42 types (`drwav`, `drwav_data_format`, `drwav_fmt`, `drwav_metadata` and its `drwav_smpl`, `drwav_cue`, `drwav_acid`, `drwav_bext`, `drwav_inst`, `drwav_list_*` …, the callback types), the 48 enumerators of its 6 enums (`drwav_container_riff` …, `DRWAV_SEEK_SET` …, `drwav_metadata_type_*`, `drwav_acid_flag_*`, `drwav_smpl_loop_type_*`, `drwav_metadata_location_*`) and all 94 `DRWAV_API` functions; re-exports `.macros` |
| `mcpplibs.mwrap.dr_libs.flac` | `<dr_flac.h>`: 32 types (`drflac`, `drflac_metadata`, `drflac_streaminfo`, `drflac_seekpoint`, the Vorbis comment and cuesheet iterators …), 6 enumerators (`drflac_container_*`, `DRFLAC_SEEK_*`) and all 31 `DRFLAC_API` functions; re-exports `.macros` |
| `mcpplibs.mwrap.dr_libs.mp3` | `<dr_mp3.h>`: 28 types (`drmp3`, `drmp3_config`, `drmp3_seek_point`, `drmp3_metadata`, the low-level `drmp3dec` and `drmp3dec_frame_info` …), 8 enumerators (`DRMP3_SEEK_*`, `DRMP3_METADATA_TYPE_*`) and all 29 `DRMP3_API` functions; re-exports `.macros` |
| `mcpplibs.mwrap.dr_libs.macros` | the 171 object-like macros of the three headers, in `mcpplibs::mwrap::dr_libs::macros` |

Each header's submodule is complete on its own: `import mcpplibs.mwrap.dr_libs.flac;` is all a FLAC decoder needs (`tests/flac_surface_test.cpp` names every entity of `<dr_flac.h>` with nothing else imported). The public part of a header is everything before its implementation section; the types in it that upstream uses only inside other structs (`drwav__memory_stream`, `drflac_bs`, `drmp3_L3_gr_info` …) are exported too, as declarations of the header.

Every enum of the three headers is named by its typedef (`typedef enum { … } drwav_container;`), so it is not TU-local and its enumerators are exported as they are: `drwav_container_riff` is the header's own enumerator, usable in `case` labels and constant expressions.

### The API in one paragraph each

- **dr_wav**: `drwav_init_file` / `drwav_init_memory` / `drwav_init` (callbacks) open a stream (RIFF, RIFX, RF64, Sony Wave64, AIFF/AIFC; integer PCM (8-bit unsigned, 16, 24, 32 bits), IEEE float (32 and 64 bits), A-law, mu-law, MS and IMA ADPCM); `drwav_read_pcm_frames_s16` / `_s32` / `_f32` decode to one sample format; `drwav_seek_to_pcm_frame`; `drwav_init_*_with_metadata` gives `pMetadata` (smpl, inst, cue, acid, bext, LIST info and labels) and `drwav_take_ownership_of_metadata` keeps it. `drwav_init_file_write` / `drwav_init_memory_write` / `drwav_init_write_with_metadata` write RIFF, RF64 or Wave64 (PCM, IEEE float, A-law, mu-law; no ADPCM, no AIFF). `drwav_open_*_and_read_pcm_frames_*` decode a whole file in one call; free with `drwav_free`. `drwav_s16_to_f32`, `drwav_f32_to_s16`, `drwav_mulaw_to_s16` … convert buffers.
- **dr_flac**: `drflac_open_file` / `drflac_open_memory` / `drflac_open` (native FLAC), `drflac_open_relaxed` (no STREAMINFO needed), `drflac_open_*_with_metadata` (every metadata block through a callback: STREAMINFO with its MD5, SEEKTABLE, VORBIS_COMMENT, CUESHEET, PICTURE; `drflac_init_vorbis_comment_iterator`, `drflac_init_cuesheet_track_iterator`). Ogg-encapsulated FLAC is recognised too (`drflac_container_ogg`). `drflac_read_pcm_frames_s16` / `_s32` / `_f32`, `drflac_seek_to_pcm_frame`, `drflac_close`.
- **dr_mp3**: `drmp3_init_file` / `drmp3_init_memory` / `drmp3_init` (MPEG-1, 2 and 2.5 Layer III), `drmp3_read_pcm_frames_f32` / `_s16`, `drmp3_seek_to_pcm_frame`, `drmp3_get_pcm_frame_count` (gapless: a LAME/Xing tag's encoder delay and padding are trimmed), `drmp3_get_mp3_frame_count`, seek tables (`drmp3_calculate_seek_points`, `drmp3_bind_seek_table`), metadata (ID3v1/v2, APE, Xing/LAME through `drmp3_init_*_with_metadata`), and the low-level push decoder `drmp3dec_init` / `drmp3dec_decode_frame`.

### Upstream behavior worth knowing

- **Wide paths on Linux and macOS need allocation callbacks.** Outside Windows (where `_wfopen_s` / `_wfopen` open them directly) the `_w` functions (`drwav_init_file_w`, `drflac_open_file_w`, `drmp3_init_file_w`, …) convert the path with `wcsrtombs` into a buffer they allocate through the `*_allocation_callbacks` they are given; with `NULL` that allocation fails and the file is not opened. Pass callbacks (malloc/realloc/free) to them, and select a UTF-8 `LC_CTYPE` locale (`setlocale`) for non-ASCII paths, since `wcsrtombs` follows it. The narrow functions take `NULL` and the path as bytes. [tests/api_test.cpp](../tests/api_test.cpp) checks both, and that `NULL` fails outside Windows.
- **dr_mp3 0.7.3 seek tables are approximate on short or gapless (LAME-tagged) streams.** A seek through a bound table can land up to a frame away; `drmp3_seek_to_pcm_frame` without a table is exact (it decodes from the start), which the tests check. Upstream reworked seek tables after the tag (commit bad753d, unreleased). `drmp3_get_mp3_and_pcm_frame_count` and `drmp3_calculate_seek_points` also fail when the stream sits within the tag's encoder delay of its end (they restore the position untrimmed); call them at the start of the stream, as upstream's documentation does. `drmp3_get_mp3_and_pcm_frame_count` counts PCM frames untrimmed (5760 for the 4410-sample fixture); `drmp3_get_pcm_frame_count` trims.
- **`drwav_f32_to_s16` is not the inverse of `drwav_s16_to_f32`.** It maps [-1, 1] onto [-32768, 32767] (`(x + 1) * 32767.5 - 32768`), so 0.0 becomes -1. To get 16-bit samples sample-exact, read them with `drwav_read_pcm_frames_s16` (or `drflac_` / `drmp3_` `read_pcm_frames_s16`), as [examples/dr_libs](../examples/dr_libs) does.
- Free what `*_open_*_and_read_*` returns with the same library's `*_free`, with the same allocation callbacks.

### miniaudio in the same program

miniaudio 0.11.25 embeds the same three versions under other names (`ma_dr_wav_*`, `ma_dr_flac_*`, `ma_dr_mp3_*`, declared only inside its implementation), so `compat.miniaudio` and `compat.dr_libs` link into one program without a symbol in common. [tests/miniaudio_test.cpp](../tests/miniaudio_test.cpp) links both and decodes the fixtures through `ma_decoder` and through dr_flac / dr_mp3 directly: the samples are identical.

## Macros

A named module cannot export a macro. `mcpplibs::mwrap::dr_libs::macros` holds the headers' object-like macros under their original names; `using namespace mcpplibs::mwrap::dr_libs::macros;` makes code read as it does with the headers.

- **Constants** as `inline constexpr`, each of the type its macro expands to: the result codes (`DRWAV_SUCCESS`, `DRWAV_INVALID_FILE` … `DRWAV_AT_END`, and the same 54 for `DRMP3_`; dr_flac reports with `drflac_bool32`), `DRWAV_TRUE` / `DRWAV_FALSE` and their `DRFLAC_` / `DRMP3_` twins, the WAVE format tags (`DR_WAVE_FORMAT_PCM`, `_ADPCM`, `_IEEE_FLOAT`, `_ALAW`, `_MULAW`, `_DVI_ADPCM`, `_EXTENSIBLE`), `DRWAV_SEQUENTIAL` and `DRWAV_WITH_METADATA`, `DRFLAC_METADATA_BLOCK_TYPE_*` and `DRFLAC_PICTURE_TYPE_*`, `DR_FLAC_BUFFER_SIZE` (4096, the build's), the `DRMP3_MAX_*` limits and the `*_VERSION_MAJOR` / `_MINOR` / `_REVISION` are `int`; `DRMP3_UINT64_MAX` is `drmp3_uint64`. `DRWAV_VERSION_STRING`, `DRFLAC_VERSION_STRING` and `DRMP3_VERSION_STRING` are references to constexpr char arrays.
- The headers define no computation macro: their function-like macros are the stringification helpers.

Each mapping is checked against the raw header (type and value, with the macro set aside by `push_macro`) in [tests/macros_mix_test.cpp](../tests/macros_mix_test.cpp), and the version strings against the linked library's `*_version_string()`.

### What stays a macro

These stay macros in `<mwrap/dr_libs/macros.hpp>`, which is `<dr_wav.h>`, `<dr_flac.h>` and `<dr_mp3.h>`:

| Macros | Why |
|---|---|
| `DRWAV_API`, `DRFLAC_API`, `DRMP3_API`, `DRWAV_PRIVATE`, `DRFLAC_PRIVATE`, `DRMP3_PRIVATE`, `DRFLAC_DEPRECATED` | attributes of declarations (`extern` and `static` here: compat.dr_libs is a static library built without `*_DLL`) |
| `DRWAV_DLL_IMPORT` / `_EXPORT` / `_PRIVATE` and their `DRFLAC_` / `DRMP3_` twins | defined only under `*_DLL` |
| `DRMP3_INLINE`, `DRMP3_GNUC_INLINE_HINT` | function specifiers |
| `DRWAV_STRINGIFY`, `DRWAV_XSTRINGIFY` and their `DRFLAC_` / `DRMP3_` twins | stringification |
| `DRFLAC_64BIT`, `DRFLAC_X64`, `DRFLAC_X86`, `DRFLAC_ARM` | platform detection, defined only on some targets: for `#ifdef` |

The configuration switches (`DR_WAV_NO_STDIO`, `DR_WAV_NO_CONVERSION_API`, `DR_FLAC_NO_STDIO`, `DR_FLAC_NO_OGG`, `DR_FLAC_NO_CRC`, `DR_FLAC_NO_SIMD`, `DR_MP3_NO_STDIO`, `DR_MP3_NO_SIMD`, `DR_MP3_FLOAT_OUTPUT` …) are inputs of the implementation, which compat.dr_libs compiles with none of them defined; defining one in a consumer changes nothing but that consumer's view of the header. Never define `DR_WAV_IMPLEMENTATION`, `DR_FLAC_IMPLEMENTATION` or `DR_MP3_IMPLEMENTATION`: the implementations are already linked.

Include the side header before the import. While a header macro is defined, it expands even inside a qualified name, so it hides `macros::DRWAV_SUCCESS`: use the plain macro, or set it aside with `#pragma push_macro` ([tests/header_mix_test.cpp](../tests/header_mix_test.cpp)). [tools/check_exports.py](../tools/check_exports.py) accounts for all 202 macros (171 mapped, 28 kept, 3 include guards).

## Licensing

The wrapper's code is Apache-2.0. dr_libs is available as a choice of Public Domain (Unlicense) or MIT No Attribution ([upstream/dr_libs/LICENSE](../upstream/dr_libs/LICENSE)); dr_mp3 is based on minimp3 (CC0). Neither alternative requires attribution, so a statically linked program carries no notice obligation for dr_libs; `runtime.deploy` still copies the wrapper's LICENSE and NOTICE and dr_libs' README and LICENSE into `licenses/dr_libs/` beside every program that depends on this package.

## Checks

- `mcpp test` runs eight tests:
  - [tests/api_test.cpp](../tests/api_test.cpp): the API through the module only — WAV round trips in RIFF, Wave64 and RF64 (s16 and f32 reads, seeks, cursor and length), IEEE float bit-exact, every A-law and mu-law code against the tables, an AIFF stream written by hand, cue points written with `drwav_init_write_with_metadata` and read back, the conversions, files with narrow and non-ASCII wide paths; the FLAC fixture decoded sample-exact against the signal its STREAMINFO MD5 belongs to, with seeks, its Vorbis comments, the s32/f32 one-call forms and files; the MP3 fixture to its gapless length (4410), its 440 Hz tone, an exact seek, the frame counts, a seek table, the s16 one-call form and the low-level frame decoder; custom allocation callbacks;
  - [tests/macros_mix_test.cpp](../tests/macros_mix_test.cpp) (generated): every constant against the header;
  - [tests/header_mix_test.cpp](../tests/header_mix_test.cpp): the headers beside the import — the same entities, `DRWAV_API` on a declaration of one's own, the stringification helpers, the platform detection;
  - [tests/macros_test.cpp](../tests/macros_test.cpp): the macros module alone, at compile time;
  - `tests/wav_surface_test.cpp`, `flac_surface_test.cpp`, `mp3_surface_test.cpp` (generated): each submodule alone names every entity of its header;
  - [tests/miniaudio_test.cpp](../tests/miniaudio_test.cpp): compat.miniaudio in the same program, the same samples.
- `python3 tools/check_exports.py` checks that the sources are what [tools/gen_exports.py](../tools/gen_exports.py) writes from the pinned headers, and that every declaration is exported by its submodule and every macro mapped, kept or an include guard.
- `python3 tools/run_example.py` builds [examples/dr_libs](../examples/dr_libs) and runs it on the fixtures ([tests/data](../tests/data/README.md)): the FLAC decoded to a 16-bit WAV is checked sample by sample against its signal.
- `python3 tools/test_template.py` scaffolds the default template from an installed copy of the package, then builds and runs it.
