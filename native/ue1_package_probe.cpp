#include <jni.h>
#include <android/log.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* kLogTag = "DeusExQuest";
constexpr std::uint32_t kUe1PackageSignature = 0x9E2A83C1u;

struct PackageSummary {
    std::uint16_t version{};
    std::uint16_t licenseeMode{};
    std::uint32_t flags{};
    std::uint32_t nameCount{};
    std::uint32_t exportCount{};
    std::uint32_t importCount{};
};

bool ReadExact(std::FILE* file, void* output, std::size_t bytes) {
    return std::fread(output, 1, bytes, file) == bytes;
}

bool ReadU16(std::FILE* file, std::uint16_t& output) {
    std::uint8_t bytes[2]{};
    if (!ReadExact(file, bytes, sizeof(bytes))) {
        return false;
    }
    output = static_cast<std::uint16_t>(bytes[0]) |
        (static_cast<std::uint16_t>(bytes[1]) << 8u);
    return true;
}

bool ReadU32(std::FILE* file, std::uint32_t& output) {
    std::uint8_t bytes[4]{};
    if (!ReadExact(file, bytes, sizeof(bytes))) {
        return false;
    }
    output = static_cast<std::uint32_t>(bytes[0]) |
        (static_cast<std::uint32_t>(bytes[1]) << 8u) |
        (static_cast<std::uint32_t>(bytes[2]) << 16u) |
        (static_cast<std::uint32_t>(bytes[3]) << 24u);
    return true;
}

bool ProbePackage(const std::string& path, PackageSummary& summary) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Cannot open %s", path.c_str());
        return false;
    }

    std::uint32_t signature{};
    std::uint32_t nameOffset{};
    std::uint32_t exportOffset{};
    std::uint32_t importOffset{};
    const bool headerRead =
        ReadU32(file, signature) &&
        ReadU16(file, summary.version) &&
        ReadU16(file, summary.licenseeMode) &&
        ReadU32(file, summary.flags) &&
        ReadU32(file, summary.nameCount) &&
        ReadU32(file, nameOffset) &&
        ReadU32(file, summary.exportCount) &&
        ReadU32(file, exportOffset) &&
        ReadU32(file, summary.importCount) &&
        ReadU32(file, importOffset);

    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);
    std::fclose(file);

    const bool offsetsValid = length >= 0 &&
        nameOffset <= static_cast<std::uint64_t>(length) &&
        exportOffset <= static_cast<std::uint64_t>(length) &&
        importOffset <= static_cast<std::uint64_t>(length);
    if (!headerRead || signature != kUe1PackageSignature ||
        summary.version != 68 || !offsetsValid) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "Invalid UE1 package header: %s signature=%08x version=%u bytes=%ld",
            path.c_str(),
            signature,
            summary.version,
            length);
        return false;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "UE1 package OK: %s version=%u names=%u exports=%u imports=%u bytes=%ld",
        path.c_str(),
        summary.version,
        summary.nameCount,
        summary.exportCount,
        summary.importCount,
        length);
    return true;
}

}  // namespace

extern "C" JNIEXPORT jboolean JNICALL
Java_dev_deusex_questvr_MainActivity_probeGameData(
    JNIEnv* env,
    jclass,
    jstring gameRoot) {
    if (gameRoot == nullptr) {
        return JNI_FALSE;
    }

    const char* rootChars = env->GetStringUTFChars(gameRoot, nullptr);
    if (rootChars == nullptr) {
        return JNI_FALSE;
    }
    const std::string root(rootChars);
    env->ReleaseStringUTFChars(gameRoot, rootChars);

    PackageSummary training{};
    PackageSummary gameScripts{};
    const bool trainingValid = ProbePackage(root + "/Maps/00_Training.dx", training);
    const bool scriptsValid = ProbePackage(root + "/System/DeusEx.u", gameScripts);
    return trainingValid && scriptsValid ? JNI_TRUE : JNI_FALSE;
}

