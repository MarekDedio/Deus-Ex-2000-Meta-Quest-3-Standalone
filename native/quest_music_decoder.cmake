# The decoder is already supplied by the pinned SurrealEngine dependency.
# Never bundle the user's commercial UMX packages in the library or APK.
add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../third_party/SurrealEngine/Thirdparty/openmpt"
    "${CMAKE_CURRENT_BINARY_DIR}/quest_openmpt")
set_target_properties(openmpt PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(quest_music_codecs STATIC "${CMAKE_CURRENT_LIST_DIR}/quest_music_codecs.cpp")
target_compile_features(quest_music_codecs PRIVATE cxx_std_20)
set_target_properties(quest_music_codecs PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(quest_music_codecs PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/SurrealEngine/Thirdparty")
# Upstream's standalone openmpt target expects its embedding AudioSource.cpp
# to supply these codecs. Keep its newer Vorbis implementation separate from
# the older one linked by Meta's sample framework (including its opaque handles).
# Do not resolve one version's declarations against the other's implementation.
set(QUEST_MUSIC_VORBIS_API
    get_info get_comment get_error close get_sample_offset get_file_offset
    open_pushdata decode_frame_pushdata flush_pushdata decode_filename decode_memory
    open_memory open_filename open_file open_file_section seek_frame seek seek_start
    stream_length_in_samples stream_length_in_seconds get_frame_float
    get_frame_short_interleaved get_frame_short get_samples_float_interleaved
    get_samples_float get_samples_short_interleaved get_samples_short)
foreach(API IN LISTS QUEST_MUSIC_VORBIS_API)
    target_compile_definitions(openmpt PRIVATE
        "stb_vorbis_${API}=quest_music_vorbis_${API}")
    target_compile_definitions(quest_music_codecs PRIVATE
        "stb_vorbis_${API}=quest_music_vorbis_${API}")
endforeach()
target_link_libraries(openmpt PUBLIC quest_music_codecs)
add_library(quest_music_decoder STATIC "${CMAKE_CURRENT_LIST_DIR}/quest_music_stream.cpp")
target_compile_features(quest_music_decoder PRIVATE cxx_std_20)
set_target_properties(quest_music_decoder PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(quest_music_decoder PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../third_party/SurrealEngine/Thirdparty/openmpt")
target_link_libraries(quest_music_decoder PUBLIC openmpt)
