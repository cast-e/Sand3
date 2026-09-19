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
| `sidebar_width` | Integer | `570` | Width of the main sidebar panel in pixels. |
| `compact_x` | Integer | `2` | Horizontal pixel offset of the compact UI overlay. |
| `compact_y` | Integer | `2` | Vertical pixel offset of the compact UI overlay. |
| `compact_width` | Integer | `280` | Width of the compact UI overlay in pixels. |
| `compact_height`| Integer | `400` | Height of the compact UI overlay in pixels. |
| `font_size` | Float | `16.0` | Base font size in pixels. |
| `ui_scale` | Float | `1.0` | Global UI scaling factor. |
| `show_fps` | Boolean (`0`/`1`, `false`/`true`) | `true` | Display real-time FPS counter in the header. |
| `show_active_cells`| Boolean (`0`/`1`, `false`/`true`)| `true` | Display active simulated cells count in the header. |
| `window_rounding` | Float | `6.0` | Corner rounding radius for windows. |
| `frame_rounding` | Float | `4.0` | Corner rounding radius for buttons and input fields. |
| `window_alpha` | Float | `0.95` | Window background opacity (`0.0` = fully transparent, `1.0` = fully opaque). |
| `button_height` | Integer | `30` | Default button height in pixels. |
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
| `sim_toggle` | `Space` | Simulation | Toggle simulation running / paused |
| `sim_step` | `F` | Simulation | Step simulation by 1 frame when paused |
| `sim_clear` | `R` | Simulation | Clear / reset the entire simulation grid |
| `cam_up` | `W` | Camera | Pan camera up (Hold `Shift` for 3x speed) |
| `cam_left` | `A` | Camera | Pan camera left (Hold `Shift` for 3x speed) |
| `cam_down` | `S` | Camera | Pan camera down (Hold `Shift` for 3x speed) |
| `cam_right` | `D` | Camera | Pan camera right (Hold `Shift` for 3x speed) |
| `cam_zoom_in` | `=` | Camera | Zoom camera in (also responds to keypad `+`) |
| `cam_zoom_out` | `-` | Camera | Zoom camera out (also responds to keypad `-`) |
| `quick_mat_1` | `1` | General | Quick-select material in slot 1 |
| `quick_mat_2` | `2` | General | Quick-select material in slot 2 |
| `quick_mat_3` | `3` | General | Quick-select material in slot 3 |
| `quick_mat_4` | `4` | General | Quick-select material in slot 4 |
| `quick_mat_5` | `5` | General | Quick-select material in slot 5 |
| `quick_mat_6` | `6` | General | Quick-select material in slot 6 |
| `quick_mat_7` | `7` | General | Quick-select material in slot 7 |
| `quick_mat_8` | `8` | General | Quick-select material in slot 8 |
| `quick_mat_9` | `9` | General | Quick-select material in slot 9 |
| `toggle_compact_ui` | `V` | General | Toggle compact UI overlay |
| `undo` | `Ctrl+Z` | General | Undo last canvas / selection action |
| `redo` | `Ctrl+Y` | General | Redo action (also accepts `Ctrl+Shift+Z`) |
| `toggle_fullscreen` | `F11` | General | Toggle fullscreen mode |
| `cancel_or_quit` | `Escape` | General | Cancel selection/move/paste, or quit |
| `tool_brush` | `B` | Tools & Selection | Switch to Brush tool |
| `tool_select` | `C` | Tools & Selection | Switch to Selection tool |
| `select_rotate_cw` | `Q` | Tools & Selection | Rotate active selection 90° clockwise |
| `select_rotate_ccw` | `E` | Tools & Selection | Rotate active selection 90° counter-clockwise |
| `select_copy` | `Ctrl+C` | Tools & Selection | Copy selected area to clipboard |
| `select_cut` | `Ctrl+X` | Tools & Selection | Cut selected area to clipboard |
| `select_paste` | `Ctrl+V` | Tools & Selection | Paste clipboard at cursor position |
| `select_fill` | `Ctrl+F` | Tools & Selection | Fill selected box with active material |
| `select_delete` | `Delete` | Tools & Selection | Delete cells inside selected box |
| `select_nudge_up` | `Up` | Tools & Selection | Nudge selection up 1 cell (Shift for 10x) |
| `select_nudge_down` | `Down` | Tools & Selection | Nudge selection down 1 cell (Shift for 10x) |
| `select_nudge_left` | `Left` | Tools & Selection | Nudge selection left 1 cell (Shift for 10x) |
| `select_nudge_right` | `Right` | Tools & Selection | Nudge selection right 1 cell (Shift for 10x) |
| `brush_shape` | `T` | Brush | Cycle brush shape (Square / Circle) |

---

### [Colors] Theme Colors

All UI colors can be customized interactively in the UI or manually via `[Colors]` in `config.ini`.

#### Color Value Formats
Colors can be written in either `#RRGGBB` or `#RRGGBBAA` hex notation (e.g. `#1A1A1AFF` or `#FF5500`).

#### Available Color Keys (Dear ImGui Tokens)

| Color Token | Description |
| :--- | :--- |
| `Text` | Primary text color |
| `TextDisabled` | Disabled / dimmed text color |
| `WindowBg` | Background color for main windows |
| `ChildBg` | Background color for child windows / scrolling regions |
| `PopupBg` | Background color for popups, menus, and tooltips |
| `Border` | Window and widget border line color |
| `BorderShadow` | Drop-shadow color under borders |
| `FrameBg` | Background for input fields, checkboxes, and sliders |
| `FrameBgHovered` | Frame background when hovered by mouse |
| `FrameBgActive` | Frame background when clicked / active |
| `TitleBg` | Title bar background (inactive) |
| `TitleBgActive` | Title bar background (active / focused) |
| `TitleBgCollapsed` | Title bar background when collapsed |
| `MenuBarBg` | Menu bar background |
| `ScrollbarBg` | Scrollbar track background |
| `ScrollbarGrab` | Scrollbar thumb / grabber color |
| `ScrollbarGrabHovered` | Scrollbar grabber when hovered |
| `ScrollbarGrabActive` | Scrollbar grabber when dragged |
| `CheckMark` | Checkbox tick mark color |
| `SliderGrab` | Slider grab handle color |
| `SliderGrabActive` | Slider grab handle when dragged |
| `Button` | Standard button background |
| `ButtonHovered` | Button background when hovered |
| `ButtonActive` | Button background when pressed |
| `Header` | Collapsing header / list item background |
| `HeaderHovered` | Collapsing header when hovered |
| `HeaderActive` | Collapsing header when clicked |
| `Separator` | Divider lines |
| `SeparatorHovered` | Divider lines when hovered |
| `SeparatorActive` | Divider lines when active |
| `ResizeGrip` | Window corner resize grip handle |
| `ResizeGripHovered` | Window resize grip when hovered |
| `ResizeGripActive` | Window resize grip when clicked |
| `TabHovered` | Tab item when hovered |
| `Tab` | Tab item (inactive) |
| `TabSelected` | Selected / active tab |
| `TabSelectedOverline` | Overline bar on selected tab |
| `TabDimmed` | Tab in background window |
| `TabDimmedSelected` | Selected tab in background window |
| `TabDimmedSelectedOverline`| Overline bar on background selected tab |
| `PlotLines` | Graph plot lines |
| `PlotLinesHovered` | Graph plot lines when hovered |
| `PlotHistogram` | Graph histogram bars |
| `PlotHistogramHovered` | Graph histogram bars when hovered |
| `TableHeaderBg` | Table column header background |
| `TableBorderStrong` | Primary outer borders of tables |
| `TableBorderLight` | Inner cell grid borders of tables |
| `TableRowBg` | Even table row background |
| `TableRowBgAlt` | Odd / alternating table row background |
| `TextLink` | Hyperlinks |
| `TextSelectedBg` | Text highlight / selection background |
| `DragDropTarget` | Highlight rectangle for drag-and-drop targets |
| `NavCursor` | Gamepad / keyboard navigation cursor highlight |
| `NavWindowingHighlight` | Window cycling overlay border |
| `NavWindowingDimBg` | Darkening background behind window cycling modal |

---

## Per-Set Configuration: `set_config.ini`

### Set File Location
Each material set has its own folder inside `sets/<set_name>/` containing a `set_config.ini`:
```
sets/enviroment/
├── set_config.ini
├── material_x.json
├── material_y.json
└── saves/
```

### Supported Properties

All properties in `set_config.ini` are optional.

| Property | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `author` | String | `""` | Name or handle of the author / set creator. |
| `description` | String | `""` | Description of the material set, simulation mechanics, or rules. |

---

## Example Configurations

### Minimal `config.ini`
In regular usage, `config.ini` only contains lines you changed from defaults:

```ini
[Window]
width=1920
height=1080
maximized=1

[Advanced]
processing_mode=1
target_fps=120

[Shortcuts]
sim_step=Space
sim_toggle=P

[Colors]
Button=#264E70FF
ButtonHovered=#3B7A8CFF
```

### Example `set_config.ini`
Inside `bin/sets/maze/set_config.ini`:

```ini
author=Samuel
description=Procedural labyrinth maze generation automaton
```
