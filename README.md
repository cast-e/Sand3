# Sand3

Fast cellular automaton with fully customizable materials and rules.

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

- **Customizable Simulation Grid Size**: Dynamic canvas sizing from 128x128 up to 4096x4096 (over 16M cells) with quick presets (256x256, 512x512, 1024x1024, 1536x1536, 2048x2048) and content preservation (crop/pad).
- **Hardware-Accelerated GPU Simulation**: Ultra-fast simulation computed with Vulkan compute shaders for high FPS on massive canvases.
- **Optimized Multithreaded CPU Processing**: High-performance multi-threaded CPU backend when Vulkan GPU compute is not in use.
- **Material Editor**: Create up to 255 materials with full RGB colors, rule inheritance, and custom reaction behaviors.
- **Integrated Community Workshop**: Browse, download, search, publish, and fork Sets, Saves, and Stamps with version history, ratings, and automatic set dependency resolution.
- **Selection & Transform Tools**: Rectangular selection to move, copy, cut, paste, resize, flip horizontally/vertically, and rotate with transparent pasting.
- **Creative Tool Suite**: Square and circle brushes with support for straight lines, flood fill, stamps, and full multi-level undo/redo history.
- **Compressed Storage**: Lossless BWT + RLE compression algorithm for compact `.save` and `.stamp` files.
- **Per-Set Customization**: Custom material shortcuts, recommended canvas dimensions, and metadata configured per set (`set_config.ini`).
- **Fully Customizable Controls & Themes**: Customize all keybindings, graphics options, UI scaling, and ~55 theme color tokens with delta-only `.ini` saving.
- **Step-by-Step Simulation**: Single-step execution for fine-grained rule inspection and debugging.
- **Multiplatform Standalone Binaries**: Clean standalone binaries for Linux and Windows (native GUI application with no console window).

## Configuration

All keybinds, graphics settings, canvas dimensions, simulation backend, and UI theme colors are fully customizable.

See [CONFIG.md](CONFIG.md) for the complete reference of `config.ini` and `set_config.ini`'s settings and default values/bindings.

## Building

### Prerequisites

Install a C++20 compatible compiler (GCC, Clang, or MinGW-w64) and CMake 3.25+.

Make sure to clone or update all submodules:

```bash
git submodule update --init --recursive
```

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

The resulting executable is placed in `./bin/linux/sand3`.

### Windows Building

#### Cross-compilation from Linux using MinGW-w64

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

Run from the `./bin/linux` folder:

```bash
cd ./bin/linux
./sand3
```

### Windows

Run from the `./bin/win64` folder:

```powershell
cd .\bin\win64
.\sand3.exe
```

## Dependencies

All major dependencies are included as submodules or source files:

- [fmt](https://github.com/fmtlib/fmt) - Fast formatting library.
- [nlohmann_json](https://github.com/nlohmann/json) - Modern JSON for C++.
- [SDL3](https://github.com/libsdl-org/SDL) - Simple DirectMedia Layer.
- [imgui](https://github.com/ocornut/imgui) - Dear ImGui.
- [volk](https://github.com/zeux/volk) - Meta-loader for Vulkan API.
- [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) - Vulkan API header files.
- [curl](https://curl.se/libcurl/) - Workshop network client.
- [famfamfam-silk](https://github.com/legacy-icons/famfamfam-silk) - The Silk icon pack.

## Credits

- [Roboto](https://fonts.google.com/specimen/Roboto) for the font.
- [Miniz](https://github.com/richgel999/miniz) for the C .ZIP library.
