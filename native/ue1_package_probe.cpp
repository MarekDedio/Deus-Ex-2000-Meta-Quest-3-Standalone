#include <jni.h>
#include <android/log.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr const char* kLogTag = "DeusExQuest";
constexpr std::uint32_t kUe1PackageSignature = 0x9E2A83C1u;

struct PackageSummary {
    std::uint16_t version{};
    std::uint16_t licenseeMode{};
    std::uint32_t flags{};
    std::uint32_t nameCount{};
    std::uint32_t nameOffset{};
    std::uint32_t exportCount{};
    std::uint32_t exportOffset{};
    std::uint32_t importCount{};
    std::uint32_t importOffset{};
    std::uint32_t namesParsed{};
    std::string firstName;
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

bool ReadCompactIndex(std::FILE* file, std::int32_t& output) {
    std::uint8_t value{};
    if (!ReadExact(file, &value, 1)) {
        return false;
    }

    const bool negative = (value & 0x80u) != 0;
    std::uint32_t magnitude = value & 0x3fu;
    bool more = (value & 0x40u) != 0;
    unsigned shift = 6;
    while (more && shift < 32) {
        if (!ReadExact(file, &value, 1)) {
            return false;
        }
        magnitude |= static_cast<std::uint32_t>(value & 0x7fu) << shift;
        more = (value & 0x80u) != 0;
        shift += 7;
    }
    if (more || magnitude > 0x7fffffffu) {
        return false;
    }
    output = negative ? -static_cast<std::int32_t>(magnitude)
                      : static_cast<std::int32_t>(magnitude);
    return true;
}

bool ReadNameTable(std::FILE* file, PackageSummary& summary) {
    if (std::fseek(file, static_cast<long>(summary.nameOffset), SEEK_SET) != 0) {
        return false;
    }

    for (std::uint32_t index = 0; index < summary.nameCount; ++index) {
        std::int32_t serializedLength{};
        if (!ReadCompactIndex(file, serializedLength) ||
            serializedLength <= 0 || serializedLength > 4096) {
            return false;
        }

        std::vector<char> bytes(static_cast<std::size_t>(serializedLength));
        std::uint32_t flags{};
        if (!ReadExact(file, bytes.data(), bytes.size()) ||
            bytes.back() != '\0' || !ReadU32(file, flags)) {
            return false;
        }
        if (index == 0) {
            summary.firstName.assign(bytes.data(), bytes.size() - 1);
        }
        ++summary.namesParsed;
    }
    return summary.namesParsed == summary.nameCount && !summary.firstName.empty();
}

bool ProbePackage(const std::string& path, PackageSummary& summary) {
    std::FILE* file = std::fopen(path.c_str(), "rb");
    if (file == nullptr) {
        __android_log_print(ANDROID_LOG_ERROR, kLogTag, "Cannot open %s", path.c_str());
        return false;
    }

    std::uint32_t signature{};
    const bool headerRead =
        ReadU32(file, signature) &&
        ReadU16(file, summary.version) &&
        ReadU16(file, summary.licenseeMode) &&
        ReadU32(file, summary.flags) &&
        ReadU32(file, summary.nameCount) &&
        ReadU32(file, summary.nameOffset) &&
        ReadU32(file, summary.exportCount) &&
        ReadU32(file, summary.exportOffset) &&
        ReadU32(file, summary.importCount) &&
        ReadU32(file, summary.importOffset);

    std::fseek(file, 0, SEEK_END);
    const long length = std::ftell(file);

    const bool offsetsValid = length >= 0 &&
        summary.nameOffset <= static_cast<std::uint64_t>(length) &&
        summary.exportOffset <= static_cast<std::uint64_t>(length) &&
        summary.importOffset <= static_cast<std::uint64_t>(length);
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
        std::fclose(file);
        return false;
    }

    const bool namesValid = ReadNameTable(file, summary);
    std::fclose(file);
    if (!namesValid) {
        __android_log_print(
            ANDROID_LOG_ERROR,
            kLogTag,
            "Invalid UE1 name table: %s parsed=%u expected=%u first=%s",
            path.c_str(),
            summary.namesParsed,
            summary.nameCount,
            summary.firstName.c_str());
        return false;
    }

    __android_log_print(
        ANDROID_LOG_INFO,
        kLogTag,
        "UE1 package OK: %s version=%u names=%u exports=%u imports=%u bytes=%ld first=%s",
        path.c_str(),
        summary.version,
        summary.nameCount,
        summary.exportCount,
        summary.importCount,
        length,
        summary.firstName.c_str());
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
    const bool valid = trainingValid && scriptsValid;

    const std::string resultPath = root + "/quest-package-probe.txt";
    if (std::FILE* result = std::fopen(resultPath.c_str(), "wb")) {
        std::fprintf(
            result,
            "result=%s\n"
            "training.version=%u\ntraining.names=%u\ntraining.names_parsed=%u\ntraining.first_name=%s\ntraining.exports=%u\ntraining.imports=%u\n"
            "scripts.version=%u\nscripts.names=%u\nscripts.names_parsed=%u\nscripts.first_name=%s\nscripts.exports=%u\nscripts.imports=%u\n",
            valid ? "ok" : "failed",
            training.version,
            training.nameCount,
            training.namesParsed,
            training.firstName.c_str(),
            training.exportCount,
            training.importCount,
            gameScripts.version,
            gameScripts.nameCount,
            gameScripts.namesParsed,
            gameScripts.firstName.c_str(),
            gameScripts.exportCount,
            gameScripts.importCount);
        std::fclose(result);
    } else {
        __android_log_print(
            ANDROID_LOG_ERROR, kLogTag, "Cannot write probe result: %s", resultPath.c_str());
    }

    return valid ? JNI_TRUE : JNI_FALSE;
}
