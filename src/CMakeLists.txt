set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags for Release build type
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -mtune=native -ffast-math -fomit-frame-pointer")
endif()

set(SOURCES
    const.hpp

    window.hpp
    window.cpp

    material_manager.hpp
    material_manager.cpp

    set_manager.hpp
    set_manager.cpp

    save_manager.hpp
    save_manager.cpp

    grid.hpp
    grid.cpp

    ui.hpp
    ui.cpp

    main.cpp
)

find_package(Threads REQUIRED)

if (WIN32)
    add_executable(sand3 ${SOURCES} "sand3.rc")
else()
    add_executable(sand3 ${SOURCES})
endif()

target_link_libraries(sand3 PRIVATE fmt::fmt SDL3::SDL3-static Threads::Threads imgui nlohmann_json::nlohmann_json)

if(MINGW)
    target_link_options(sand3 PRIVATE -static)
endif()

set_property(TARGET sand3 PROPERTY CXX_STANDARD 20)