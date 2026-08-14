cmake_minimum_required(VERSION 3.22.1)
project(deusex_quest_port LANGUAGES C CXX)

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/Meta-OpenXR-SDK/Samples"
    "${CMAKE_CURRENT_BINARY_DIR}/meta_openxr_samples"
)

set(SURREAL_ENGINE_ROOT
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/SurrealEngine/SurrealEngine")
add_library(surreal_portable_core STATIC
    "${SURREAL_ENGINE_ROOT}/Precomp.cpp"
    "${SURREAL_ENGINE_ROOT}/Utils/File.cpp"
    "${SURREAL_ENGINE_ROOT}/Utils/StrTools.cpp"
    "${SURREAL_ENGINE_ROOT}/Package/NameString.cpp"
    surreal_package_stream_reader.cpp
    surreal_portable_package_tables.cpp
    surreal_android_exception.cpp
)
target_compile_features(surreal_portable_core PUBLIC cxx_std_20)
target_compile_options(surreal_portable_core PRIVATE -Wall -Wextra)
target_include_directories(surreal_portable_core PUBLIC "${SURREAL_ENGINE_ROOT}")

add_library(deusex_data_probe SHARED ue1_package_probe.cpp)
target_compile_features(deusex_data_probe PRIVATE cxx_std_20)
target_compile_options(deusex_data_probe PRIVATE -Wall -Wextra -Werror)
target_link_libraries(deusex_data_probe PRIVATE log surreal_portable_core)
target_link_options(deusex_data_probe PRIVATE -Wl,--gc-sections)

add_library(deusex_quest MODULE quest_main.cpp)
target_compile_features(deusex_quest PRIVATE cxx_std_17)
target_compile_options(deusex_quest PRIVATE -Wall -Wextra -Werror)
target_include_directories(deusex_quest PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/Meta-OpenXR-SDK/Samples/SampleXrFramework/Src"
    "${ANDROID_NDK}/sources/android/native_app_glue"
)
target_link_libraries(deusex_quest PRIVATE
    android
    aaudio
    EGL
    GLESv3
    log
    ktx
    samplexrframework
)
set_target_properties(deusex_quest PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")
