// dr_wav, dr_flac and dr_mp3 through the modules, spelled as their headers
// document them: WAV writing and reading in memory and in files (RIFF,
// Sony Wave64, RF64; PCM, IEEE float, A-law, mu-law; wide-character paths),
// an AIFF stream read as written by hand, metadata (cue points) written and
// read back, the sample format conversions; a FLAC stream decoded
// sample-exact against the signal its STREAMINFO MD5 belongs to, with seeks
// and its Vorbis comments; an MP3 decoded to its LAME tag's gapless length,
// with a seek table and the low-level frame decoder. Only the module is
// imported; no dr_libs header is included.
#include <clocale>
#include <gtest/gtest.h>

import std;
import mcpplibs.mwrap.dr_libs;

using namespace mcpplibs::mwrap::dr_libs::macros;  // DRWAV_SUCCESS, DR_WAVE_FORMAT_PCM, DRFLAC_METADATA_BLOCK_TYPE_* …

namespace {

std::filesystem::path data_dir() {
    return std::filesystem::path(std::source_location::current().file_name()).parent_path() / "data";
}

std::vector<unsigned char> slurp(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

// tests/data/tone.flac's source (tests/data/README.md): a triangle wave per
// channel (periods 400 and 500) plus 4 bits of LCG noise, interleaved.
std::vector<std::int16_t> flac_signal(unsigned frames, unsigned channels) {
    std::vector<std::int16_t> out;
    std::uint32_t x = 12345;
    for (unsigned n = 0; n < frames; ++n)
        for (unsigned c = 0; c < channels; ++c) {
            x = x * 1664525u + 1013904223u;
            int noise = int((x >> 16) & 15) - 8;
            int p = 400 + 100 * int(c), t = int(n) % p;
            int tri = t < p / 2 ? (t * 4 * 8000) / p - 8000 : 24000 - (t * 4 * 8000) / p;
            out.push_back(std::int16_t(tri + noise));
        }
    return out;
}

double power(std::span<const float> s, double hz, double rate) {  // Goertzel
    const double k = 2.0 * std::cos(2.0 * std::numbers::pi * hz / rate);
    double a = 0, b = 0;
    for (float v : s) {
        double y = v + k * a - b;
        b = a;
        a = y;
    }
    return a * a + b * b - k * a * b;
}

// dr_libs opens a wide path with _wfopen on Windows and, elsewhere, converts
// it with wcsrtombs, which follows the C locale's LC_CTYPE: a program that
// opens non-ASCII wide paths there selects a UTF-8 locale first (the default
// "C" locale converts ASCII only). True when non-ASCII wide paths can be used.
bool utf8_locale() {
#if defined(_WIN32)
    return true;
#else
    for (const char* name : {"C.UTF-8", "C.utf8", "en_US.UTF-8", "UTF-8"})
        if (std::setlocale(LC_CTYPE, name) != nullptr) return true;
    return false;
#endif
}

// The heap, as allocation callbacks of each library. On every platform but
// Windows, the _w functions convert the path into a buffer they allocate
// through the callbacks they are given, and with none (NULL) that allocation
// fails: dr_wav 0.14.5, dr_flac 0.13.3 and dr_mp3 0.7.3 open a wide path there
// only with callbacks (docs/usage.md). The narrow functions take NULL.
template <class Callbacks>
const Callbacks* heap() {
    static const Callbacks callbacks{nullptr, [](std::size_t n, void*) { return std::malloc(n); },
                                     [](void* p, std::size_t n, void*) { return std::realloc(p, n); },
                                     [](void* p, void*) { std::free(p); }};
    return &callbacks;
}

std::vector<drwav_int16> ramp(unsigned samples) {
    std::vector<drwav_int16> pcm(samples);
    for (unsigned i = 0; i < samples; ++i) pcm[i] = drwav_int16(int(i * 131u % 65536u) - 32768);
    return pcm;
}

// A growable in-memory stream with the write and seek procs drwav_init_write* takes.
struct Sink {
    std::vector<unsigned char> bytes;
    std::size_t pos = 0;
    static std::size_t write(void* user, const void* data, std::size_t n) {
        auto* s = static_cast<Sink*>(user);
        if (s->bytes.size() < s->pos + n) s->bytes.resize(s->pos + n);
        std::memcpy(s->bytes.data() + s->pos, data, n);
        s->pos += n;
        return n;
    }
    static drwav_bool32 seek(void* user, int offset, drwav_seek_origin origin) {
        auto* s = static_cast<Sink*>(user);
        std::int64_t base = origin == DRWAV_SEEK_SET ? 0 : origin == DRWAV_SEEK_CUR ? std::int64_t(s->pos) : std::int64_t(s->bytes.size());
        if (base + offset < 0) return DRWAV_FALSE;
        s->pos = std::size_t(base + offset);
        return DRWAV_TRUE;
    }
};

}  // namespace

TEST(Version, TheLinkedLibrarys) {
    drwav_uint32 major = 0, minor = 0, revision = 0;
    drwav_version(&major, &minor, &revision);
    EXPECT_EQ(major, DRWAV_VERSION_MAJOR);
    EXPECT_EQ(minor, DRWAV_VERSION_MINOR);
    EXPECT_EQ(revision, DRWAV_VERSION_REVISION);
    EXPECT_STREQ(drwav_version_string(), "0.14.5");
    EXPECT_STREQ(drflac_version_string(), "0.13.3");
    EXPECT_STREQ(drmp3_version_string(), "0.7.3");
    drflac_uint32 fmaj = 0, fmin = 0, frev = 0;
    drflac_version(&fmaj, &fmin, &frev);
    EXPECT_EQ(fmaj * 10000 + fmin * 100 + frev, 1303u);
}

TEST(Wav, RoundTripEveryContainer) {
    const auto pcm = ramp(2 * 1000);
    for (drwav_container container : {drwav_container_riff, drwav_container_w64, drwav_container_rf64}) {
        drwav_data_format format{container, DR_WAVE_FORMAT_PCM, 2, 48000, 16};
        void* data = nullptr;
        std::size_t size = 0;
        drwav w;
        ASSERT_TRUE(drwav_init_memory_write(&w, &data, &size, &format, nullptr));
        EXPECT_EQ(drwav_write_pcm_frames(&w, 1000, pcm.data()), 1000u);
        EXPECT_EQ(drwav_uninit(&w), DRWAV_SUCCESS);
        EXPECT_EQ(size, drwav_target_write_size_bytes(&format, 1000, nullptr, 0));

        drwav r;
        ASSERT_TRUE(drwav_init_memory(&r, data, size, nullptr));
        EXPECT_EQ(r.container, container);
        EXPECT_EQ(drwav_fmt_get_format(&r.fmt), DR_WAVE_FORMAT_PCM);
        EXPECT_EQ(r.channels, 2u);
        EXPECT_EQ(r.sampleRate, 48000u);
        drwav_uint64 length = 0;
        EXPECT_EQ(drwav_get_length_in_pcm_frames(&r, &length), DRWAV_SUCCESS);
        EXPECT_EQ(length, 1000u);
        std::vector<drwav_int16> back(pcm.size());
        EXPECT_EQ(drwav_read_pcm_frames_s16(&r, 1000, back.data()), 1000u);
        EXPECT_EQ(back, pcm);
        // As f32: each sample s / 32768.
        ASSERT_TRUE(drwav_seek_to_pcm_frame(&r, 250));
        drwav_uint64 cursor = 0;
        EXPECT_EQ(drwav_get_cursor_in_pcm_frames(&r, &cursor), DRWAV_SUCCESS);
        EXPECT_EQ(cursor, 250u);
        std::vector<float> f(2 * 750);
        EXPECT_EQ(drwav_read_pcm_frames_f32(&r, 750, f.data()), 750u);
        for (std::size_t i = 0; i < f.size(); ++i) ASSERT_EQ(f[i], float(pcm[2 * 250 + i]) / 32768.0f);
        drwav_uninit(&r);
        drwav_free(data, nullptr);
    }
}

TEST(Wav, FloatALawMuLaw) {
    std::vector<float> f(512);
    for (std::size_t i = 0; i < f.size(); ++i) f[i] = float(std::sin(double(i) * 0.05)) * 0.75f;
    drwav_data_format format{drwav_container_riff, DR_WAVE_FORMAT_IEEE_FLOAT, 1, 44100, 32};
    void* data = nullptr;
    std::size_t size = 0;
    drwav w;
    ASSERT_TRUE(drwav_init_memory_write(&w, &data, &size, &format, nullptr));
    EXPECT_EQ(drwav_write_pcm_frames(&w, f.size(), f.data()), f.size());
    drwav_uninit(&w);
    unsigned channels = 0, rate = 0;
    drwav_uint64 frames = 0;
    float* back = drwav_open_memory_and_read_pcm_frames_f32(data, size, &channels, &rate, &frames, nullptr);
    ASSERT_NE(back, nullptr);
    EXPECT_EQ(channels, 1u);
    EXPECT_EQ(rate, 44100u);
    ASSERT_EQ(frames, f.size());
    EXPECT_EQ(std::memcmp(back, f.data(), f.size() * sizeof(float)), 0);  // bit-exact
    drwav_free(back, nullptr);
    drwav_free(data, nullptr);

    // Every A-law and mu-law code, through the reader and through the tables.
    std::vector<drwav_uint8> codes(256);
    std::iota(codes.begin(), codes.end(), drwav_uint8(0));
    for (auto [tag, table] : {std::pair{DR_WAVE_FORMAT_ALAW, &drwav_alaw_to_s16}, std::pair{DR_WAVE_FORMAT_MULAW, &drwav_mulaw_to_s16}}) {
        drwav_data_format law{drwav_container_riff, drwav_uint32(tag), 1, 8000, 8};
        data = nullptr;
        size = 0;
        ASSERT_TRUE(drwav_init_memory_write(&w, &data, &size, &law, nullptr));
        EXPECT_EQ(drwav_write_pcm_frames(&w, 256, codes.data()), 256u);
        drwav_uninit(&w);
        std::vector<drwav_int16> reader(256), direct(256);
        table(direct.data(), codes.data(), 256);
        drwav r;
        ASSERT_TRUE(drwav_init_memory(&r, data, size, nullptr));
        EXPECT_EQ(drwav_fmt_get_format(&r.fmt), tag);
        EXPECT_EQ(drwav_read_pcm_frames_s16(&r, 256, reader.data()), 256u);
        EXPECT_EQ(reader, direct);
        drwav_uninit(&r);
        drwav_free(data, nullptr);
    }
    std::array<drwav_int16, 2> s{};
    const drwav_uint8 mu[2] = {0x00, 0xFF};
    drwav_mulaw_to_s16(s.data(), mu, 2);
    EXPECT_EQ(s[0], -32124);
    EXPECT_EQ(s[1], 0);
}

TEST(Wav, AiffWrittenByHand) {
    // FORM AIFF: COMM (2 channels, 4 frames, 16 bits, 44100 Hz as an 80-bit
    // extended float) and SSND, big-endian samples.
    const std::int16_t samples[8] = {1, -1, 1000, -1000, 32767, -32768, 12345, -12345};
    std::vector<unsigned char> f;
    auto be16 = [&](unsigned v) { f.push_back(std::uint8_t(v >> 8)); f.push_back(std::uint8_t(v)); };
    auto be32 = [&](std::uint32_t v) { be16(v >> 16); be16(v & 0xFFFF); };
    auto id = [&](const char* s) { f.insert(f.end(), s, s + 4); };
    id("FORM"); be32(4 + 8 + 18 + 8 + 8 + 16); id("AIFF");
    id("COMM"); be32(18); be16(2); be32(4); be16(16);
    for (unsigned char b : {0x40, 0x0E, 0xAC, 0x44, 0, 0, 0, 0, 0, 0}) f.push_back(b);
    id("SSND"); be32(8 + 16); be32(0); be32(0);
    for (std::int16_t v : samples) be16(std::uint16_t(v));
    drwav r;
    ASSERT_TRUE(drwav_init_memory(&r, f.data(), f.size(), nullptr));
    EXPECT_EQ(r.container, drwav_container_aiff);
    EXPECT_EQ(r.channels, 2u);
    EXPECT_EQ(r.sampleRate, 44100u);
    EXPECT_EQ(r.totalPCMFrameCount, 4u);
    std::int16_t back[8] = {};
    EXPECT_EQ(drwav_read_pcm_frames_s16(&r, 4, back), 4u);
    EXPECT_TRUE(std::ranges::equal(back, samples));
    drwav_uninit(&r);
}

TEST(Wav, CuePointsWrittenAndRead) {
    drwav_cue_point points[2] = {};
    points[0] = {1, 0, {'d', 'a', 't', 'a'}, 0, 0, 100};
    points[1] = {2, 1, {'d', 'a', 't', 'a'}, 0, 0, 700};
    drwav_metadata meta{};
    meta.type = drwav_metadata_type_cue;
    meta.data.cue.cuePointCount = 2;
    meta.data.cue.pCuePoints = points;
    drwav_data_format format{drwav_container_riff, DR_WAVE_FORMAT_PCM, 1, 22050, 16};
    Sink sink;
    drwav w;
    ASSERT_TRUE(drwav_init_write_with_metadata(&w, &format, &Sink::write, &Sink::seek, &sink, nullptr, &meta, 1));
    const auto pcm = ramp(1000);
    EXPECT_EQ(drwav_write_pcm_frames(&w, 1000, pcm.data()), 1000u);
    drwav_uninit(&w);

    drwav r;
    ASSERT_TRUE(drwav_init_memory_with_metadata(&r, sink.bytes.data(), sink.bytes.size(), 0, nullptr));
    drwav_metadata* got = nullptr;
    for (drwav_uint32 i = 0; i < r.metadataCount; ++i)
        if (r.pMetadata[i].type == drwav_metadata_type_cue) got = &r.pMetadata[i];
    ASSERT_NE(got, nullptr);
    ASSERT_EQ(got->data.cue.cuePointCount, 2u);
    EXPECT_EQ(got->data.cue.pCuePoints[0].sampleOffset, 100u);
    EXPECT_EQ(got->data.cue.pCuePoints[1].sampleOffset, 700u);
    EXPECT_TRUE(drwav_fourcc_equal(got->data.cue.pCuePoints[1].dataChunkId, "data"));
    std::vector<drwav_int16> back(1000);
    EXPECT_EQ(drwav_read_pcm_frames_s16(&r, 1000, back.data()), 1000u);
    EXPECT_EQ(back, pcm);
    drwav_metadata* owned = drwav_take_ownership_of_metadata(&r);
    EXPECT_EQ(r.pMetadata, nullptr);
    drwav_uninit(&r);
    drwav_free(owned, nullptr);
}

TEST(Wav, Conversions) {
    const drwav_int16 s16[4] = {-32768, -1, 0, 32767};
    float f[4];
    drwav_s16_to_f32(f, s16, 4);
    EXPECT_EQ(f[0], -1.0f);
    EXPECT_EQ(f[2], 0.0f);
    EXPECT_FLOAT_EQ(f[3], 32767.0f / 32768.0f);
    // f32 to s16 clamps to [-1, 1] and maps it onto [-32768, 32767].
    const float wide_range[4] = {-1.0f, 1.0f, 2.0f, -3.0f};
    drwav_int16 back[4];
    drwav_f32_to_s16(back, wide_range, 4);
    EXPECT_EQ(back[0], -32768);
    EXPECT_EQ(back[1], 32767);
    EXPECT_EQ(back[2], 32767);
    EXPECT_EQ(back[3], -32768);
    const drwav_uint8 u8[3] = {0, 128, 255};
    drwav_int16 from_u8[3];
    drwav_u8_to_s16(from_u8, u8, 3);
    EXPECT_EQ(from_u8[0], -32768);
    EXPECT_EQ(from_u8[1], 0);
    drwav_int32 s32[2];
    drwav_s16_to_s32(s32, s16, 2);
    EXPECT_EQ(s32[0], std::int32_t(-32768) * 65536);
}

TEST(Wav, FilesNarrowAndWide) {
    const auto dir = std::filesystem::temp_directory_path() / "dr_libs-m-api";
    std::filesystem::create_directories(dir);
    const auto pcm = ramp(2 * 300);
    drwav_data_format format{drwav_container_riff, DR_WAVE_FORMAT_PCM, 2, 32000, 16};
    // A narrow path, then a wide one with a character outside ASCII (Windows: _wfopen_s).
    const auto narrow = dir / "narrow.wav";
    ASSERT_TRUE(utf8_locale());
    const std::wstring wide = (dir / L"wide-ü中.wav").wstring();
    drwav w;
    ASSERT_TRUE(drwav_init_file_write(&w, narrow.string().c_str(), &format, nullptr));
    EXPECT_EQ(drwav_write_pcm_frames(&w, 300, pcm.data()), 300u);
    drwav_uninit(&w);
    ASSERT_TRUE(drwav_init_file_write_w(&w, wide.c_str(), &format, heap<drwav_allocation_callbacks>()));
    EXPECT_EQ(drwav_write_pcm_frames(&w, 300, pcm.data()), 300u);
    drwav_uninit(&w);
    for (bool use_wide : {false, true}) {
        drwav r;
        ASSERT_TRUE(use_wide ? drwav_init_file_w(&r, wide.c_str(), heap<drwav_allocation_callbacks>()) : drwav_init_file(&r, narrow.string().c_str(), nullptr));
        std::vector<drwav_int16> back(pcm.size());
        EXPECT_EQ(drwav_read_pcm_frames_s16(&r, 300, back.data()), 300u);
        EXPECT_EQ(back, pcm);
        drwav_uninit(&r);
    }
    EXPECT_TRUE(std::filesystem::exists(std::filesystem::path(wide)));
#if !defined(_WIN32)
    // Upstream behavior: without callbacks, a wide path is not opened here.
    drwav without;
    EXPECT_FALSE(drwav_init_file_w(&without, wide.c_str(), nullptr));
#endif
    drwav r;
    EXPECT_FALSE(drwav_init_file(&r, (dir / "missing.wav").string().c_str(), nullptr));
    std::filesystem::remove_all(dir);
}

namespace {
struct FlacMeta {
    std::array<drflac_uint8, 16> md5{};
    std::string vendor;
    drflac_uint32 comments = ~0u;
    int blocks = 0;
};
void on_meta(void* user, drflac_metadata* m) {
    auto* meta = static_cast<FlacMeta*>(user);
    ++meta->blocks;
    if (m->type == DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO) {
        std::memcpy(meta->md5.data(), m->data.streaminfo.md5, 16);
    } else if (m->type == DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT) {
        meta->vendor.assign(m->data.vorbis_comment.vendor, m->data.vorbis_comment.vendorLength);
        drflac_vorbis_comment_iterator it;
        drflac_init_vorbis_comment_iterator(&it, m->data.vorbis_comment.commentCount, m->data.vorbis_comment.pComments);
        drflac_uint32 length = 0;
        meta->comments = 0;
        while (drflac_next_vorbis_comment(&it, &length) != nullptr) ++meta->comments;
    }
}
}  // namespace

TEST(Flac, SampleExactAgainstItsMd5Signal) {
    const auto bytes = slurp(data_dir() / "tone.flac");
    ASSERT_EQ(bytes.size(), 3097u);
    FlacMeta meta;
    drflac* f = drflac_open_memory_with_metadata(bytes.data(), bytes.size(), &on_meta, &meta, nullptr);
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->channels, 2u);
    EXPECT_EQ(f->sampleRate, 44100u);
    EXPECT_EQ(f->bitsPerSample, 16u);
    EXPECT_EQ(f->totalPCMFrameCount, 2048u);
    EXPECT_EQ(f->container, drflac_container_native);
    // FLAC's MD5 is of the source's interleaved little-endian samples: the
    // signal below, which tests/data/README.md defines (md5sum of its bytes).
    const std::array<drflac_uint8, 16> md5{0x29, 0x02, 0x2c, 0x35, 0x13, 0xe9, 0xa6, 0xfe,
                                           0xa7, 0x6a, 0x96, 0x0f, 0xd2, 0x14, 0x70, 0xc2};
    EXPECT_EQ(meta.md5, md5);
    EXPECT_EQ(meta.vendor, "ffmpeg");
    EXPECT_EQ(meta.comments, 0u);
    EXPECT_EQ(meta.blocks, 2);
    const auto want = flac_signal(2048, 2);
    std::vector<drflac_int16> got(want.size());
    EXPECT_EQ(drflac_read_pcm_frames_s16(f, 2048, got.data()), 2048u);
    EXPECT_EQ(got, want);
    // Seeks: into the third frame (512 frames each), back to the start, to the last frame.
    for (drflac_uint64 at : {1100u, 0u, 2047u, 512u}) {
        ASSERT_TRUE(drflac_seek_to_pcm_frame(f, at));
        EXPECT_EQ(f->currentPCMFrame, at);
        drflac_int16 two[2];
        ASSERT_EQ(drflac_read_pcm_frames_s16(f, 1, two), 1u);
        EXPECT_EQ(two[0], want[2 * at]);
        EXPECT_EQ(two[1], want[2 * at + 1]);
    }
    drflac_close(f);

    // The one-call forms: s32 is s16 << 16, f32 is s16 / 32768.
    unsigned channels = 0, rate = 0;
    drflac_uint64 frames = 0;
    drflac_int32* s32 = drflac_open_memory_and_read_pcm_frames_s32(bytes.data(), bytes.size(), &channels, &rate, &frames, nullptr);
    ASSERT_NE(s32, nullptr);
    ASSERT_EQ(frames, 2048u);
    for (std::size_t i = 0; i < want.size(); ++i) ASSERT_EQ(s32[i], std::int32_t(want[i]) * 65536);
    drflac_free(s32, nullptr);
    float* f32 = drflac_open_memory_and_read_pcm_frames_f32(bytes.data(), bytes.size(), &channels, &rate, &frames, nullptr);
    ASSERT_NE(f32, nullptr);
    for (std::size_t i = 0; i < want.size(); ++i) ASSERT_EQ(f32[i], float(want[i]) / 32768.0f);
    drflac_free(f32, nullptr);
}

TEST(Flac, FilesNarrowAndWide) {
    const auto src = data_dir() / "tone.flac";
    const auto dir = std::filesystem::temp_directory_path() / "dr_libs-m-flac";
    std::filesystem::create_directories(dir);
    ASSERT_TRUE(utf8_locale());
    const auto wide = dir / L"été.flac";
    std::filesystem::copy_file(src, wide, std::filesystem::copy_options::overwrite_existing);
    for (bool use_wide : {false, true}) {
        drflac* f = use_wide ? drflac_open_file_w(wide.wstring().c_str(), heap<drflac_allocation_callbacks>()) : drflac_open_file(src.string().c_str(), nullptr);
        ASSERT_NE(f, nullptr);
        EXPECT_EQ(f->totalPCMFrameCount, 2048u);
        drflac_close(f);
    }
    unsigned channels = 0, rate = 0;
    drflac_uint64 frames = 0;
    drflac_int16* all = drflac_open_file_and_read_pcm_frames_s16(src.string().c_str(), &channels, &rate, &frames, nullptr);
    ASSERT_NE(all, nullptr);
    EXPECT_TRUE(std::equal(all, all + 2 * frames, flac_signal(2048, 2).begin()));
    drflac_free(all, nullptr);
    EXPECT_EQ(drflac_open_file("no such file.flac", nullptr), nullptr);
    std::filesystem::remove_all(dir);
}

TEST(Mp3, GaplessLengthAndTone) {
    const auto bytes = slurp(data_dir() / "tone.mp3");
    ASSERT_EQ(bytes.size(), 1227u);
    drmp3 m;
    ASSERT_TRUE(drmp3_init_memory(&m, bytes.data(), bytes.size(), nullptr));
    EXPECT_EQ(m.channels, 1u);
    EXPECT_EQ(m.sampleRate, 22050u);
    // The LAME tag's encoder delay and padding: exactly the 4410 samples encoded.
    const drmp3_uint64 frames = drmp3_get_pcm_frame_count(&m);
    EXPECT_EQ(frames, 4410u);
    ASSERT_TRUE(drmp3_seek_to_pcm_frame(&m, 0));
    std::vector<float> s(frames);
    EXPECT_EQ(drmp3_read_pcm_frames_f32(&m, frames, s.data()), frames);
    const double p440 = power(s, 440, 22050);
    EXPECT_GT(p440, 1000 * power(s, 1000, 22050));
    EXPECT_GT(p440, 1000 * power(s, 220, 22050));
    float peak = 0;
    for (float v : s) peak = std::max(peak, std::abs(v));
    EXPECT_NEAR(peak, 12000.0f / 32768.0f, 0.05f);

    // A seek lands on the same samples a straight decode gives.
    ASSERT_TRUE(drmp3_seek_to_pcm_frame(&m, 3000));
    std::vector<float> tail(1000);
    EXPECT_EQ(drmp3_read_pcm_frames_f32(&m, 1000, tail.data()), 1000u);
    for (std::size_t i = 0; i < tail.size(); ++i) ASSERT_EQ(tail[i], s[3000 + i]);

    // The stream's frame counts: 10 MP3 frames, 5760 PCM frames before the
    // LAME tag's trimming. (In 0.7.3 this, and drmp3_calculate_seek_points,
    // fail when the stream sits within the tag's encoder delay of its end — they
    // restore the position untrimmed — so ask from the start; docs/usage.md.)
    ASSERT_TRUE(drmp3_seek_to_pcm_frame(&m, 0));
    drmp3_uint64 mp3_frames = 0, raw_frames = 0;
    ASSERT_TRUE(drmp3_get_mp3_and_pcm_frame_count(&m, &mp3_frames, &raw_frames));
    EXPECT_EQ(mp3_frames, 10u);
    EXPECT_EQ(raw_frames, 10u * 576u);
    EXPECT_EQ(drmp3_get_mp3_frame_count(&m), 10u);
    drmp3_uninit(&m);

    // A seek table, computed at the start of the stream (as upstream's
    // documentation does) and bound. In 0.7.3 a seek through the table lands
    // up to a frame away on a stream like this one (upstream reworked seek
    // tables after the tag, bad753d; docs/usage.md), so only the start is
    // compared here: the exact seek is the one without a table, above.
    ASSERT_TRUE(drmp3_init_memory(&m, bytes.data(), bytes.size(), nullptr));
    drmp3_uint32 count = 16;
    std::array<drmp3_seek_point, 16> points{};
    ASSERT_TRUE(drmp3_calculate_seek_points(&m, &count, points.data()));
    EXPECT_GE(count, 1u);
    EXPECT_LE(count, 9u);  // at most one per MP3 frame but the first
    for (drmp3_uint32 i = 1; i < count; ++i) EXPECT_GT(points[i].pcmFrameIndex, points[i - 1].pcmFrameIndex);
    ASSERT_TRUE(drmp3_bind_seek_table(&m, count, points.data()));
    ASSERT_TRUE(drmp3_seek_to_pcm_frame(&m, 0));
    std::vector<float> start(400);
    EXPECT_EQ(drmp3_read_pcm_frames_f32(&m, 400, start.data()), 400u);
    for (std::size_t i = 0; i < start.size(); ++i) ASSERT_EQ(start[i], s[i]);
    drmp3_uninit(&m);

    // s16 through the one-call form.
    drmp3_config config{};
    drmp3_uint64 total = 0;
    drmp3_int16* s16 = drmp3_open_memory_and_read_pcm_frames_s16(bytes.data(), bytes.size(), &config, &total, nullptr);
    ASSERT_NE(s16, nullptr);
    EXPECT_EQ(total, 4410u);
    EXPECT_EQ(config.channels, 1u);
    EXPECT_EQ(config.sampleRate, 22050u);
    drmp3_free(s16, nullptr);
}

TEST(Mp3, FrameDecoder) {
    // The low-level decoder over the raw stream: every frame is MPEG-2 Layer
    // III, 22050 Hz mono, 32 kb/s; the first is the LAME/Info frame.
    const auto bytes = slurp(data_dir() / "tone.mp3");
    drmp3dec dec;
    drmp3dec_init(&dec);
    std::vector<float> pcm(DRMP3_MAX_SAMPLES_PER_FRAME);
    std::size_t pos = 0, decoded = 0;
    int frames = 0;
    while (pos < bytes.size()) {
        drmp3dec_frame_info info{};
        int samples = drmp3dec_decode_frame(&dec, bytes.data() + pos, int(bytes.size() - pos), pcm.data(), &info);
        if (info.frame_bytes == 0) break;
        pos += std::size_t(info.frame_bytes);
        EXPECT_EQ(info.sample_rate, 22050);
        EXPECT_EQ(info.channels, 1);
        EXPECT_EQ(info.layer, 3);
        if (frames > 0) EXPECT_EQ(info.bitrate_kbps, 32);  // the Info frame's own bitrate is the tag's
        ++frames;
        decoded += std::size_t(samples);
    }
    EXPECT_EQ(pos, bytes.size());
    // The Info frame and 10 audio frames; the low-level decoder does not read
    // the tag, so every frame gives its 576 samples, delay and padding included.
    EXPECT_EQ(frames, 11);
    EXPECT_EQ(decoded, 11u * 576u);
}

TEST(Allocation, CustomCallbacks) {
    struct Counter {
        int mallocs = 0, frees = 0;
    } counter;
    drflac_allocation_callbacks cb{};
    cb.pUserData = &counter;
    cb.onMalloc = [](std::size_t n, void* u) -> void* { ++static_cast<Counter*>(u)->mallocs; return std::malloc(n); };
    cb.onRealloc = [](void* p, std::size_t n, void* u) -> void* { (void)u; return std::realloc(p, n); };
    cb.onFree = [](void* p, void* u) { if (p) ++static_cast<Counter*>(u)->frees; std::free(p); };
    const auto bytes = slurp(data_dir() / "tone.flac");
    drflac* f = drflac_open_memory(bytes.data(), bytes.size(), &cb);
    ASSERT_NE(f, nullptr);
    drflac_close(f);
    EXPECT_GT(counter.mallocs, 0);
    EXPECT_EQ(counter.mallocs, counter.frees);
}
