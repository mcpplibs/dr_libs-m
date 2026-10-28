// dr_libs — describe audio files and decode them to WAV, through
// import mcpplibs.mwrap.dr_libs.
//
//   dr_libs info <file>...                    format, channels, rate, length, peak and RMS
//   dr_libs decode <in> <out.wav> [--float]   any WAV/AIFF/FLAC/MP3 to a 16-bit (or float) WAV
//
// The container is recognised by its first bytes (RIFF, RIFX, RF64, Wave64,
// FORM/AIFF: dr_wav; fLaC, OggS: dr_flac; anything else is tried as MP3), and
// every decoder streams: a file is read a block at a time, never whole. A
// 16-bit source decodes to a 16-bit WAV sample-exact.
import std;
import mcpplibs.mwrap.dr_libs;

using namespace mcpplibs::mwrap::dr_libs::macros;

namespace {

enum class Kind { wav, flac, mp3 };

// One decoder of any of the three, reading float frames.
class Source {
public:
    // On the heap and never moved: a drwav or drmp3 may point into itself.
    static std::unique_ptr<Source> open(const std::string& path) {
        std::ifstream in(path, std::ios::binary);
        if (!in) return nullptr;
        char magic[4] = {};
        in.read(magic, 4);
        const std::string_view m(magic, 4);
        std::unique_ptr<Source> p(new Source);
        Source& s = *p;
        if (m == "RIFF" || m == "RIFX" || m == "RF64" || m == "riff" || m == "FORM") {
            s.kind_ = Kind::wav;
            if (!drwav_init_file(&s.wav_, path.c_str(), nullptr)) return nullptr;
            s.channels_ = s.wav_.channels;
            s.rate_ = s.wav_.sampleRate;
            s.frames_ = s.wav_.totalPCMFrameCount;
            s.format_ = describe_wav(s.wav_);
        } else if (m == "fLaC" || m == "OggS") {
            s.kind_ = Kind::flac;
            s.flac_ = drflac_open_file(path.c_str(), nullptr);
            if (s.flac_ == nullptr) return nullptr;
            s.channels_ = s.flac_->channels;
            s.rate_ = s.flac_->sampleRate;
            s.frames_ = s.flac_->totalPCMFrameCount;
            s.format_ = std::format("FLAC{}, {}-bit", s.flac_->container == drflac_container_ogg ? " (Ogg)" : "", s.flac_->bitsPerSample);
        } else {
            s.kind_ = Kind::mp3;
            if (!drmp3_init_file(&s.mp3_, path.c_str(), nullptr)) return nullptr;
            s.channels_ = s.mp3_.channels;
            s.rate_ = s.mp3_.sampleRate;
            s.frames_ = drmp3_get_pcm_frame_count(&s.mp3_);
            drmp3_seek_to_pcm_frame(&s.mp3_, 0);
            s.format_ = std::format("MP3, {} MPEG frames", drmp3_get_mp3_frame_count(&s.mp3_));
            drmp3_seek_to_pcm_frame(&s.mp3_, 0);
        }
        s.open_ = true;
        return p;
    }

    Source(const Source&) = delete;
    Source& operator=(const Source&) = delete;
    ~Source() {
        if (!open_) return;
        switch (kind_) {
        case Kind::wav: drwav_uninit(&wav_); break;
        case Kind::flac: drflac_close(flac_); break;
        case Kind::mp3: drmp3_uninit(&mp3_); break;
        }
    }

    std::uint64_t read(std::uint64_t frames, float* out) {
        switch (kind_) {
        case Kind::wav: return drwav_read_pcm_frames_f32(&wav_, frames, out);
        case Kind::flac: return drflac_read_pcm_frames_f32(flac_, frames, out);
        case Kind::mp3: return drmp3_read_pcm_frames_f32(&mp3_, frames, out);
        }
        return 0;
    }

    std::uint64_t read(std::uint64_t frames, drwav_int16* out) {
        switch (kind_) {
        case Kind::wav: return drwav_read_pcm_frames_s16(&wav_, frames, out);
        case Kind::flac: return drflac_read_pcm_frames_s16(flac_, frames, out);
        case Kind::mp3: return drmp3_read_pcm_frames_s16(&mp3_, frames, out);
        }
        return 0;
    }

    unsigned channels() const { return channels_; }
    unsigned rate() const { return rate_; }
    std::uint64_t frames() const { return frames_; }
    const std::string& format() const { return format_; }

private:
    Source() = default;

    static std::string describe_wav(const drwav& w) {
        const char* container = w.container == drwav_container_riff  ? "WAV"
                                : w.container == drwav_container_rifx ? "WAV (RIFX)"
                                : w.container == drwav_container_w64  ? "Wave64"
                                : w.container == drwav_container_rf64 ? "RF64"
                                                                      : "AIFF";
        const unsigned tag = drwav_fmt_get_format(&w.fmt);
        const char* codec = tag == DR_WAVE_FORMAT_PCM          ? "PCM"
                            : tag == DR_WAVE_FORMAT_IEEE_FLOAT ? "IEEE float"
                            : tag == DR_WAVE_FORMAT_ALAW       ? "A-law"
                            : tag == DR_WAVE_FORMAT_MULAW      ? "mu-law"
                            : tag == DR_WAVE_FORMAT_ADPCM      ? "MS ADPCM"
                            : tag == DR_WAVE_FORMAT_DVI_ADPCM  ? "IMA ADPCM"
                                                               : "other";
        return std::format("{}, {} {}-bit", container, codec, w.bitsPerSample);
    }

    bool open_ = false;
    Kind kind_ = Kind::wav;
    drwav wav_{};
    drflac* flac_ = nullptr;
    drmp3 mp3_{};
    unsigned channels_ = 0, rate_ = 0;
    std::uint64_t frames_ = 0;
    std::string format_;
};

int info(const std::string& path) {
    auto source = Source::open(path);
    if (!source) {
        std::println(std::cerr, "{}: not a WAV, AIFF, FLAC or MP3 file dr_libs can read", path);
        return 1;
    }
    std::vector<float> block(4096 * source->channels());
    double peak = 0, sum = 0;
    std::uint64_t decoded = 0;
    for (;;) {
        const std::uint64_t n = source->read(4096, block.data());
        if (n == 0) break;
        for (std::size_t i = 0; i < n * source->channels(); ++i) {
            peak = std::max(peak, double(std::abs(block[i])));
            sum += double(block[i]) * block[i];
        }
        decoded += n;
    }
    const double rms = decoded ? std::sqrt(sum / double(decoded * source->channels())) : 0;
    auto db = [](double v) { return v > 0 ? 20 * std::log10(v) : -std::numeric_limits<double>::infinity(); };
    std::println("{}: {}, {} ch, {} Hz, {} frames ({:.3f} s), peak {:.1f} dBFS, RMS {:.1f} dBFS", path, source->format(),
                 source->channels(), source->rate(), decoded, double(decoded) / source->rate(), db(peak), db(rms));
    if (decoded != source->frames()) {
        std::println(std::cerr, "{}: the header says {} frames, {} decoded", path, source->frames(), decoded);
        return 1;
    }
    return 0;
}

int decode(const std::string& in, const std::string& out, bool as_float) {
    auto source = Source::open(in);
    if (!source) {
        std::println(std::cerr, "{}: not a WAV, AIFF, FLAC or MP3 file dr_libs can read", in);
        return 1;
    }
    drwav_data_format format{drwav_container_riff, drwav_uint32(as_float ? DR_WAVE_FORMAT_IEEE_FLOAT : DR_WAVE_FORMAT_PCM),
                             source->channels(), source->rate(), as_float ? 32u : 16u};
    drwav w;
    if (!drwav_init_file_write(&w, out.c_str(), &format, nullptr)) {
        std::println(std::cerr, "{}: cannot write", out);
        return 1;
    }
    std::vector<float> block(4096 * source->channels());
    std::vector<drwav_int16> pcm(block.size());
    std::uint64_t total = 0;
    // Each decoder's own s16 or f32 output: a 16-bit source decodes to 16
    // bits sample-exact.
    for (std::uint64_t n; (n = as_float ? source->read(4096, block.data()) : source->read(4096, pcm.data())) != 0; total += n)
        drwav_write_pcm_frames(&w, n, as_float ? static_cast<const void*>(block.data()) : pcm.data());
    drwav_uninit(&w);
    std::println("{} -> {}: {} frames, {} ch, {} Hz, {}", in, out, total, source->channels(), source->rate(),
                 as_float ? "IEEE float 32-bit" : "PCM 16-bit");
    return 0;
}

int usage() {
    std::println(std::cerr, "usage: dr_libs info <file>...\n       dr_libs decode <in> <out.wav> [--float]");
    return 2;
}

}  // namespace

int main(int argc, char** argv) {
    std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) return usage();
    if (args[0] == "info" && args.size() >= 2) {
        int status = 0;
        for (std::size_t i = 1; i < args.size(); ++i) status |= info(args[i]);
        return status;
    }
    if (args[0] == "decode" && (args.size() == 3 || (args.size() == 4 && args[3] == "--float")))
        return decode(args[1], args[2], args.size() == 4);
    return usage();
}
