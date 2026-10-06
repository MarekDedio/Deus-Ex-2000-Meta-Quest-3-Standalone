#pragma once

extern "C" bool BuildQuestMapCache(const char* gameRoot, const char* mapName);

// Desktop visual tests keep derived caches in their output directory, never
// beside the user's original installation packages.
extern "C" bool BuildQuestMapCacheToDirectory(
    const char* gameRoot, const char* mapName, const char* outputRoot);
