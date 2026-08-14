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

