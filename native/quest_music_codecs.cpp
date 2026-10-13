// One implementation shared by tracker compressed samples and original MP3
// dialogue. Do not also define MINIMP3_IMPLEMENTATION in quest_main.cpp.
// CMake prefixes the newer Vorbis API to isolate Meta's older implementation.
#define MINIMP3_IMPLEMENTATION
#include "minimp3_ex.h"
#include "stb_vorbis.h"
