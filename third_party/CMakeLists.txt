add_subdirectory(json)
add_subdirectory(SDL EXCLUDE_FROM_ALL)

add_library(imgui STATIC)
target_include_directories(imgui PUBLIC imgui imgui/backends)

target_sources(imgui PRIVATE
  imgui/imgui.h
  imgui/imgui.cpp

  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_tables.cpp

  imgui/backends/imgui_impl_sdl3.h
  imgui/backends/imgui_impl_sdl3.cpp
  imgui/backends/imgui_impl_sdlrenderer3.h
  imgui/backends/imgui_impl_sdlrenderer3.cpp
  )

target_link_libraries(imgui PUBLIC SDL3::SDL3)