// The macros module on its own (no header, no other module): the constants
// are constant expressions of their macros' types, so they work in
// constant-initialised tables, case labels and static_asserts.
import std;
import mcpplibs.mwrap.dr_libs.macros;

namespace macros = mcpplibs::mwrap::dr_libs::macros;
using namespace mcpplibs::mwrap::dr_libs::macros;

// Every constant has the type its macro expands to.
static_assert(std::is_same_v<decltype(DRWAV_SUCCESS), const int> && DRWAV_SUCCESS == 0);
static_assert(std::is_same_v<decltype(DRWAV_AT_END), const int> && DRWAV_AT_END == -53);
static_assert(std::is_same_v<decltype(DR_WAVE_FORMAT_EXTENSIBLE), const int> && DR_WAVE_FORMAT_EXTENSIBLE == 0xFFFE);
static_assert(DR_WAVE_FORMAT_PCM == 1 && DR_WAVE_FORMAT_IEEE_FLOAT == 3 && DR_WAVE_FORMAT_ALAW == 6 && DR_WAVE_FORMAT_MULAW == 7);
static_assert(DRWAV_SEQUENTIAL == 1 && DRWAV_WITH_METADATA == 2);
static_assert(DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO == 0 && DRFLAC_METADATA_BLOCK_TYPE_PICTURE == 6);
static_assert(DRFLAC_METADATA_BLOCK_TYPE_INVALID == 127 && DRFLAC_PICTURE_TYPE_COVER_FRONT == 3);
static_assert(DR_FLAC_BUFFER_SIZE == 4096);  // the build's (upstream's default)
static_assert(std::is_same_v<decltype(DRMP3_UINT64_MAX), const unsigned long long>);  // drmp3_uint64
static_assert(DRMP3_UINT64_MAX == ~0ull);
static_assert(DRMP3_MAX_PCM_FRAMES_PER_MP3_FRAME == 1152 && DRMP3_MAX_SAMPLES_PER_FRAME == 2304);
static_assert(DRMP3_SUCCESS == DRWAV_SUCCESS && DRMP3_INVALID_FILE == DRWAV_INVALID_FILE);  // one code table
static_assert(DRWAV_TRUE == 1 && DRFLAC_FALSE == 0 && DRMP3_TRUE == 1);

// The versions: the pinned commit's three headers.
static_assert(std::is_same_v<decltype(DRWAV_VERSION_STRING), const char (&)[7]>);
static_assert(std::string_view(DRWAV_VERSION_STRING) == "0.14.5" && DRWAV_VERSION_MINOR == 14);
static_assert(std::string_view(DRFLAC_VERSION_STRING) == "0.13.3" && DRFLAC_VERSION_REVISION == 3);
static_assert(std::string_view(DRMP3_VERSION_STRING) == "0.7.3" && DRMP3_VERSION_MAJOR == 0);

// A constant-initialised table and a switch, as the macros allow.
constexpr int formats[] = {DR_WAVE_FORMAT_PCM, DR_WAVE_FORMAT_ADPCM, DR_WAVE_FORMAT_IEEE_FLOAT};
static_assert(formats[2] == 3);

constexpr std::string_view name(int result) {
    switch (result) {
    case DRWAV_SUCCESS: return "success";
    case DRWAV_INVALID_FILE: return "invalid file";
    case DRWAV_OUT_OF_MEMORY: return "out of memory";
    default: return "other";
    }
}
static_assert(name(DRWAV_INVALID_FILE) == "invalid file");

int main() {
    return macros::DRWAV_VERSION_MAJOR == 0 && macros::DRMP3_MAX_BITRESERVOIR_BYTES == 511 ? 0 : 1;
}
