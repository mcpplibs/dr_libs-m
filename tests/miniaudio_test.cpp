// dr_libs and miniaudio in one program. miniaudio 0.11.25 embeds the same
// dr_wav 0.14.5, dr_flac 0.13.3 and dr_mp3 0.7.3, renamed (ma_dr_wav_*,
// ma_dr_flac_*, ma_dr_mp3_*), so compat.dr_libs and compat.miniaudio link
// together without a symbol in common — and both decode the fixtures to the
// very same samples.
#include <miniaudio.h>
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.dr_libs;

namespace {
std::vector<unsigned char> slurp(const char* name) {
    auto path = std::filesystem::path(std::source_location::current().file_name()).parent_path() / "data" / name;
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

template <class T>
std::vector<T> through_miniaudio(const std::vector<unsigned char>& bytes, ma_format format, ma_uint32 channels, ma_uint32 rate) {
    ma_decoder_config config = ma_decoder_config_init(format, channels, rate);
    ma_decoder decoder;
    if (ma_decoder_init_memory(bytes.data(), bytes.size(), &config, &decoder) != MA_SUCCESS) return {};
    ma_uint64 length = 0;
    if (ma_decoder_get_length_in_pcm_frames(&decoder, &length) != MA_SUCCESS) length = 0;
    std::vector<T> out(std::size_t(length) * channels);
    ma_uint64 read = 0;
    ma_decoder_read_pcm_frames(&decoder, out.data(), length, &read);
    out.resize(std::size_t(read) * channels);
    ma_decoder_uninit(&decoder);
    return out;
}
}  // namespace

TEST(Miniaudio, Versions) {
    // miniaudio declares its ma_dr_* copies inside its implementation only;
    // the versions it embeds are recorded in docs/usage.md.
    EXPECT_STREQ(ma_version_string(), "0.11.25");
    EXPECT_STREQ(drflac_version_string(), "0.13.3");
}

TEST(Miniaudio, FlacSameSamples) {
    const auto bytes = slurp("tone.flac");
    unsigned channels = 0, rate = 0;
    drflac_uint64 frames = 0;
    drflac_int16* dr = drflac_open_memory_and_read_pcm_frames_s16(bytes.data(), bytes.size(), &channels, &rate, &frames, nullptr);
    ASSERT_NE(dr, nullptr);
    const auto ma = through_miniaudio<ma_int16>(bytes, ma_format_s16, 2, 44100);
    ASSERT_EQ(ma.size(), std::size_t(frames) * 2);
    EXPECT_TRUE(std::equal(ma.begin(), ma.end(), dr));
    drflac_free(dr, nullptr);
}

TEST(Miniaudio, Mp3SameSamples) {
    const auto bytes = slurp("tone.mp3");
    drmp3_config config{};
    drmp3_uint64 frames = 0;
    float* dr = drmp3_open_memory_and_read_pcm_frames_f32(bytes.data(), bytes.size(), &config, &frames, nullptr);
    ASSERT_NE(dr, nullptr);
    EXPECT_EQ(frames, 4410u);
    const auto ma = through_miniaudio<float>(bytes, ma_format_f32, 1, 22050);
    ASSERT_EQ(ma.size(), std::size_t(frames));
    EXPECT_TRUE(std::equal(ma.begin(), ma.end(), dr));
    drmp3_free(dr, nullptr);
}
