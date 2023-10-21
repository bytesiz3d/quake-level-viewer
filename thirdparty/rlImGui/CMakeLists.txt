cmake_minimum_required(VERSION 3.0)
add_library(rlImGui STATIC
	imgui_impl_raylib.h
	rlImGui.cpp
	rlImGui.h
	rlImGuiColors.h)

target_include_directories(rlImGui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(rlImGui raylib imgui)