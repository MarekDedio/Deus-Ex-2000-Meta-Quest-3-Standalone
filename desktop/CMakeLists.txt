cmake_minimum_required(VERSION 3.22.1)
project(deusex_desktop_visual LANGUAGES CXX)

set(SURREAL_ENGINE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../third_party/SurrealEngine/SurrealEngine")
if(NOT EXISTS "${SURREAL_ENGINE_ROOT}/Precomp.cpp")
    message(FATAL_ERROR "Missing pinned SurrealEngine. Run tools/Initialize-ThirdParty.ps1 first.")
endif()
set(PORT_NATIVE "${CMAKE_CURRENT_LIST_DIR}/../native")
add_library(quest_portable_host STATIC
    "${SURREAL_ENGINE_ROOT}/Precomp.cpp"
    "${SURREAL_ENGINE_ROOT}/Utils/File.cpp"
    "${SURREAL_ENGINE_ROOT}/Utils/StrTools.cpp"
    "${SURREAL_ENGINE_ROOT}/Utils/UTF16.cpp"
    "${SURREAL_ENGINE_ROOT}/Package/NameString.cpp"
    "${SURREAL_ENGINE_ROOT}/GC/GC.cpp"
    "${PORT_NATIVE}/surreal_package_stream_reader.cpp"
    "${PORT_NATIVE}/surreal_portable_package_tables.cpp"
    "${PORT_NATIVE}/portable_unreal_runtime.cpp"
    "${PORT_NATIVE}/surreal_gc_probe.cpp"
    "${PORT_NATIVE}/surreal_android_exception.cpp"
    "${PORT_NATIVE}/ue1_package_probe.cpp"
)
target_compile_features(quest_portable_host PUBLIC cxx_std_20)
target_include_directories(quest_portable_host PUBLIC "${SURREAL_ENGINE_ROOT}" "${PORT_NATIVE}")
# Upstream's package headers rely on its Precomp.h for memory/integer types.
target_precompile_headers(quest_portable_host PRIVATE "${SURREAL_ENGINE_ROOT}/Precomp.h")
if(WIN32)
    target_compile_definitions(quest_portable_host PUBLIC WIN32 UNICODE _UNICODE
        WINVER=0x0A00 _WIN32_WINNT=0x0A00 NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(quest_portable_host PUBLIC shlwapi shell32)
endif()

add_executable(deusex_desktop_visual main.cpp visual_renderer.cpp persona_preview.cpp)
target_compile_features(deusex_desktop_visual PRIVATE cxx_std_20)
target_link_libraries(deusex_desktop_visual PRIVATE quest_portable_host)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(deusex_desktop_visual PRIVATE -Wall -Wextra -Werror)
    target_link_options(deusex_desktop_visual PRIVATE -static -static-libgcc -static-libstdc++)
endif()

enable_testing()
add_test(NAME synthetic_visual COMMAND deusex_desktop_visual --self-test
    --width 640 --height 360 --output "${CMAKE_CURRENT_BINARY_DIR}/self-test/capture.bmp")
add_test(NAME persona_canvas COMMAND deusex_desktop_visual --test-persona-canvas)

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/vr_world_transform_test.cpp")
    add_executable(vr_world_transform_test vr_world_transform_test.cpp)
    target_compile_features(vr_world_transform_test PRIVATE cxx_std_17)
    target_include_directories(vr_world_transform_test PRIVATE "${PORT_NATIVE}"
        "${CMAKE_CURRENT_LIST_DIR}/../third_party/Meta-OpenXR-SDK/Samples/1stParty/OVR/Include")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(vr_world_transform_test PRIVATE -Wall -Wextra -Werror)
        target_link_options(vr_world_transform_test PRIVATE -static -static-libgcc -static-libstdc++)
    endif()
    add_test(NAME vr_world_transform COMMAND vr_world_transform_test)
endif()
