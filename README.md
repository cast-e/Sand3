# Sand3
 Fast celullar automaton with fully customizable materials and rules.

<p align="center">
  <img src=./src/resources/sand3.png alt="Sand3" width="300"/>
</p>

<p align="center">
  <a href="https://sand3.vercel.app">
    <img src="https://img.shields.io/badge/Download-Sand3-ffa757?style=for-the-badge">
  </a>

  <a href="https://sand3.vercel.app/workshop">
    <img src="https://img.shields.io/badge/Open-Workshop-blue?style=for-the-badge">
  </a>
</p>

## Features

- Material editor for creating up to 255 materials with custom color and rules.
- 5x5 neighborhood rules for complex behaviours and patterns.
- Smooth zoom and panning controls for easy navigation.
- Hardware-accelerated GPU simulation using Vulkan compute shaders for high performance.
- Optimized multithreaded simulation for CPU processing.
- Multiplatform support (Windows, Linux).
- Step-by-step simulation for debugging rules.
- Helpful tooltips and shortcuts in the GUI.
- Standalone executable for easy portability.
- Compressed saves and stamps using a BWT + RLE compression algorithm.
- Inherit rules from other materials for easier material creation.
- Square and circle brushes with support for straight lines and flood fill.
- Simple undo/redo system to help creating complex saves.
- Fully customizable keyboard shortcuts and UI theme colors.

## Configuration

All keybinds, graphics settings, simulation backend, and UI theme colors are fully customizable.

See [CONFIG.md](CONFIG.md) for the complete reference of `config.ini` and `set_config.ini`'s settings and default values/bindings.

## Building

### Prerequisites

Install a C++20 compatible compiler (GCC recommended) and CMake.

Make sure to update all submodules:

```bash
git submodule update --init --recursive
```

## Using CMake

### Linux Building

#### Native compilation using GCC

To build as a Release (default):

```bash
cmake -B build --preset linux-gcc
cmake --build build
```

To build as a Debug:

```bash
cmake -B build --preset linux-gcc-debug
cmake --build build
```

#### Cross-compilation using MinGW-w64 (Linux -> Windows)

To build as a Release (default):

```bash
cmake -B build --preset windows-mingw
cmake --build build
```

To build as a Debug:

```bash
cmake -B build --preset windows-mingw-debug
cmake --build build
```

### Windows Building

Windows building is currently not supported.

## Running

### Linux

Run it from the "./bin/linux" folder.

```bash
cd ./bin/linux
./sand3
```

### Windows

Just run it from the "./bin/win64" folder.

```powershell
cd .\bin\w64
.\sand3.exe
```

## Dependencies

All major dependencies are included as a submodule in the "third-party/" directory.

- [fmt](https://github.com/fmtlib/fmt) - Fast formatting library.
- [nlohmann_json](https://github.com/nlohmann/json) - Modern JSON for C++.
- [SDL3](https://github.com/libsdl-org/SDL) - Simple DirectMedia Layer.
- [imgui](https://github.com/ocornut/imgui) - Dear ImGui.
- [volk](https://github.com/zeux/volk) - Meta-loader for Vulkan API.
- [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) - Vulkan API header files.
- [famfamfam-silk](https://github.com/legacy-icons/famfamfam-silk) - The Silk icon pack.

## Credits

- [Roboto](https://fonts.google.com/specimen/Roboto) for the font.
- [Miniz](https://github.com/richgel999/miniz) for the C .ZIP library.
