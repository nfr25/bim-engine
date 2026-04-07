# Cairo GUI — cairo_layout.h & cairo_style_editor.h

> Framework GUI Cairo/Win32 self-contained pour SIG métier  
> Compilateur : GCC/MinGW-w64 (MSYS2) — Dépendances : cairo, gdi32, comdlg32

---

## Architecture générale

```
cairo_layout.h        ← toolbar + combo layer + statusbar
cairo_menu.h          ← menu contextuel 2 niveaux
cairo_style_editor.h  ← dialogue modal édition de style
```

Chaque fichier est **self-contained** (pattern single-header) :

```c
#define CAIRO_LAYOUT_IMPLEMENTATION
#include "cairo_layout.h"
```

---

## cairo_layout.h

### Structure visuelle

```
┌─────────────────────────────────────────────────────────────┐
│  TOOLBAR  [btn][btn] | [▼ Layer 1          ][+][×][...]    │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  CANVAS  (HWND fourni par l'appelant)                      │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│  STATUSBAR  [message          ] [     ] [X: 123  Y: 456 ]  │
└─────────────────────────────────────────────────────────────┘
```

### Hauteurs (px)

| Constante       | Valeur | Zone       |
|-----------------|--------|------------|
| `CL_MENUBAR_H`  | 24     | Menubar    |
| `CL_TOOLBAR_H`  | 38     | Toolbar    |
| `CL_STATUS_H`   | 22     | Statusbar  |

### Cycle de vie

```c
CairoLayout *layout = cl_create(hwnd_parent, hwnd_canvas);
// ... ajouter boutons, combo, panes ...
cl_resize(layout);   // dans WM_SIZE
cl_destroy(layout);  // dans WM_DESTROY
```

### Toolbar — boutons

```c
// Bouton texte
cl_toolbar_add_btn(layout, ID, "label", CL_BTN_NORMAL);
cl_toolbar_add_btn(layout, ID, "label", CL_BTN_TOGGLE);
cl_toolbar_add_btn(layout, ID, "label", CL_BTN_DISABLED);

// Bouton icône Cairo (fonction de dessin)
typedef void (*ClIconFn)(cairo_t *cr, double cx, double cy);
cl_toolbar_add_icon(layout, ID, my_icon_fn, "Tooltip", CL_BTN_NORMAL);

// Split button (bouton principal + flèche dropdown)
// ┌─────────────────┬───┐
// │  ⬡  Place node  │ ▼ │
// └─────────────────┴───┘
cl_toolbar_add_btn(layout, ID_PLACE, "Node", CL_BTN_SPLIT);
// ou avec icône :
cl_toolbar_add_icon(layout, ID_PLACE, my_icon_fn, "Placer", CL_BTN_SPLIT);

// Séparateur vertical
cl_toolbar_add_sep(layout);

// État
cl_toolbar_set_pressed(layout, ID, 1);   // activer toggle
cl_toolbar_set_enabled(layout, ID, 0);   // griser
```

### Callback split button

```c
void on_toolbar(int id, void *userdata) {

    // Clic bouton principal → action directe
    if (id == ID_PLACE_NODE) {
        g_placing = 1;   // placer le dernier symbole utilisé
        return;
    }

    // Clic flèche ▼ → CL_ID_SPLIT_BASE + id du bouton
    if (id == CL_ID_SPLIT_BASE + ID_PLACE_NODE) {
        CspSymbol *syms = build_picker_symbols(g_symbol_cache);
        CspResult result;
        if (csp_pick(hwnd, syms, &result)) {
            g_last_symbol = &g_symbol_cache[result.symbol_idx];
            g_last_angle  = result.angle;
            g_placing     = 1;
        }
        free(syms);
        return;
    }
    // ...
}
```

> **Règle à retenir** : `id == CL_ID_SPLIT_BASE + votre_id` → flèche, `id == votre_id` → bouton principal

### Toolbar — combo layer

```c
// Créer la combo (largeur en px)
cl_toolbar_add_combo(layout, ID_COMBO, 160);

// Alimenter
cl_combo_add_layer  (layout, ID_COMBO, "Réseau principal", 1);  // visible
cl_combo_add_layer  (layout, ID_COMBO, "Câblage BT",       0);  // masqué
cl_combo_set_current(layout, ID_COMBO, 0);

// Modifier
cl_combo_set_name   (layout, ID_COMBO, idx, "Nouveau nom");
cl_combo_set_visible(layout, ID_COMBO, idx, 0);
cl_combo_clear      (layout, ID_COMBO);   // vider et reconstruire

// Lire
int         idx  = cl_combo_get_current(layout, ID_COMBO);
int         vis  = cl_combo_get_visible(layout, ID_COMBO, idx);
const char *name = cl_combo_get_name   (layout, ID_COMBO, idx);
int         n    = cl_combo_get_count  (layout, ID_COMBO);
```

### Callback toolbar

```c
void on_toolbar(int id, void *userdata) {

    // Toggle visibilité œil — ID = CL_ID_EYE_BASE + index_layer
    if (id >= CL_ID_EYE_BASE) {
        int layer = id - CL_ID_EYE_BASE;
        int vis   = cl_combo_get_visible(layout, ID_COMBO, layer);
        // mettre à jour rendu...
        return;
    }

    switch (id) {
    case ID_COMBO:       // sélection changée
        g_layer = cl_combo_get_current(layout, ID_COMBO);
        break;
    case ID_LAYER_ADD:   // bouton +
        // cl_combo_add_layer(...)
        break;
    case ID_LAYER_DEL:   // bouton ×
        // cl_combo_clear + reconstruire
        break;
    case ID_LAYER_STYLE: // bouton ...
        cse_edit(hwnd, &styles[g_layer]);
        break;
    }
}

cl_set_toolbar_cb(layout, on_toolbar, userdata);
```

### Statusbar

```c
// Définir les zones (ordre de gauche à droite)
cl_status_add_pane(layout, CL_PANE_FIXED,  220);  // largeur fixe px
cl_status_add_pane(layout, CL_PANE_SPRING,   0);  // espace élastique
cl_status_add_pane(layout, CL_PANE_FIXED,  200);  // coordonnées

// Mettre à jour
cl_status_set(layout, 0, "Prêt");
cl_status_set(layout, 2, "X: 123.45  Y: 678.90");
```

### Intégration WndProc parent

```c
case WM_SIZE:    cl_resize(layout);  break;
case WM_DESTROY: cl_destroy(layout); break;
```

---

## cairo_style_editor.h

### Structure du dialogue

```
┌─────────────────────────────────────────┐
│  Nom du style  [___________________]    │
├── TRAIT ────────────────────────────────┤
│  Couleur   [███] [Choisir...]           │
│  Épaisseur [══════●══] 2.0 px           │
│  Style     [━━━][···][─·─][─··─]        │
├── REMPLISSAGE ──────────────────────────┤
│  Couleur   [███] [Choisir...]           │
│  Actif     [✓]                          │
├── TRANSPARENCE ─────────────────────────┤
│  Alpha     [══════●══] 100%             │
├── APERÇU ───────────────────────────────┤
│  ╔══════════════════════════════════╗   │
│  ║   preview live polyligne+polygon ║   │
│  ╚══════════════════════════════════╝   │
├─────────────────────────────────────────┤
│              [Annuler]  [Appliquer]     │
└─────────────────────────────────────────┘
```

### Structure Style

```c
typedef enum {
    STROKE_SOLID = 0,   // trait plein
    STROKE_DASH,        // tirets
    STROKE_DOT,         // pointillés
    STROKE_DASHDOT,     // tiret-point
} StrokeStyle;

typedef struct {
    char        name[64];
    // Trait
    double      stroke_r, stroke_g, stroke_b;
    double      stroke_width;      // 0.5 → 10.0 px
    StrokeStyle stroke_style;
    // Remplissage
    double      fill_r, fill_g, fill_b;
    int         fill_active;
    // Transparence globale
    double      alpha;             // 0.0 → 1.0
} Style;
```

### Initialisation

```c
Style s;
style_init_default(&s, "Réseau principal");
// stroke : bleu 0.30/0.70/1.00, width 2.0, solid
// fill   : inactif
// alpha  : 1.0
```

### Ouverture du dialogue

```c
// Retourne 1 si Appliquer, 0 si Annuler/Escape
if (cse_edit(hwnd_parent, &style)) {
    // style contient les nouvelles valeurs
    InvalidateRect(hwnd_canvas, NULL, FALSE);
}
```

### Application dans draw_cb

```c
// Avant cairo_stroke()
style_apply_stroke(cr, &style);
cairo_stroke(cr);

// Avant cairo_fill()
if (style.fill_active) {
    style_apply_fill(cr, &style);
    cairo_fill(cr);
}
```

### StyleManager (un style par layer)

```c
#define MAX_LEVELS 32

typedef struct {
    Style styles[MAX_LEVELS];
    int   count;
} StyleManager;

void sm_init(StyleManager *sm) {
    sm->count = MAX_LEVELS;
    for (int i = 0; i < MAX_LEVELS; i++) {
        char name[64];
        snprintf(name, sizeof name, "Layer %d", i);
        style_init_default(&sm->styles[i], name);
    }
}

Style *sm_get(StyleManager *sm, int level) {
    if (level < 0 || level >= sm->count) return &sm->styles[0];
    return &sm->styles[level];
}

// Éditer le style du layer courant
cse_edit(hwnd, sm_get(&sm, g_layer_current));
```

### Dépendances

```makefile
LDFLAGS = -lcairo -lgdi32 -lcomdlg32 -lm -mwindows
```

> `comdlg32` est requis pour `ChooseColor()` (picker couleur natif Windows)

---

## cairo_symbol_picker.h

### Structure du dialogue

```
┌─────────────────────────────────────────────────┐
│  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐  │
│  │ SVG  │ │ SVG  │ │ SVG  │ │ SVG  │ │ SVG  │  │
│  │Vanne │ │Inter.│ │Transf│ │ Nœud │ │ ...  │  │
│  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘  │
├── APERÇU ───────────────────────────────────────┤
│  ┌────────────┐   Nom     Catégorie             │
│  │  preview   │   Ports : E W                   │
│  │  + ports   │   [↺]  0°  [↻]                 │
│  └────────────┘                                 │
├─────────────────────────────────────────────────┤
│                      [Annuler]  [Sélectionner]  │
└─────────────────────────────────────────────────┘
```

### Structure CspSymbol

```c
typedef struct {
    const char *name;
    const char *category;
    const char *svg_text;    /* SVG inline — utilisé si rsvg == NULL        */
    RsvgHandle *rsvg;        /* handle pré-compilé — prioritaire sur svg_text */
    int         ports_mask;
    double      width, height;
    int         port_max;
} CspSymbol;
```

### Depuis un cache SQLite (stb_ds hash table)

```c
// Conversion hash table → tableau CspSymbol
CspSymbol *build_picker_symbols(SymbolCacheEntry *cache) {
    int count = hmlen(cache);
    CspSymbol *out = calloc(count + 1, sizeof(CspSymbol));
    for (int i = 0; i < count; i++) {
        out[i].name       = cache[i].name;
        out[i].rsvg       = cache[i].handle;  // pré-compilé, zéro rechargement
        out[i].ports_mask = cache[i].ports_mask;
        out[i].width      = cache[i].w;
        out[i].height     = cache[i].h;
        out[i].port_max   = 1;
    }
    out[count].name = NULL;  /* sentinel — obligatoire */
    return out;
}

// Ouverture
CspSymbol *syms = build_picker_symbols(g_symbol_cache);
CspResult result;
if (csp_pick(hwnd, syms, &result)) {
    // result.symbol_idx → index dans syms
    // result.angle      → rotation initiale choisie
    // g_symbol_cache[result.symbol_idx].key → symbol_id SQLite
}
free(syms);  // safe — les handles pré-compilés ne sont pas libérés
```

### Depuis SVG inline

```c
static const CspSymbol g_symbols[] = {
    { "Vanne", "Hydraulique", "<svg...>", NULL,
      PORT_BIT(PORT_E)|PORT_BIT(PORT_W), 20, 20, 1 },
    { NULL }  /* sentinel — NE PAS SUPPRIMER */
};
csp_pick(hwnd, g_symbols, &result);
```

### Navigation clavier

| Touche | Action |
|--------|--------|
| `←→↑↓` | Naviguer dans la grille |
| `R` | Rotation +90° |
| `Entrée` | Sélectionner |
| `Echap` | Annuler |
| Double-clic | Sélectionner directement |

### Dépendances

```makefile
CFLAGS  += $(shell pkg-config --cflags librsvg-2.0 glib-2.0)
LDFLAGS += $(shell pkg-config --libs   librsvg-2.0 glib-2.0)
LDFLAGS += -lcairo -lgdi32 -ldwmapi -lm -mwindows
```

---

## Compilation complète (tous modules)

```bash
gcc main.c -o app.exe \
    $(pkg-config --cflags --libs librsvg-2.0 glib-2.0) \
    -lcairo -lgdi32 -lcomdlg32 -ldwmapi -lm -mwindows \
    -I"C:/msys64/mingw64/include/cairo" \
    -L"C:/msys64/mingw64/lib"
```

| Module | Dépendance supplémentaire |
|--------|--------------------------|
| `cairo_layout.h` | `-lgdi32` |
| `cairo_style_editor.h` | `-lcomdlg32` |
| `cairo_symbol_picker.h` | `pkg-config librsvg-2.0 glib-2.0` |
| Dark mode titlebar | `-ldwmapi` |

---

## Points d'attention

- **Macros couleur** — ne pas utiliser `#define COL 0.1, 0.2, 0.3` dans les appels Cairo,  
  utiliser des fonctions `static inline void col_xxx(cairo_t *cr, double a)` à la place
- **GET_X_LPARAM / GET_Y_LPARAM** — nécessite `#include <windowsx.h>`
- **Une seule surface Cairo active** par HDC à la fois — libérer avant d'en créer une autre
- **cm_render()** (menu contextuel) doit être appelé **après** `cairo_destroy/surface_destroy`
- **Dropdown combo** — HWND popup `WS_EX_TOPMOST`, se ferme sur `WM_KILLFOCUS`
- **WM_DESTROY dialogues modaux** — jamais de `PostQuitMessage` — boucle modale via `IsWindow()`
- **Dark mode** — appeler `DwmSetWindowAttribute` **après** `ShowWindow` pour les `WS_POPUP`
- **windres** — utiliser `echo "101 ICON app.ico" > app.rc && windres app.rc -o resources.res`
