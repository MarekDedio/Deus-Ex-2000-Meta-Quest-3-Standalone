#pragma once
#include "quest_input_source.h"
// CPU-owner binding to a prepared native viewport and the SAME render lifetime.
// This binds raw observations only; it does not initialize original Input,
// implement UpdateInput/ReadInput, change the viewport class or claim RenDev.
void BindPortableRuntimeQuestInputSource(std::uint64_t viewportIdentity,
    std::shared_ptr<QuestVr::QuestInputSource> source);
QuestVr::QuestInputSource::Snapshot ReadPortableRuntimeQuestInputSource(std::uint64_t viewportIdentity);
QuestVr::QuestInputSource::Batch StagePortableRuntimeQuestInputSource(std::uint64_t viewportIdentity,
    const QuestVr::QuestInputCursor& cursor,bool reset=false);
