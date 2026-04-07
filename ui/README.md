# cairo-win32-ui

A lightweight, dark-mode UI framework for Win32 applications using Cairo for rendering.

No Qt. No wxWidgets. No MFC. Just Cairo, Win32, and clean C.

![Demo Screenshot](docs/screenshot.png)

---

## Why?

Cairo is a powerful 2D vector graphics library — but almost all documentation and examples target Linux/GTK. Getting Cairo to work well on Win32 is surprisingly undocumented.

This framework fills that gap: a set of self-contained single-header libraries that give you a professional dark-mode UI on Windows, built entirely on Cairo + Win32.

Born from a real-world BIM/GIS topological editor project, every component was forged in the heat of actual use.

---

## Components

| Header | Description |
|--------|-------------|
| `cairo_layout.h` | Toolbar, combo layer selector, statusbar, menubar, splitter, transcript pane |
| `cairo_menu.h` | Autonomous popup context menu with submenus |
| `cairo_symbol_picker.h` | SVG symbol picker dialog (requires librsvg) |
| `cairo_style_editor.h` | Visual style editor dialog (colors, line width, dash) |
| `cairo_input_dialog.h` | Dark-mode text input dialog |
| `cairo_transcript.h` | Interactive console/log pane with scrolling and command input |

All headers are **single-file**, **self-contained**, and follow the `#define XXX_IMPLEMENTATION` pattern.

---

## Features

- 🎨 **Full Cairo rendering** — vector graphics, anti-aliasing, PDF export ready
- 🌑 **Dark mode** — consistent dark theme throughout, DWM integration
- 🪟 **Pure Win32** — no dependencies beyond Cairo and GDI
- 📦 **Single-header** — drop a `.h` file in your project and go
- 🔧 **Composable** — use one component or all of them
- ⚡ **Backstore pattern** — efficient rendering with cache invalidation
- 🖱️ **Tooltips** — built-in 600ms delay tooltip system
- 🔀 **Split buttons** — toolbar buttons with dropdown arrow
- 📊 **Progress bar** — in-statusbar progress indicator
- 🗂️ **Layer combo** — toolbar combo with eye toggle per layer
- 📋 **Transcript pane** — Apollo-style command log with double-click replay

---

## Dependencies

| Library | Purpose | Install (MSYS2) |
|---------|---------|-----------------|
| Cairo | Core rendering | `pacman -S mingw-w64-x86_64-cairo` |
| librsvg | SVG symbols (optional) | `pacman -S mingw-w64-x86_64-librsvg` |
| GDI32 | Win32 surfaces | Included with Windows SDK |

**Compiler:** GCC/MinGW-w64 via [MSYS2](https://www.msys2.org/)

---

## Quick Start

### Minimal toolbar + canvas

```c
#define CAIRO_LAYOUT_IMPLEMENTATION
#include "cairo_layout.h"

// In WM_CREATE:
CairoLayout *layout = cl_create(hwnd, hwnd_canvas);

cl_toolbar_add_icon(layout, ID_SELECT, icon_select, "Select", CL_BTN_TOGGLE);
cl_toolbar_add_icon(layout, ID_ZOOM,   icon_zoom,   "Zoom",   CL_BTN_SPLIT);
cl_toolbar_add_sep(layout);
cl_toolbar_add_combo(layout, ID_LAYERS, 180);
cl_combo_add_layer(layout, ID_LAYERS, "Layer 1", 1);

cl_status_add_pane(layout, CL_PANE_SPRING, 0);
cl_status_add_pane(layout, CL_PANE_FIXED, 200);

cl_set_toolbar_cb(layout, on_toolbar, NULL);

// In WM_SIZE:
cl_resize(layout);

// In WM_DESTROY:
cl_destroy(layout);
```

### Context menu

```c
#define CAIRO_MENU_IMPLEMENTATION
#include "cairo_menu.h"

CairoMenu *menu = cm_create(hwnd, on_menu_item, NULL);
cm_add_item(menu, ID_NEW,    "New",      "Ctrl+N", CM_FLAG_NONE);
cm_add_item(menu, ID_OPEN,   "Open",     "Ctrl+O", CM_FLAG_NONE);
cm_add_sep(menu);
int sub = cm_add_submenu(menu, "View");
cm_sub_add_item(menu, sub, ID_ZOOM_IN,  "Zoom In",  "Ctrl++", CM_FLAG_NONE);
cm_sub_add_item(menu, sub, ID_ZOOM_OUT, "Zoom Out", "Ctrl+-", CM_FLAG_NONE);

// On right-click:
case WM_RBUTTONDOWN: {
    POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
    ClientToScreen(hwnd, &pt);
    cm_show(menu, pt.x, pt.y);
    break;
}
```

### Transcript pane with splitter

```c
#define CAIRO_TRANSCRIPT_IMPLEMENTATION
#include "cairo_transcript.h"

#define CAIRO_LAYOUT_IMPLEMENTATION
#include "cairo_layout.h"

// Create transcript
CairoTranscript *ct = ct_create(hwnd, 0, 0, 1, 1);
ct_set_cb(ct, on_command, NULL);

// Attach to layout with splitter
cl_attach_transcript(layout, ct, 220);
cl_show_transcript(layout);   // or cl_toggle_transcript(layout)

// Add log entries
ct_add(ct, CT_CMD,  "SELECT * FROM nodes;");
ct_add(ct, CT_OK,   "3 results found");
ct_add(ct, CT_ERR,  "File not found: data.db");
ct_add(ct, CT_SEP,  "Query Results");
ct_add(ct, CT_INFO, "Loading layer...");
ct_add(ct, CT_WARN, "Index missing on column x");
```

### Drawing icons for toolbar buttons

Icons are plain Cairo functions — draw anything you want:

```c
void icon_zoom_plus(cairo_t *cr, double cx, double cy)
{
    // Magnifier glass
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.8);
    cairo_arc(cr, cx-2, cy-2, 6, 0, 2*M_PI);
    cairo_stroke(cr);
    // Handle
    cairo_move_to(cr, cx+3, cy+3);
    cairo_line_to(cr, cx+8, cy+8);
    cairo_stroke(cr);
    // Plus sign
    cairo_set_source_rgb(cr, 0.3, 0.8, 0.3);
    cairo_move_to(cr, cx-2, cy-5); cairo_line_to(cr, cx-2, cy+1);
    cairo_move_to(cr, cx-5, cy-2); cairo_line_to(cr, cx+1, cy-2);
    cairo_stroke(cr);
}

// Register with toolbar
cl_toolbar_add_icon(layout, ID_ZOOM_IN, icon_zoom_plus, "Zoom In", CL_BTN_NORMAL);
```

Or use pre-compiled SVG handles (requires librsvg):

```c
RsvgHandle *rsvg = rsvg_handle_new_from_file("my_icon.svg", NULL);
cl_toolbar_add_svg(layout, ID_MY_BTN, rsvg, "My Button", CL_BTN_SPLIT);
// g_object_unref(rsvg) when done
```

---

## cairo_layout.h — Full API Reference

### Lifecycle
```c
CairoLayout *cl_create (HWND parent, HWND canvas);
void         cl_destroy(CairoLayout *l);
void         cl_resize (CairoLayout *l);   // call on WM_SIZE
```

### Toolbar — Buttons
```c
// Text button
int  cl_toolbar_add_btn (l, id, "Label", CL_BTN_NORMAL);
int  cl_toolbar_add_btn (l, id, "Label", CL_BTN_TOGGLE);
int  cl_toolbar_add_btn (l, id, "Label", CL_BTN_SPLIT);

// Cairo icon button
int  cl_toolbar_add_icon(l, id, my_icon_fn, "Tooltip", CL_BTN_NORMAL);

// SVG icon button (librsvg)
int  cl_toolbar_add_svg (l, id, rsvg_handle, "Tooltip", CL_BTN_SPLIT);

// Separator
void cl_toolbar_add_sep (l);

// State
void cl_toolbar_set_pressed(l, id, 1);    // toggle state
void cl_toolbar_set_enabled(l, id, 0);    // gray out

// Position (for positioning popups under a button)
int  cl_toolbar_get_btn_screen_rect(l, id, &rect);
```

### Toolbar — Layer Combo
```c
int  cl_toolbar_add_combo  (l, id, width_px);
void cl_combo_clear        (l, id);
int  cl_combo_add_layer    (l, id, "Layer name", is_visible);
void cl_combo_set_layer_data(l, id, idx, db_id, geom_type, style_id);
void cl_combo_set_current  (l, id, idx);
int  cl_combo_get_current  (l, id);
int  cl_combo_get_count    (l, id);
const char *cl_combo_get_name    (l, id, idx);
int         cl_combo_get_visible (l, id, idx);
int         cl_combo_get_db_id   (l, id, idx);
int         cl_combo_get_geom_type(l, id, idx);
int         cl_combo_get_style_id (l, id, idx);
```

### Toolbar — Callback
```c
void cl_set_toolbar_cb(l, on_toolbar, userdata);

void on_toolbar(int id, void *userdata) {
    // Split button arrow click
    if (id == CL_ID_SPLIT_BASE + ID_MY_BTN) { /* show dropdown */ }
    // Eye toggle on layer
    if (id >= CL_ID_EYE_BASE) {
        int layer = id - CL_ID_EYE_BASE;
        int vis = cl_combo_get_visible(l, ID_COMBO, layer);
        // toggle visibility...
    }
    switch (id) {
    case ID_MY_BTN: /* handle click */ break;
    case ID_COMBO:  /* layer changed */ break;
    }
}
```

### Menubar
```c
int  cl_menubar_add    (l, id, "File");
void cl_set_menubar_cb (l, on_menu, userdata);
int  cl_menubar_get_item_screen_rect(l, id, &rect);
void cl_menubar_close  (l);
```

### Statusbar
```c
int  cl_status_add_pane    (l, CL_PANE_FIXED,  120);
int  cl_status_add_pane    (l, CL_PANE_SPRING, 0);
void cl_status_set         (l, pane_idx, "Ready");
void cl_status_set_progress(l, pane_idx, 0.75f);   // 0.0–1.0
void cl_status_clear       (l, pane_idx);
```

### Transcript + Splitter
```c
void cl_attach_transcript(l, ct, init_height_px);
void cl_show_transcript  (l);
void cl_hide_transcript  (l);
void cl_toggle_transcript(l);
```

---

## Key Win32/Cairo Integration Notes

These are the hard-won lessons from building this framework:

### 1. Always flush before BitBlt
```c
canvas_render_viewport(...);
cairo_surface_flush(surface);   // REQUIRED before BitBlt
BitBlt(hdcCache, 0, 0, w, h, hdcMem, 0, 0, SRCCOPY);
```
Cairo buffers drawing operations. Without `cairo_surface_flush`, BitBlt copies an empty HDC.

### 2. Never use GWLP_ID on WS_POPUP windows
```c
// WRONG — GWLP_ID behaves differently on WS_POPUP
SetWindowLongPtr(hwnd, GWLP_ID, (LONG_PTR)my_ptr);

// RIGHT — use SetProp/GetProp for second pointer
SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)first_ptr);
SetProp(hwnd, "my_data", (HANDLE)second_ptr);
```

### 3. Dark mode on WS_POPUP — call after ShowWindow
```c
ShowWindow(hwnd, SW_SHOW);
BOOL dark = TRUE;
DwmSetWindowAttribute(hwnd, 20, &dark, sizeof dark);
```

### 4. SVG with viewBox only — use get_intrinsic_dimensions
```c
// WRONG for SVG with viewBox but no width/height attributes
rsvg_handle_get_intrinsic_size_in_pixels(rsvg, &w, &h);

// RIGHT
RsvgRectangle vb = {0};
gboolean has_vb = FALSE;
rsvg_handle_get_intrinsic_dimensions(rsvg, NULL, NULL, NULL, NULL, &has_vb, &vb);
if (has_vb) { w = vb.width; h = vb.height; }
```

### 5. DestroyWindow order in dropdown — destroy last
```c
// Update state FIRST, then destroy
c->current = i;
c->open = 0;
c->hwnd_drop = NULL;
InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
UpdateWindow(l->hwnd_toolbar);
if (l->tb_cb) l->tb_cb(c->id, l->tb_ud);
DestroyWindow(hwnd);   // LAST — c and l still valid above
```

---

## Makefile (MSYS2 / MinGW-w64)

```makefile
CC      = gcc
TARGET  = demo.exe
SRCS    = demo.c
CFLAGS  = -std=c11 -Wall \
          $(shell pkg-config --cflags cairo librsvg-2.0 glib-2.0)
LDFLAGS = -lm -lgdi32 -luser32 -lcomctl32 -lcomdlg32 -ldwmapi -mwindows \
          $(shell pkg-config --libs cairo librsvg-2.0 glib-2.0)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
```

---

## Project Structure

```
cairo-win32-ui/
├── cairo_layout.h          # Main layout framework
├── cairo_menu.h            # Popup context menu
├── cairo_transcript.h      # Console/log pane
├── cairo_symbol_picker.h   # SVG symbol picker
├── cairo_style_editor.h    # Style editor dialog
├── cairo_input_dialog.h    # Text input dialog
├── demo/
│   ├── demo.c              # Full demonstration
│   └── Makefile
├── docs/
│   └── screenshot.png
└── README.md
```

---

## License

MIT License — use freely, attribution appreciated.

---

## Background

This framework was developed as the UI layer of a BIM/GIS topological editor
for urban infrastructure networks, running on cadastral data from Brussels (UrbIS).

If you're building any serious Win32 application with Cairo — maps, CAD tools,
scientific visualization, custom UI — this framework gives you a solid foundation
without the weight of a full toolkit.

---

*Built with Cairo, Win32, SQLite, and a healthy respect for the Win32 message loop.*
