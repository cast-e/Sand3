# Sand3 Configuration Reference

This guide documents the configuration system in Sand3, including `config.ini` (global application configuration) and `set_config.ini` (per-material-set configuration).

---

## Table of Contents
1. [Delta-Only Saving](#delta-only-saving)
2. [Global Configuration: `config.ini`](#global-configuration-configini)
   - [[Window] Settings](#window-settings)
   - [[Advanced] Settings](#advanced-settings)
   - [[UI] Settings](#ui-settings)
   - [[Shortcuts] Keyboard Customization](#shortcuts-keyboard-customization)
   - [[Colors] Theme Colors](#colors-theme-colors)
3. [Per-Set Configuration: `set_config.ini`](#per-set-configuration-set_configini)
   - [File Location](#set-file-location)
   - [Supported Properties](#supported-properties)
4. [Example Configurations](#example-configurations)

---

## Delta-Only Saving

Sand3 uses a **delta-only serialization** approach for all configuration files:

- **Unchanged defaults are omitted**: If a setting retains its default value, it is **never written** to `config.ini` or `set_config.ini`.
- **Minimal file size**: Instead of dumping hundreds of default values (such as ~55 theme color tokens or 33 shortcut entries), the file contains only your custom overrides.
- **Clean defaults evolution**: Future updates that adjust default values will automatically apply to your client unless you have explicitly overridden that specific setting.
- **Omitted sections**: If an entire section contains no overrides, its section header (e.g. `[Colors]`) will not be written to disk.
- **Restoring defaults**: Resetting a shortcut or color in the Settings UI (or deleting the corresponding line from the `.ini` file) will automatically revert it to default and remove it from the file on the next save.

---

## Global Configuration: `config.ini`

### [Window] Settings

Controls the initial placement and display state of the application window.

| Key | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `x` | Integer | `-1` | Window X coordinate on screen in pixels (`-1` = auto-centered by OS/SDL). |
| `y` | Integer | `-1` | Window Y coordinate on screen in pixels (`-1` = auto-centered by OS/SDL). |
| `width` | Integer | `1600` | Window width in pixels. |
| `height` | Integer | `900` | Window height in pixels. |
| `maximized` | Boolean (`0`/`1`, `false`/`true`) | `false` | Whether the window should start maximized. |
| `fullscreen` | Boolean (`0`/`1`, `false`/`true`) | `false` | Whether the window should start in borderless fullscreen mode. |

---

### [Advanced] Settings

Controls simulation execution, multi-threading, frame rate targets, and synchronization.

| Key | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `vsync` | Boolean (`0`/`1`, `false`/`true`) | `false` | Enable or disable vertical synchronization with the display. |
| `target_fps` | Integer | `500` | Target simulation and rendering frame rate cap (FPS). Set higher for faster simulation. |
| `processing_mode` | Integer (`0` or `1`) | `0` | Simulation backend:<br>• `0`: CPU Multithreaded (SIMD/threaded)<br>• `1`: GPU Compute (Vulkan Compute Shader) |
| `thread_count` | Integer | Auto | Number of worker threads utilized when running in CPU mode. Defaults to half the vertical chunk strips or CPU hardware concurrency. |
| `prevent_downclock`| Boolean (`0`/`1`, `false`/`true`) | `true` | Runs a continuous background heartbeat loop during low-load frames to prevent modern GPUs from downclocking to power-saving clocks, preventing frame pacing stutters. |

---

### [UI] Settings

Controls layout, scaling, typography, and styling of the Dear ImGui interface.

| Key | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `sidebar_x` | Integer | `0` | Horizontal pixel offset of the main sidebar (`0` = docked flush to left edge). |
| `sidebar_y` | Integer | `0` | Vertical pixel offset of the main sidebar (`0` = docked flush to top edge). |
| `sidebar_width` | Integer | `380` | Width of the main sidebar panel in pixels (adjustable via draggable bar). |
| `material_list_height` | Integer | `150` | Height of the materials list in the Material Editor (adjustable via draggable bar). |
| `compact_x` | Integer | `2` | Horizontal pixel offset of the compact UI overlay. |
| `compact_y` | Integer | `2` | Vertical pixel offset of the compact UI overlay. |
| `compact_width` | Integer | `280` | Width of the compact UI overlay in pixels. |
| `compact_height`| Integer | `400` | Height of the compact UI overlay in pixels. |
| `font_size` | Float | `16.0` | Base font size in pixels. |
| `ui_scale` | Float | `1.0` | Global UI scaling factor. |
| `show_fps` | Boolean (`0`/`1`, `false`/`true`) | `true` | Display real-time FPS counter in the header. |
| `show_active_cells`| Boolean (`0`/`1`, `false`/`true`)| `true` | Display active simulated cells count in the header. |
| `show_simulation_status`| Boolean (`0`/`1`, `false`/`true`)| `true` | Display PAUSED or UNPAUSED status indicator in the header. |
| `window_rounding` | Float | `6.0` | Corner rounding radius for windows. |
| `frame_rounding` | Float | `4.0` | Corner rounding radius for buttons and input fields. |
| `button_size` | Integer | `32` | Default button size / height in pixels. |
| `icon_size` | Integer | `16` | Preferred icon size in pixels inside buttons. |
| `background_color` | Color Hex (`#RRGGBB` / `#RRGGBBAA`) | `#000000FF` | Color of the canvas viewport outside of the simulation grid. |

---

### [Shortcuts] Keyboard Customization

All keyboard shortcuts can be customized either directly in `config.ini` under `[Shortcuts]` or through the in-game UI.

#### Key Combination Syntax
Key combinations can include zero or more modifiers separated by `+`, followed by a key name:
- **Modifiers**: `Ctrl+`, `Shift+`, `Alt+` (case-insensitive)
- **Standard Keys**: Letters (`A`-`Z`), digits (`0`-`9`)
- **Special Keys**: `Space`, `Escape` (or `Esc`), `Enter` (or `Return`), `Delete` (or `Del`), `Backspace`, `Tab`, `PageUp`, `PageDown`, `Home`, `End`, `Insert`
- **Directional Keys**: `Up`, `Down`, `Left`, `Right`
- **Function Keys**: `F1` through `F12`
- **Punctuation & Math**: `=`, `-`, `+`, `[`, `]`, `\`, `;`, `'`, `,`, `.`, `/`, `` ` ``
- **Keypad**: `Keypad +`, `Keypad -`, etc.

#### Complete List of Shortcut Action Identifiers

| Action Key | Default Binding | Category | Description |
| :--- | :--- | :--- | :--- |
| `toggle_simulation` | `Space` | Simulation | Toggle simulation running / paused |
| `step_frame` | `F` | Simulation | Step simulation by 1 frame when paused |
| `clear_grid` | `R` | Simulation | Clear / reset the entire simulation grid |
| `camera_up` | `W` | Camera | Pan camera up (Hold `Shift` for 2.5x speed) |
| `camera_left` | `A` | Camera | Pan camera left (Hold `Shift` for 2.5x speed) |
| `camera_down` | `S` | Camera | Pan camera down (Hold `Shift` for 2.5x speed) |
| `camera_right` | `D` | Camera | Pan camera right (Hold `Shift` for 2.5x speed) |
| `zoom_in` | `PageUp` | Camera | Zoom camera in |
| `zoom_out` | `PageDown` | Camera | Zoom camera out |
| `toggle_compact` | `V` | General | Toggle compact UI overlay |
| `undo` | `Ctrl+Z` | General | Undo last canvas / selection action |
| `redo` | `Ctrl+Y` | General | Redo last action (also accepts `Ctrl+Shift+Z`) |
| `fullscreen` | `F11` | General | Toggle fullscreen mode |
| `cancel_or_quit` | `Escape` | General | Cancel selection/move/paste, or quit |
| `tool_brush` | `B` | Tools & Selection | Switch to Brush tool |
| `tool_select` | `C` | Tools & Selection | Switch to Selection tool |
| `copy` | `Ctrl+C` | Tools & Selection | Copy selected area to clipboard |
| `cut` | `Ctrl+X` | Tools & Selection | Cut selected area to clipboard |
| `paste` | `Ctrl+V` | Tools & Selection | Paste clipboard at cursor position |
| `fill` | `Ctrl+F` | Tools & Selection | Fill selected box with active material |
| `delete` | `Delete` | Tools & Selection | Delete cells inside selected box |
| `rotate_cw` | `Q` | Tools & Selection | Rotate active selection 90° clockwise |
| `rotate_ccw` | `E` | Tools & Selection | Rotate active selection 90° counter-clockwise |
| `brush_shape` | `T` | Brush | Cycle brush shape (Square / Circle) |

#### Canvas & Mouse Shortcuts

These controls operate directly on the simulation canvas and viewport:

| Control | Action | Description |
| :--- | :--- | :--- |
| `Left Mouse Button` | Paint / Select | Draw with active material (Brush) / Create or move selection box (Select tool) |
| `Right Mouse Button` | Erase / Deselect | Clear cells to empty material / Deselect active selection |
| `Middle Mouse Drag` | Pan Camera | Drag to pan the camera viewport across the simulation grid |
| `Middle Mouse Click` | Eyedropper | Sample and select the material under the mouse cursor |
| `Mouse Scroll Wheel` | Brush Size | Increment or decrement the brush tool radius |
| `Ctrl + Scroll Wheel`| Fast Brush Size | Adjust brush size rapidly (4x step rate) |
| `Shift + Scroll Wheel`| Zoom Canvas | Smoothly zoom viewport in and out centered at mouse |
| `Shift + Left/Right Drag` | Line Tool | Draw straight continuous lines between click and release points |
| `Shift + Alt + Click` | Flood Fill | Flood fill or clear contiguous connected cells of the same material |
| `Shift + W/A/S/D` | Fast Camera Pan | Pan camera across canvas at 2.5x speed |
| `Arrow Keys` | Nudge Selection | Nudge active selection box by 1 cell (Hold `Shift` for 10 cells) |
| `Selection Handles` | Resize Selection | Drag border handles to resize active selection bounding box |
| `Ctrl + Shift + Z` | Redo Alternate | Secondary shortcut to redo last undone action |
| `Ctrl + Shift + Delete`| Clear Alternate | Secondary shortcut to clear/reset the simulation grid |
| `Keypad +` / `Keypad -`| Zoom Alternate | Secondary shortcuts to zoom canvas camera |

---

### [Colors] Theme Colors

All UI colors can be customized interactively in the UI or manually via `[Colors]` in `config.ini`.

#### Color Value Formats
Colors can be written in either `#RRGGBB` or `#RRGGBBAA` hex notation (e.g. `#1A1A1AFF` or `#FF5500`).

#### Active Curated Color Tokens

| Color Token | Category | Description |
| :--- | :--- | :--- |
| `Text` | Text & Selection | Primary text color across all windows and widgets |
| `TextDisabled` | Text & Selection | Disabled, placeholder, and dimmed text |
| `TextSelectedBg` | Text & Selection | Background highlight for selected text in input fields |
| `WindowBg` | Windows & Backgrounds | Main background for sidebar, editor, and floating windows |
| `ChildBg` | Windows & Backgrounds | Background for scrollable child frames and list panels |
| `PopupBg` | Windows & Backgrounds | Background for popups, modals, dropdown menus, and tooltips |
| `Border` | Windows & Backgrounds | Outer border line color for windows and frames |
| `MenuBarBg` | Windows & Backgrounds | Menu bar background |
| `Button` | Buttons & Accent | Normal button background |
| `ButtonHovered` | Buttons & Accent | Button background when hovered by cursor |
| `ButtonActive` | Buttons & Accent | Button background when pressed / active |
| `Header` | Buttons & Accent | Background for active headers, list items, and selected materials |
| `HeaderHovered` | Buttons & Accent | Collapsing headers and selectable items when hovered |
| `HeaderActive` | Buttons & Accent | Collapsing headers and selectable items when clicked |
| `FrameBg` | Inputs & Sliders | Background for text inputs, number fields, and sliders |
| `FrameBgHovered` | Inputs & Sliders | Input field background when hovered |
| `FrameBgActive` | Inputs & Sliders | Input field background when clicked / focused |
| `CheckMark` | Inputs & Sliders | Checkmark tick color inside checkboxes |
| `SliderGrab` | Inputs & Sliders | Knob / grabber handle on sliders |
| `SliderGrabActive` | Inputs & Sliders | Slider grabber handle when actively dragged |
| `Tab` | Tabs | Inactive tab background |
| `TabHovered` | Tabs | Tab background when hovered |
| `TabActive` | Tabs | Currently active / selected tab background |
| `TabUnfocused` | Tabs | Tab background when window is unfocused |
| `TabUnfocusedActive` | Tabs | Active tab background when window is unfocused |
| `Separator` | Dividers & Scrollbars | Subtle divider and separator lines |
| `ScrollbarBg` | Dividers & Scrollbars | Scrollbar track background |
| `ScrollbarGrab` | Dividers & Scrollbars | Scrollbar thumb handle color |
| `ScrollbarGrabHovered`| Dividers & Scrollbars | Scrollbar thumb when hovered |
| `ScrollbarGrabActive` | Dividers & Scrollbars | Scrollbar thumb when actively dragged |

---

## Per-Set Configuration: `set_config.ini`

### Set File Location
Each material set has its own folder inside `sets/<set_name>/` containing optional set metadata and custom material shortcuts:
```
sets/crystals/
├── set_config.ini
├── crystal_1.mat
├── crystal_2.mat
├── empty.mat
└── saves/
```

### Supported Metadata Properties

All properties in `set_config.ini` are optional:

| Property | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `author` | String | `""` | Name or handle of the author / set creator. |
| `description` | String | `""` | Description of the material set, simulation mechanics, or rules. |
| `width` | Integer | `0` | Recommended canvas width in cells (`0` = inherit global setting). |
| `height` | Integer | `0` | Recommended canvas height in cells (`0` = inherit global setting). |
| `target_fps` | Integer | `0` | Target simulation FPS for this set (`0` = inherit global setting). |
| `processing_mode` | Integer | `-1` | Recommended simulation mode (`-1` = inherit, `0` = CPU, `1` = GPU). |
| `prevent_downclock` | Boolean | `true` | Prevents GPU downclocking while simulating this set. |

### [Shortcuts] Section: Per-Material Shortcuts

You can assign custom keyboard shortcuts to any material in a set. These are saved in `set_config.ini` under `[Shortcuts]`:

```ini
[Shortcuts]
sand = 1
water = Shift+W
crystal_growing = 4
empty = 0
```

#### Shortcut Sequencing Rule
- If a material is assigned a custom shortcut with a single number (e.g. `4`), the materials that follow it will automatically follow in order (`5`, `6`, `7`...).
- Materials without custom shortcuts are automatically assigned sequential quick-select numbers (`1`, `2`, `3`...).
- The `empty` material is placed at the end of the order, so selecting it follows after all other non-empty materials.
- Custom shortcuts can use any valid key combination (e.g. `G`, `Shift+1`, `Ctrl+M`).

---

## Example Configurations

### Minimal `config.ini`
In regular usage, `config.ini` only contains lines you changed from defaults:

```ini
[Window]
width = 1920
height = 1080
maximized = true

[UI]
sidebar_width = 420
button_size = 36
icon_size = 18
show_simulation_status = true

[Advanced]
processing_mode = 1
target_fps = 120

[Shortcuts]
step_frame = Space
toggle_simulation = P

[Colors]
Button = #264E70FF
ButtonHovered = #3B7A8CFF
```

### Example `set_config.ini`
Inside `bin/sets/maze/set_config.ini`:

```ini
author=Samuel
description=Procedural labyrinth maze generation automaton

[Shortcuts]
maze_maker = G
rat_spawner = N
rat = O
static_maze = M
```
