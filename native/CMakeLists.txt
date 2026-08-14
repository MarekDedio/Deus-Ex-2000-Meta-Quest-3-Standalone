cmake_minimum_required(VERSION 3.22.1)
project(deusex_quest_port LANGUAGES C CXX)

add_subdirectory(
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/Meta-OpenXR-SDK/Samples"
    "${CMAKE_CURRENT_BINARY_DIR}/meta_openxr_samples"
)

add_library(deusex_data_probe SHARED ue1_package_probe.cpp)
target_compile_features(deusex_data_probe PRIVATE cxx_std_17)
target_compile_options(deusex_data_probe PRIVATE -Wall -Wextra -Werror)
target_link_libraries(deusex_data_probe PRIVATE log)

add_library(deusex_quest MODULE quest_main.cpp)
target_compile_features(deusex_quest PRIVATE cxx_std_17)
target_compile_options(deusex_quest PRIVATE -Wall -Wextra -Werror)
target_include_directories(deusex_quest PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/Meta-OpenXR-SDK/Samples/SampleXrFramework/Src"
    "${ANDROID_NDK}/sources/android/native_app_glue"
)
target_link_libraries(deusex_quest PRIVATE
    android
    EGL
    GLESv3
    log
    ktx
    samplexrframework
)
set_target_properties(deusex_quest PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")
