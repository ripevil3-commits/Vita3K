add_library(
    audio
    STATIC
    src/audio.cpp
    src/impl/sdl_audio.cpp)

target_include_directories(audio PUBLIC include)
target_link_libraries(audio PRIVATE util SDL3::SDL3)
if(NOT VITA3K_PS4)
    target_sources(audio PRIVATE src/impl/cubeb_audio.cpp)
    target_link_libraries(audio PRIVATE cubeb)
endif()
