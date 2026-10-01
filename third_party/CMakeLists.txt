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

add_subdirectory(fmt)
add_subdirectory(Vulkan-Headers EXCLUDE_FROM_ALL)
add_subdirectory(volk EXCLUDE_FROM_ALL)

# Configure libcurl to build statically as minimal HTTP/HTTPS client
set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
set(CURL_STATICLIB ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CURL_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(CURL_DISABLE_TESTS ON CACHE BOOL "" FORCE)
set(ENABLE_CURL_MANUAL OFF CACHE BOOL "" FORCE)
set(CURL_ENABLE_SSL ON CACHE BOOL "" FORCE)
if(WIN32)
    set(CURL_USE_SCHANNEL ON CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBPSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_LIBSSH2 OFF CACHE BOOL "" FORCE)
endif()

add_subdirectory(curl EXCLUDE_FROM_ALL)