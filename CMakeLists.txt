cmake_minimum_required(VERSION 3.20)
project(HunterAssassin LANGUAGES C CXX)

include(FetchContent)
FetchContent_Declare(raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG 5.5
    GIT_SHALLOW TRUE
)
set(BUILD_EXAMPLES OFF CACHE BOOL "Build raylib examples")
FetchContent_MakeAvailable(raylib)

add_executable(HunterAssassin
    src/main.cpp
    src/Game.cpp
    src/Player.cpp
    src/TileMap.cpp
)
set_target_properties(HunterAssassin PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)
target_link_libraries(HunterAssassin PRIVATE raylib)
if(WIN32)
    target_link_libraries(HunterAssassin PRIVATE opengl32 gdi32 winmm)
endif()

add_custom_target(copy_map
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:HunterAssassin>/maps"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/maps/test_map.txt"
        "$<TARGET_FILE_DIR:HunterAssassin>/maps/test_map.txt"
    VERBATIM
)
add_dependencies(HunterAssassin copy_map)
