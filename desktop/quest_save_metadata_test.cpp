#include "quest_save_metadata.h"

#include <iostream>

int main() {
    using namespace QuestVr;
    int failures{};
    const auto check = [&](bool passed, const char* label) {
        if (!passed) { ++failures; std::cerr << "FAIL: " << label << '\n'; }
    };
    QuestSaveMetadata original{{1.0f, -2.0f, 3.0f, 0.5f}, "00_Training",
        {{"Mission01.ConversationB", 12u}, {"Mission01.ConversationA", 7u}},
        {"LOG ENTRY", "Second entry"}};
    std::vector<std::uint8_t> bytes;
    check(EncodeQuestSaveMetadata(original, bytes), "encode v4");
    QuestSaveMetadata restored;
    check(DecodeQuestSaveMetadata(bytes, "ignored", restored) &&
        restored.pose == original.pose && restored.mapName == original.mapName &&
        restored.dialogueOffsets == original.dialogueOffsets &&
        restored.personaLogs == original.personaLogs, "v4 exact round trip");
    auto mapLocal = original;
    mapLocal.mapLocalPose = true;
    std::vector<std::uint8_t> v5;
    check(EncodeQuestSaveMetadata(mapLocal, v5) && v5[4] == 5u &&
        DecodeQuestSaveMetadata(v5, "ignored", restored) && restored.mapLocalPose &&
        restored.pose == mapLocal.pose, "v5 map-local pose round trip");
    std::vector<std::uint8_t> deterministic;
    original.dialogueOffsets.clear();
    original.dialogueOffsets.emplace("Mission01.ConversationA", 7u);
    original.dialogueOffsets.emplace("Mission01.ConversationB", 12u);
    check(EncodeQuestSaveMetadata(original, deterministic) && deterministic == bytes,
        "deterministic ordering");
    const auto unchanged = [&](const std::vector<std::uint8_t>& invalid) {
        QuestSaveMetadata sentinel{{9.0f, 9.0f, 9.0f, 9.0f}, "SENTINEL", {}, {"KEEP"}};
        return !DecodeQuestSaveMetadata(invalid, "00_Training", sentinel) &&
            sentinel.mapName == "SENTINEL" && sentinel.personaLogs ==
                std::vector<std::string>{"KEEP"} && sentinel.pose[0] == 9.0f;
    };
    bool allTruncated = true;
    for (std::size_t count = 0u; count < bytes.size(); ++count)
        allTruncated = allTruncated && unchanged({bytes.begin(), bytes.begin() + count});
    check(allTruncated, "every truncation rejects without mutation");
    auto invalid = bytes;
    invalid.push_back(0u);
    check(unchanged(invalid), "trailing bytes reject");
    invalid = bytes;
    invalid[10] = 0x80u; invalid[11] = 0x7fu; // First float becomes +infinity.
    check(unchanged(invalid), "nonfinite pose rejects");
    invalid = bytes;
    invalid[24] = 0xffu; invalid[25] = 0xffu;
    check(unchanged(invalid), "oversized map rejects before allocation");
    invalid = bytes;
    invalid[28] = 0u;
    check(unchanged(invalid), "embedded NUL rejects");
    check(unchanged(std::vector<std::uint8_t>(kQuestSaveMetadataLimit + 1u)),
        "total bound rejects");
    auto v1 = std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 24u);
    v1[4] = 1u;
    check(DecodeQuestSaveMetadata(v1, "LEGACY", restored) && restored.mapName == "LEGACY" &&
        restored.dialogueOffsets.empty() && restored.personaLogs.empty(), "legacy v1");
    auto v2 = std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + 28u + original.mapName.size());
    v2[4] = 2u;
    check(DecodeQuestSaveMetadata(v2, "ignored", restored) && restored.mapName == original.mapName &&
        restored.dialogueOffsets.empty(), "legacy v2");
    const auto v3End = bytes.size() - 4u - (4u + original.personaLogs[0].size()) -
        (4u + original.personaLogs[1].size());
    auto v3 = std::vector<std::uint8_t>(bytes.begin(), bytes.begin() + v3End);
    v3[4] = 3u;
    check(DecodeQuestSaveMetadata(v3, "ignored", restored) &&
        restored.dialogueOffsets == original.dialogueOffsets && restored.personaLogs.empty(),
        "legacy v3");
    auto duplicate = original;
    duplicate.dialogueOffsets = {{"AA", 1u}, {"BB", 2u}};
    check(EncodeQuestSaveMetadata(duplicate, invalid), "duplicate fixture encode");
    const auto secondPath = 28u + duplicate.mapName.size() + 4u + 4u + 2u + 8u + 4u;
    invalid[secondPath] = 'A'; invalid[secondPath + 1u] = 'A';
    check(unchanged(invalid), "duplicate dialogue keys reject");
    auto excessive = original;
    excessive.personaLogs.resize(13u, "X");
    deterministic = {1u, 2u};
    check(!EncodeQuestSaveMetadata(excessive, deterministic) && deterministic ==
        std::vector<std::uint8_t>{1u, 2u}, "invalid encode preserves output");
    excessive = original;
    for (std::size_t index = 0u; index < 100u; ++index)
        excessive.dialogueOffsets.emplace(std::to_string(index) + std::string(1000u, 'x'), 0u);
    check(!EncodeQuestSaveMetadata(excessive, deterministic), "aggregate bound rejects");
    if (failures != 0) return 1;
    std::cout << "Quest save metadata checks passed (including all truncations)\n";
    return 0;
}
