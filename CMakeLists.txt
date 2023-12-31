cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_C_STANDARD 11)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")

project(quake-level-viewer LANGUAGES C CXX)

set(OPENGL_VERSION "4.3")
add_compile_definitions($<$<CONFIG:Debug>:RLGL_ENABLE_OPENGL_DEBUG_CONTEXT=1>)

add_subdirectory(thirdparty/raylib)
add_subdirectory(thirdparty/imgui)
add_subdirectory(thirdparty/rlImGui)

add_executable(quake-level-viewer main.cpp bsp.cpp)
target_link_libraries(quake-level-viewer raylib imgui rlImGui)

if (MSVC)
	target_compile_options(quake-level-viewer PUBLIC $<$<CONFIG:Debug>:/ZI>)
	target_link_options(quake-level-viewer PUBLIC $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>)
endif()

target_compile_definitions(quake-level-viewer PRIVATE
	MAP_SOURCE_DIR="${CMAKE_SOURCE_DIR}/maps"
	VS_PATH="${CMAKE_SOURCE_DIR}/lighting.vert"
	FS_PATH="${CMAKE_SOURCE_DIR}/lighting.frag")