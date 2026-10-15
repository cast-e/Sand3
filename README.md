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
- **Continuous Attributes & Attribute Equations**: Simulate heat, pressure, and charge with per-cell float attributes, visual heatmaps, dynamic color modifiers, and an intuitive badge-based equation editor with relative and absolute cell references.
- **Material Editor**: Create up to 255 materials with full RGB colors, rule inheritance, and custom reaction behaviors.
- **Integrated Community Workshop**: Browse, download, search, publish, and fork Sets, Saves, and Stamps with version history, ratings, and automatic set dependency resolution.
- **Selection & Transform Tools**: Rectangular selection to move, copy, cut, paste, resize, flip horizontally/vertically, and rotate with transparent pasting.
- **Creative Tool Suite**: Square and circle brushes with support for straight lines, flood fill, stamps, and full multi-level undo/redo history.
- **Compressed Storage**: Lossless BWT + RLE compression algorithm for compact `.save` and `.stamp` files.
- **Per-Set Customization**: Custom material shortcuts, recommended canvas dimensions, and metadata configured per set (`set_config.ini`).
- **Fully Customizable Controls & Themes**: Customize all keybindings, graphics options, UI scaling, and ~55 theme color tokens with delta-only `.ini` saving.
- **Step-by-Step Simulation**: Single-step execution for fine-grained rule inspection and debugging.
- **Multiplatform Standalone Binaries**: Clean standalone binaries for Linux and Windows (native GUI application).

## Configuration

All keybinds, graphics settings, canvas dimensions, simulation backend, and UI theme colors are fully customizable.

See [CONFIG.md](CONFIG.md) for the complete reference of `config.ini` and `set_config.ini`'s settings and default values/bindings.

## Building From Source

### Prerequisites

| Platform | Required Tools | Notes |
| :--- | :--- | :--- |
| **Common** | Git, CMake 3.25+ | Ensure CMake and Git are in your `PATH`. |
| **Linux** | GCC 12+ (or Clang 16+) with C++20 | `sudo apt install build-essential cmake ninja-build git` |
| **Windows** | MinGW-w64 (GCC 12+) and Ninja | Recommended: [MSYS2](https://www.msys2.org/) UCRT64 (`pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-ninja`). Add `C:\msys64\ucrt64\bin` to your system `PATH`. |

Clone the repository recursively with all submodules:

```bash
git clone --recursive https://github.com/cast-e/Sand3.git
cd Sand3
```

If you previously cloned without `--recursive`, fetch submodules manually:

```bash
git submodule update --init --recursive
```

---

### Linux

#### Native Compilation (GCC)

```bash
# Release build (recommended)
cmake --preset linux-gcc
cmake --build --preset linux-gcc

# Debug build
cmake --preset linux-gcc-debug
cmake --build --preset linux-gcc-debug
```

Output executable: `./bin/linux/sand3`
Build directory: `./build-linux` (contains `./build-linux/compile_commands.json`)

---

### Windows

#### Native Compilation (MinGW-w64 + Ninja)

Open PowerShell or Command Prompt with MSYS2/MinGW in your `PATH`:

```powershell
# Release build (recommended)
cmake --preset windows-mingw
cmake --build --preset windows-mingw

# Debug build
cmake --preset windows-mingw-debug
cmake --build --preset windows-mingw-debug
```

Output executable: `.\bin\win64\sand3.exe` (standalone executable).
Build directory: `.\build-win64` (contains `.\build-win64\compile_commands.json`)

#### Cross-compilation from Linux

You can also cross-compile Windows binaries from Linux using `x86_64-w64-mingw32-gcc`:

```bash
cmake --preset windows-mingw
cmake --build --preset windows-mingw
```

---

### IDE & Language Server Support (clangd / VS Code)

Linux and Windows builds cleanly coexist in their respective directories (`build-linux` and `build-win64`). VS Code and the clangd language server automatically detect the active `compile_commands.json` based on the selected CMake preset via `${command:cmake.buildDirectory}` without requiring symlinks or root copies.

---

## Running From Source

- **Linux**:
  ```bash
  cd ./bin/linux
  ./sand3
  ```

- **Windows**:
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
