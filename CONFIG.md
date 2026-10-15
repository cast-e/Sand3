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
   - [Mouse & Canvas Controls](#mouse--canvas-controls)
   - [[Colors] Theme Colors](#colors-theme-colors)
3. [Per-Set Configuration: `set_config.ini`](#per-set-configuration-set_configini)
   - [File Location](#set-file-location)
   - [Supported Properties](#supported-properties)
   - [Dynamic Physics Attributes](#dynamic-physics-attributes)
   - [Attribute Equations & Cell Badges](#attribute-equations--cell-badges)
   - [Material Attribute Color Modifiers](#material-attribute-color-modifiers)
   - [[Heatmap] Heatmap Gradient](#heatmap-section-set-wide-heatmap-gradient)
   - [[Shortcuts] Per-Material Shortcuts](#shortcuts-section-per-material-shortcuts)
4. [Example Configurations](#example-configurations)

---

## Delta-Only Saving

Sand3 uses a **delta-only serialization** approach for all configuration files:

- **Unchanged defaults are omitted**: If a setting retains its default value, it is **never written** to `config.ini` or `set_config.ini`.
- **Minimal file size**: Instead of dumping hundreds of default values (such as ~55 theme color tokens or 37 shortcut entries), the file contains only your custom overrides.
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

Controls simulation execution, multi-threading, frame rate targets, synchronization, and neighborhood safety limits.

| Key | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `vsync` | Boolean (`0`/`1`, `false`/`true`) | `false` | Enable or disable vertical synchronization with the display. |
| `target_fps` | Integer | `500` | Target simulation and rendering frame rate cap (FPS). Set to 500 for Unlimited. |
| `processing_mode` | Integer (`0` or `1`) | `0` | Simulation backend:<br>• `0`: CPU Multithreaded (SIMD/threaded)<br>• `1`: GPU Compute (Vulkan Compute Shader) |
| `thread_count` | Integer | Auto | Number of worker threads utilized when running in CPU mode. Defaults to half the vertical chunk strips or CPU hardware concurrency. |
| `prevent_downclock`| Boolean (`0`/`1`, `false`/`true`) | `true` | Runs a continuous background heartbeat loop during low-load frames to prevent modern GPUs from downclocking to power-saving clocks, preventing unpause frame drops. |
| `allow_large_neighborhoods` | Boolean (`0`/`1`, `false`/`true`) | `false` | Allows rule neighborhoods to exceed 100 cells without triggering confirmation warnings. |

---

### [UI] Settings

Controls layout, scaling, typography, and styling of the Dear ImGui interface.

| Key | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `sidebar_x` | Integer | `0` | Horizontal pixel offset of the main sidebar (`0` = docked flush to left edge). |
| `sidebar_y` | Integer | `0` | Vertical pixel offset of the main sidebar (`0` = docked flush to top edge). |
| `sidebar_width` | Integer | `380` | Width of the main sidebar panel in pixels (adjustable via draggable bar). |
| `material_list_height` | Integer | `150` | Height of the materials list adjustable via draggable bar. |
| `set_list_height` | Integer | `150` | Height of the sets list adjustable via draggable bar. |
| `save_list_height` | Integer | `180` | Height of the saves list adjustable via draggable bar. |
| `stamp_list_height` | Integer | `180` | Height of the stamps list adjustable via draggable bar. |
| `compact_x` | Integer | `2` | Horizontal pixel offset of the compact UI overlay. |
| `compact_y` | Integer | `2` | Vertical pixel offset of the compact UI overlay. |
| `compact_width` | Integer | `280` | Width of the compact UI overlay in pixels. |
| `compact_height`| Integer | `400` | Height of the compact UI overlay in pixels. |
| `font_size` | Float | `16.0` | Base font size in pixels. Requires application restart to reload font atlas. |
| `show_fps` | Boolean (`0`/`1`, `false`/`true`) | `true` | Display real-time FPS counter in the header. |
| `show_active_cells`| Boolean (`0`/`1`, `false`/`true`)| `true` | Display active simulated cells count in the header. |
| `show_simulation_status`| Boolean (`0`/`1`, `false`/`true`)| `true` | Display PAUSED or UNPAUSED status indicator in the header. |
| `base_spacing` | Float | `6.0` | Master base spacing in pixels. Changing this scales all derived UI spacings and paddings. |
| `window_padding_x` | Float | `12.0` | Horizontal window padding in pixels (Base + 6.0). |
| `window_padding_y` | Float | `12.0` | Vertical window padding in pixels (Base + 6.0). |
| `frame_padding_x` | Float | `8.0` | Horizontal frame/button padding in pixels (Base + 2.0). |
| `frame_padding_y` | Float | `5.0` | Vertical frame/button padding in pixels (Base - 1.0). |
| `item_spacing_x` | Float | `8.0` | Horizontal spacing between items/widgets in pixels (Base + 2.0). |
| `item_spacing_y` | Float | `6.0` | Vertical spacing between items/widgets in pixels (Base + 0.0). |
| `item_inner_spacing_x` | Float | `6.0` | Horizontal inner spacing between components of composite widgets (Base + 0.0). |
| `item_inner_spacing_y` | Float | `6.0` | Vertical inner spacing between components of composite widgets (Base + 0.0). |
| `cell_padding_x` | Float | `4.0` | Table cell horizontal padding in pixels (Base - 2.0). |
| `cell_padding_y` | Float | `2.0` | Table cell vertical padding in pixels (Base - 4.0). |
| `indent_spacing` | Float | `21.0` | Tree node / header indent spacing in pixels. |
| `scrollbar_size` | Float | `14.0` | Width/height of scrollbars in pixels. |
| `grab_min_size` | Float | `10.0` | Minimum size of slider/scrollbar grab handles in pixels. |
| `window_rounding` | Float | `6.0` | Corner rounding radius for windows. |
| `child_rounding` | Float | `4.0` | Corner rounding radius for child panels and workshop cards. |
| `frame_rounding` | Float | `4.0` | Corner rounding radius for buttons, toggle pills, and input fields. |
| `popup_rounding` | Float | `4.0` | Corner rounding radius for popup windows and tooltips. |
| `scrollbar_rounding`| Float | `4.0` | Corner rounding radius for scrollbars. |
| `grab_rounding` | Float | `4.0` | Corner rounding radius for slider grab handles. |
| `tab_rounding` | Float | `4.0` | Corner rounding radius for tabs. |
| `window_border_size`| Float | `1.0` | Border thickness for windows. |
| `child_border_size` | Float | `1.0` | Border thickness for child windows and cards. |
| `popup_border_size` | Float | `1.0` | Border thickness for popup windows. |
| `frame_border_size` | Float | `0.0` | Border thickness for frames and buttons. |
| `tab_border_size` | Float | `0.0` | Border thickness for tabs. |
| `button_size` | Integer | `32` | Default button size / height in pixels. |
| `icon_size` | Integer | `16` | Preferred icon size in pixels inside buttons and toggle pills. |
| `background_color` | Color Hex (`#RRGGBB` / `#RRGGBBAA`) | `#000000FF` | Color of the canvas viewport outside of the simulation grid. |
| `selection_box_color`| Color Hex | `#4B9CD3FF` | Border outline color for the selection box. |
| `selection_box_fill` | Color Hex | `#4B9CD333` | Translucent fill color inside the selection box. |

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
| `toggle_compact` | `G` | General | Toggle compact UI overlay |
| `cycle_display_mode` | `H` | General | Cycle display mode between normal material colors and attribute heatmaps |
| `undo` | `Ctrl+Z` | General | Undo last canvas / selection action |
| `redo` | `Ctrl+Y` | General | Redo last action (also accepts `Ctrl+Shift+Z`) |
| `fullscreen` | `F11` | General | Toggle fullscreen mode |
| `cancel_or_quit` | `Escape` | General | Cancel selection/move/paste, or quit |
| `tool_brush` | `B` | Tools & Selection | Switch to Drawing Brush tool |
| `tool_select` | `C` | Tools & Selection | Switch to Selection / Transform tool |
| `tool_attribute` | `V` | Tools & Selection | Switch to Attribute Brush tool (disabled if current set has no attributes) |
| `increase_attr_value` | `]` | Tools & Selection | Increase attribute value or delta on Attribute tool (Hold `Shift` for 5x step) |
| `decrease_attr_value` | `[` | Tools & Selection | Decrease attribute value or delta on Attribute tool (Hold `Shift` for 5x step) |
| `copy` | `Ctrl+C` | Tools & Selection | Copy selected area to clipboard |
| `cut` | `Ctrl+X` | Tools & Selection | Cut selected area to clipboard |
| `paste` | `Ctrl+V` | Tools & Selection | Paste clipboard at cursor position |
| `fill` | `Ctrl+F` | Tools & Selection | Fill selected box with active material |
| `delete` | `Delete` | Tools & Selection | Delete cells inside selected box |
| `rotate_cw` | `Q` | Tools & Selection | Rotate active selection 90° clockwise |
| `rotate_ccw` | `E` | Tools & Selection | Rotate active selection 90° counter-clockwise |
| `toggle_transparent` | `Alt+T` | Tools & Selection | Toggle selection transparency mode |
| `brush_shape` | `T` | Brush | Cycle brush shape (Square / Circle) |

---

### Mouse & Canvas Controls

These controls operate directly on the simulation canvas, viewport, and floating toolbar:

| Control | Action | Description |
| :--- | :--- | :--- |
| `Left Mouse Button` | Paint / Select / Apply Attribute | • **Brush**: Paint with active material<br>• **Select**: Create or drag selection box<br>• **Attribute Tool**: +Delta or Apply Target Value |
| `Right Mouse Button` | Erase / Deselect / Subtract Attribute | • **Brush**: Erase cells (draw empty material)<br>• **Select**: Deselect current selection<br>• **Attribute Tool**: -Delta or Reset to default value |
| `Middle Mouse Drag` | Pan Camera | Drag to pan the camera viewport across the grid |
| `Middle Mouse Click` | Eyedropper | Sample material from the cell under cursor |
| `Mouse Scroll Wheel` | Brush Size | Adjust simulation brush size / attribute brush diameter |
| `Ctrl + Scroll Wheel`| Fast Brush Size | Rapidly adjust brush size (4x step size) |
| `Shift + Scroll Wheel`| Zoom Viewport | Smoothly zoom canvas in or out centered at cursor |
| `Shift + Left / Right Drag` | Draw Straight Line | Draw straight lines with brush or apply attribute across a line |
| `Shift + Alt + Click` | Flood Fill | Flood fill connected region of matching cells |
| `Shift + W/A/S/D` | Fast Camera Pan | Pan camera across canvas at 2.5x speed |
| `Arrow Keys` | Nudge Selection | Nudge active selection box 1 cell (Hold `Shift` for 10 cells) |
| `Selection Handles` | Resize Selection | Drag border handles to resize selection box |
| `Center-Right Toolbar` | Quick Tool Switcher | Floating dock at center-right of screen for Brush, Select, Attribute Tool, and expandable settings drawer |

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
| `CheckMark` | Inputs & Sliders | Checkmark tick color inside checkboxes and toggle pills |
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
Each material set has its own folder inside `sets/<set_name>/` containing optional set metadata, attribute definitions, and custom material shortcuts:
```
sets/thermodynamics/
├── set_config.ini
├── metadata.json
├── hot_iron.mat
├── cold_water.mat
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

---

### Dynamic Physics Attributes

Sets can define up to **3 continuous dynamic physics attributes** (such as Temperature, Electrical Charge, Pressure, or Velocity). Cells maintain an ultra-compact **8-byte layout** (`sizeof(Cell) == 8`):

- **1 Attribute**: Full 32-bit float or 32-bit unsigned integer resolution (up to 4.29 billion or IEEE 754 float).
- **2 Attributes**: 32-bit attribute + 16-bit half-float attribute.
- **3 Attributes**: Three 16-bit half-float attributes (`float16_t`).
- **Real-Time Heatmaps**: Inspect continuous attributes visually across the canvas using customizable multi-stop color gradients. Configurable per-set in `set_config.ini` under `[Heatmap]` or adjusted on-the-fly via the toolbar heatmap popup. Toggle instantly with `H`.
- **Attribute Tool (`V`)**: Paint attribute values or deltas onto the grid. Adjust brush value with `[` and `]`.

---

### Attribute Equations & Cell Badges

Material rule actions (`THEN`) can execute custom mathematical expressions to update continuous attributes (such as calculating thermal conduction, pressure distribution, or electrical potential).

#### Interactive Equation System
- **Cell Picking**: When editing an attribute equation in the cell's `THEN` popup, simply click on any cell in either the **WHEN** or **THEN** grid. The cell's coordinate reference is immediately appended to the equation.
- **Uneditable Badges**: Cell references are displayed as clean, uneditable visual badges embedded directly inside the equation text field:
  - **Absolute Badges (`[dx, dy]`)**: By default, cell references are absolute. When copying a cell or using `Shift + Left Click` to paint rules across multiple cells, absolute cell references remain unchanged.
  - **Relative Badges (`~[dx, dy]`)**: Right-click any badge directly inside the box to toggle it into relative mode. Relative badges shift dynamically when the cell is copied (e.g. shifting by `(+1, 0)` shifts `~[0, 1]` to `~[1, 1]`).
  - **Remove Badge**: `Shift + Click` on any badge to remove it from the formula.
  - **In-Place Typing**: Click on the equation box to edit operators, numbers, and functions directly on your keyboard.
- **Operators & Functions**:
  - Arithmetic: `+`, `-`, `*`, `/`, `%`, `^`
  - Functions: `min(a, b)`, `max(a, b)`, `abs(x)`, `clamp(val, min, max)`, `round(x)`, `floor(x)`, `ceil(x)`
  - Symmetries: Automatic rule symmetries (`sym_x`, `sym_y`, rotations) symmetrically transform both absolute and relative coordinates.

---

### Material Attribute Color Modifiers

Materials can dynamically modify their display colors based on attribute values (e.g. glowing incandescent red as temperature rises, or turning frosty blue below freezing).

Each material supports up to **2 color modifier gradients** corresponding to physical attributes.

#### Color Modifier Properties
- **Interactive Multi-Stop Gradient**: Define multiple color stops positioned along the attribute's valid range (`min_val` to `max_val`).
- **Alpha Blending**: Each gradient stop includes an alpha channel (`rgba`), controlling how it composites over the material's base color:
  - `alpha = 0.0`: Completely transparent — displays the material's underlying base color unchanged.
  - `alpha = 1.0`: Completely opaque — replaces the material's base color with the gradient color.
  - `0.0 < alpha < 1.0`: Smoothly blends the gradient color over the material's base color.
- **Gradient Editor Controls**:
  - **Select Stop**: Left-click a stop handle to select it and edit its RGBA color.
  - **Move Stop**: Drag a stop handle horizontally. Stops have a drag delta threshold (4px) to prevent accidental movement when simply clicking to select.
  - **Add Stop**: Left-click an empty area on the gradient bar to create a new stop sampled from the current gradient.
  - **Remove Stop**: Right-click any stop handle to delete it (minimum 1 stop).

In material files (`.mat`, JSON format), color modifiers are stored under `"color_modifiers"`:
```json
"color_modifiers": [
    {
        "enabled": true,
        "gradient": [
            {
                "pos": 0.0,
                "color": [0.0, 0.4, 1.0, 1.0]
            },
            {
                "pos": 273.0,
                "color": [0.0, 0.2, 1.0, 0.0]
            },
            {
                "pos": 373.0,
                "color": [0.0, 0.0, 0.0, 0.0]
            },
            {
                "pos": 5000.0,
                "color": [1.0, 0.0, 0.0, 1.0]
            },
            {
                "pos": 10000.0,
                "color": [1.0, 1.0, 1.0, 1.0]
            }
        ]
    },
    {
        "enabled": false,
        "gradient": [
            { "pos": 0.0, "color": [0.0, 0.0, 0.0, 1.0] },
            { "pos": 1.0, "color": [1.0, 1.0, 1.0, 1.0] }
        ]
    }
]
```

---

### [Heatmap] Section: Set-Wide Heatmap Gradient

Material sets can configure a default multi-stop heatmap gradient for continuous attribute visualization. This is stored in `set_config.ini` under `[Heatmap]`:

```ini
[Heatmap]
stop_count = 5
stop0_pos = 0.0
stop0_color = 0,0,1,1
stop1_pos = 273.0
stop1_color = 0,0.8,1,1
stop2_pos = 373.0
stop2_color = 0,1,0,1
stop3_pos = 1500.0
stop3_color = 1,0.5,0,1
stop4_pos = 10000.0
stop4_color = 1,1,1,1
```

| Key | Type | Description |
| :--- | :--- | :--- |
| `stop_count` | Integer | Total number of gradient stops (up to 8 stops for GPU compute acceleration). |
| `stop<N>_pos` | Float | Position of stop `N` in attribute units (between attribute `min_val` and `max_val`). |
| `stop<N>_color` | Comma-separated Floats (`r,g,b,a`) | Normalized RGBA color values (`0.0` to `1.0`). |

---

### [Shortcuts] Section: Per-Material Shortcuts

You can assign custom keyboard shortcuts to any material in a set. These are saved in `set_config.ini` under `[Shortcuts]`:

```ini
[Shortcuts]
sand = 1
water = Shift+W
hot_iron = 4
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
allow_large_neighborhoods = true

[Shortcuts]
step_frame = Space
toggle_simulation = P
toggle_compact = G
tool_attribute = V
cycle_display_mode = H
increase_attr_value = ]
decrease_attr_value = [

[Colors]
Button = #264E70FF
ButtonHovered = #3B7A8CFF
```

### Example `set_config.ini`
Inside `sets/maze/set_config.ini`:

```ini
author=Samuel
description=Procedural labyrinth maze generation automaton

[Shortcuts]
maze_maker = G
rat_spawner = N
rat = O
static_maze = M
```
