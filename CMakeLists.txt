cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

project(quake-level-viewer LANGUAGES C CXX)

add_subdirectory(thirdparty/raylib)

add_executable(quake-level-viewer raymath-extras.h quake-level-viewer.cpp)
target_link_libraries(quake-level-viewer raylib)
target_compile_definitions(quake-level-viewer PRIVATE
	MAP_SOURCE_DIR="${CMAKE_SOURCE_DIR}/quake_map_source"
	QUAD_VS_PATH="${CMAKE_SOURCE_DIR}/quad.vs"
	QUAD_FS_PATH="${CMAKE_SOURCE_DIR}/quad.fs"
)
