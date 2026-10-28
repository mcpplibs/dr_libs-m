// The raw headers and the import in one translation unit (the side header
// first, the canonical order): the module's names are the very same entities
// as the headers'; the macros a C++ entity cannot be work beside them — the
// *_API attribute on a declaration of one's own, the stringification helpers,
// dr_flac's platform detection.
#include <mwrap/dr_libs/macros.hpp>
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.dr_libs;

// A declaration of one's own in dr_libs' style.
extern "C" {
DRWAV_API int mwrap_dr_libs_test_entry(int value);
}
int mwrap_dr_libs_test_entry(int value) { return value + 1; }

TEST(HeaderMix, SameEntities) {
    // One declaration each, whichever way it was introduced.
    static_assert(std::is_same_v<decltype(&::drwav_init_memory), decltype(&drwav_init_memory)>);
    static_assert(std::is_same_v<::drwav_int16, std::int16_t> && std::is_same_v<::drflac_uint64, unsigned long long>);
    static_assert(std::is_same_v<::drwav, drwav> && sizeof(::drmp3) == sizeof(drmp3));
    EXPECT_EQ(&::drflac_open_memory, &drflac_open_memory);
    EXPECT_EQ(&::drmp3_init_memory, &drmp3_init_memory);
    // The enumerators are the header's own, exported as they are: unqualified
    // names are not ambiguous with both in scope.
    static_assert(drwav_container_riff == ::drwav_container_riff && drflac_container_ogg == ::drflac_container_ogg);
    static_assert(std::is_same_v<decltype(DRMP3_SEEK_CUR), drmp3_seek_origin> && DRMP3_SEEK_CUR == 1);
    EXPECT_EQ(mwrap_dr_libs_test_entry(41), 42);
}

TEST(HeaderMix, MacrosBesideTheMappings) {
    namespace m = mcpplibs::mwrap::dr_libs::macros;
    // An object-like macro expands even in a qualified name: set it aside to
    // name the constant.
    constexpr auto raw = DR_WAVE_FORMAT_IEEE_FLOAT;
#pragma push_macro("DR_WAVE_FORMAT_IEEE_FLOAT")
#undef DR_WAVE_FORMAT_IEEE_FLOAT
    static_assert(m::DR_WAVE_FORMAT_IEEE_FLOAT == raw && std::is_same_v<decltype(m::DR_WAVE_FORMAT_IEEE_FLOAT), const int>);
#pragma pop_macro("DR_WAVE_FORMAT_IEEE_FLOAT")
    // The stringification helpers stay macros.
    EXPECT_STREQ(DRWAV_XSTRINGIFY(DRWAV_VERSION_MINOR), "14");
    EXPECT_STREQ(DRMP3_STRINGIFY(abc), "abc");
    EXPECT_STREQ(DRFLAC_VERSION_STRING, drflac_version_string());
}

TEST(HeaderMix, PlatformDetection) {
    // dr_flac's platform macros are defined or not, for #ifdef: exactly one
    // architecture on the targets CI covers.
    int arch = 0;
#if defined(DRFLAC_X64)
    ++arch;
#endif
#if defined(DRFLAC_X86)
    ++arch;
#endif
#if defined(DRFLAC_ARM)
    ++arch;
#endif
    EXPECT_EQ(arch, 1);
#if defined(DRFLAC_64BIT)
    EXPECT_EQ(sizeof(drflac_cache_t), 8u);
#else
    EXPECT_EQ(sizeof(drflac_cache_t), 4u);
#endif
    // compat.dr_libs is built with upstream's defaults: no switch is defined.
#if defined(DR_WAV_NO_STDIO) || defined(DR_FLAC_NO_STDIO) || defined(DR_MP3_NO_STDIO) || defined(DR_WAV_NO_CONVERSION_API)
    FAIL() << "compat.dr_libs is built with the stdio and conversion APIs";
#endif
}
