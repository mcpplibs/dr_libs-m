// <mwrap/dr_libs/macros.hpp> — the macros of dr_wav, dr_flac and dr_mp3 as
// macros, for the ones a C++ entity cannot be: the declaration attributes
// (DRWAV_API, DRFLAC_API, DRMP3_API, *_PRIVATE, DRFLAC_DEPRECATED,
// DRMP3_INLINE …), the stringification helpers (*_STRINGIFY, *_XSTRINGIFY),
// the platform detection of dr_flac (DRFLAC_64BIT, DRFLAC_X64, DRFLAC_X86,
// DRFLAC_ARM: defined or not, for #ifdef), and the configuration switches a
// program tests with #ifdef (DR_WAV_NO_STDIO … are not defined:
// compat.dr_libs is built with upstream's defaults).
//
// Include it before `import mcpplibs.mwrap.dr_libs;`: it is <dr_wav.h>,
// <dr_flac.h> and <dr_mp3.h>, as the modules see them. Every other macro is
// defined too, and while a macro is defined it expands even inside
// `mcpplibs::mwrap::dr_libs::macros::NAME`: use the plain macro, or set it
// aside with #pragma push_macro. Never define DR_WAV_IMPLEMENTATION,
// DR_FLAC_IMPLEMENTATION or DR_MP3_IMPLEMENTATION before it: the
// implementations are compat.dr_libs', already linked.
#pragma once
#include <dr_wav.h>
#include <dr_flac.h>
#include <dr_mp3.h>
