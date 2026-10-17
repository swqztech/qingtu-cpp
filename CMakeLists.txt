cmake_minimum_required(VERSION 3.20)
project(qingtu-cpp VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
  add_compile_options(/utf-8)
endif()

include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.91.9
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_backend STATIC
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui_backend PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)
target_link_libraries(imgui_backend PUBLIC d3d11 dxgi)

add_executable(qingtu-cpp WIN32
  src/main.cpp
  src/app.cpp
  src/image_job.cpp
  src/platform_win.cpp
  src/dialogs.cpp
)
target_include_directories(qingtu-cpp PRIVATE src ${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb)
target_link_libraries(qingtu-cpp PRIVATE imgui_backend windowscodecs ole32 shell32 comdlg32)

if(MSVC)
  set_target_properties(qingtu-cpp PROPERTIES OUTPUT_NAME "qingtu-cpp")
endif()
