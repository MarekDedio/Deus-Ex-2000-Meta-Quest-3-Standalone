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
target_include_directories(deusex_desktop_visual PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/Meta-OpenXR-SDK/Samples/1stParty/OVR/Include")
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

add_executable(frame_work_budget_test frame_work_budget_test.cpp)
target_compile_features(frame_work_budget_test PRIVATE cxx_std_17)
target_include_directories(frame_work_budget_test PRIVATE "${PORT_NATIVE}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(frame_work_budget_test PRIVATE -Wall -Wextra -Werror)
    target_link_options(frame_work_budget_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME frame_work_budget COMMAND frame_work_budget_test)

add_executable(map_transition_transaction_test map_transition_transaction_test.cpp)
target_compile_features(map_transition_transaction_test PRIVATE cxx_std_17)
target_include_directories(map_transition_transaction_test PRIVATE "${PORT_NATIVE}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(map_transition_transaction_test PRIVATE -Wall -Wextra -Werror)
    target_link_options(map_transition_transaction_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME map_transition_transaction COMMAND map_transition_transaction_test)

add_executable(portable_bitmap_font_test portable_bitmap_font_test.cpp)
target_compile_features(portable_bitmap_font_test PRIVATE cxx_std_20)
target_link_libraries(portable_bitmap_font_test PRIVATE quest_portable_host)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(portable_bitmap_font_test PRIVATE -Wall -Wextra -Werror)
    target_link_options(portable_bitmap_font_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME portable_bitmap_font COMMAND portable_bitmap_font_test)

add_executable(portable_runtime_state_test portable_runtime_state_test.cpp)
target_compile_features(portable_runtime_state_test PRIVATE cxx_std_20)
target_link_libraries(portable_runtime_state_test PRIVATE quest_portable_host)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(portable_runtime_state_test PRIVATE -Wall -Wextra -Werror)
    target_link_options(portable_runtime_state_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME portable_runtime_state COMMAND portable_runtime_state_test)
set_tests_properties(portable_runtime_state PROPERTIES SKIP_RETURN_CODE 77)

add_executable(quest_save_metadata_test quest_save_metadata_test.cpp)
target_compile_features(quest_save_metadata_test PRIVATE cxx_std_17)
target_include_directories(quest_save_metadata_test PRIVATE "${PORT_NATIVE}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(quest_save_metadata_test PRIVATE -Wall -Wextra -Werror)
    target_link_options(quest_save_metadata_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME quest_save_metadata COMMAND quest_save_metadata_test)

add_executable(portable_mp3_audio_test portable_mp3_audio_test.cpp)
target_compile_features(portable_mp3_audio_test PRIVATE cxx_std_20)
target_include_directories(portable_mp3_audio_test PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/SurrealEngine/Thirdparty")
target_link_libraries(portable_mp3_audio_test PRIVATE quest_portable_host)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(portable_mp3_audio_test PRIVATE -Wall -Wextra -Werror)
    target_link_options(portable_mp3_audio_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME portable_mp3_audio COMMAND portable_mp3_audio_test)

add_executable(quest_map_lighting_test quest_map_lighting_test.cpp
    "${SURREAL_ENGINE_ROOT}/Math/hsb.cpp")
target_compile_features(quest_map_lighting_test PRIVATE cxx_std_17)
target_include_directories(quest_map_lighting_test PRIVATE "${PORT_NATIVE}" "${SURREAL_ENGINE_ROOT}"
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/Meta-OpenXR-SDK/Samples/1stParty/OVR/Include")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(quest_map_lighting_test PRIVATE -Wall -Wextra -Werror)
    target_link_options(quest_map_lighting_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME quest_map_lighting COMMAND quest_map_lighting_test)

add_executable(quest_save_bundle_test quest_save_bundle_test.cpp)
target_compile_features(quest_save_bundle_test PRIVATE cxx_std_17)
target_include_directories(quest_save_bundle_test PRIVATE "${PORT_NATIVE}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(quest_save_bundle_test PRIVATE -Wall -Wextra -Werror)
    target_link_options(quest_save_bundle_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME quest_save_bundle COMMAND quest_save_bundle_test)

add_executable(async_result_epoch_test async_result_epoch_test.cpp)
target_compile_features(async_result_epoch_test PRIVATE cxx_std_17)
target_include_directories(async_result_epoch_test PRIVATE "${PORT_NATIVE}")
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(async_result_epoch_test PRIVATE -Wall -Wextra -Werror)
    target_link_options(async_result_epoch_test PRIVATE -static -static-libgcc -static-libstdc++)
endif()
add_test(NAME async_result_epoch COMMAND async_result_epoch_test)
