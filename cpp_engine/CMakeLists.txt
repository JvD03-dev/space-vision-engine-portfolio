cmake_minimum_required(VERSION 3.20)

if(NOT WIN32)
  message(FATAL_ERROR "cpp_engine currently supports Windows only (Win32 + OpenGL).")
endif()

add_executable(
  space_vision_engine
  src/main.cpp
  src/math.cpp
  src/config.cpp
  src/simulation.cpp
  src/camera.cpp
  src/ui.cpp
  src/renderer_gl_fixed.cpp
  src/win32_window.cpp
)
target_compile_features(space_vision_engine PRIVATE cxx_std_20)

target_include_directories(
  space_vision_engine
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party
)

if(MSVC)
  target_compile_options(space_vision_engine PRIVATE /W4 /permissive-)
else()
  target_compile_options(space_vision_engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

target_link_libraries(space_vision_engine PRIVATE opengl32 glu32 gdi32 user32 winmm gdiplus)
