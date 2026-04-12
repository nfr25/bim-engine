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


---

## cairo_transcript.h

### Structure visuelle

```
┌─────────────────────────────────────────────────────────────┐
│  [14:23:01]  SELECT * FROM nodes WHERE layer_id = 1        │ ← CMD
│  [ok]        3 résultats trouvés                           │ ← OK
│  [14:23:15]  LOAD shapefile 'urb_parcelles.shp'            │ ← CMD
│  [err]       Fichier non trouvé : urb_parcelles.shp        │ ← ERR
│  [info]      Chargement layer 'Default' en cours...        │ ← INFO
│                                             ║ scrollbar    │
├─────────────────────────────────────────────────────────────┤
│  > _                                                        │ ← INPUT
└─────────────────────────────────────────────────────────────┘
```

### Cycle de vie

```c
CairoTranscript *ct = ct_create(parent, 0, 0, 1, 1); // pos provisoire
ct_set_cb(ct, on_command, userdata);

// intégration WndProc parent (si utilisé sans tabpane)
case WM_SIZE:        ct_resize(ct, x, y, w, h);        break;
case WM_CTLCOLOREDIT: return ct_on_ctlcolor(ct, (HDC)wp, (HWND)lp);
case WM_DESTROY:     ct_destroy(ct);                   break;
```

### Types de lignes

```c
ct_add(ct, CT_CMD,  "SELECT * FROM nodes");   // bleu clair
ct_add(ct, CT_OK,   "3 résultats");           // vert
ct_add(ct, CT_ERR,  "Fichier non trouvé");    // rouge
ct_add(ct, CT_INFO, "Chargement...");         // gris
ct_add(ct, CT_WARN, "Index manquant");        // orange
ct_add(ct, CT_SEP,  "");                      // ligne séparatrice fine
```

### Callback commande

```c
void on_command(const char *text, void *userdata) {
    ct_add(ct, CT_CMD, text);   // écho de la commande
    // traiter text...
}
ct_set_cb(ct, on_command, NULL);
```

### Prompt et contrôle de l'input

```c
ct_set_input(ct, "texte");      // pré-remplir la zone de saisie
ct_clear(ct);                   // vider tout le log
ct_scroll_to_bottom(ct);        // forcer scroll en bas
```

> Le prompt `>` est un `EM_SETCUEBANNER` posé à la création — il disparaît
> dès que l'utilisateur commence à saisir.

---

## cairo_tabpane.h

### Structure visuelle

```
┌──────────╲  ╭──────────────╲  ╭──────────────╲  < >
│ Transcript│  │ Sélection  × │  │ Query 1     × │
├───────────────────────────────────────────────────────┤
│                                                       │
│   contenu de l'onglet actif (HWND enfant)            │
│                                                       │
└───────────────────────────────────────────────────────┘
```

### Règles de design

- **Tab 0** : fixe, sans bouton close (ex: Transcript)
- **Tab 1..n** : dynamiques, bouton close visible au hover
- Forme **trapézoïdale** (pente à droite) — `TP_SLANT 10.0` ajustable
- Largeur fixe limitée à `TP_TAB_MAX_W` — s'étale mais ne dépasse pas
- Overflow : flèches `< >` apparaissent automatiquement
- Ligne de séparation **interrompue** sous le tab actif (fusion avec le contenu)

### Ordre des includes (obligatoire)

```c
#define CAIRO_TRANSCRIPT_IMPLEMENTATION
#include "ui/cairo_transcript.h"
#define CAIRO_TABPANE_IMPLEMENTATION
#include "ui/cairo_tabpane.h"
#define CAIRO_LAYOUT_IMPLEMENTATION
#include "ui/cairo_layout.h"          // détecte CL_HAS_TABPANE automatiquement
```

### Intégration dans CairoLayout (recommandée)

```c
// Création — remplace cl_attach_transcript
CairoTranscript *ct = ct_create(parent, 0, 0, 1, 1);
ct_set_cb(ct, on_command, NULL);
cl_attach_tabpane(g_layout, ct, 220, on_tab_close, NULL);

// WM_SIZE — inchangé, cl_resize propage tout
case WM_SIZE: cl_resize(g_layout); return 0;

// WM_DESTROY — inchangé, cl_destroy détruit le tabpane
case WM_DESTROY: cl_destroy(g_layout); PostQuitMessage(0); return 0;
```

> **Note** : `WM_CTLCOLOREDIT` n'est plus nécessaire dans `ParentProc` —
> il est intercepté dans le wrapper interne `BIM_CtWrap`.

### Callback fermeture de tab

```c
static void on_tab_close(int idx, HWND content, void *ud)
{
    if (!content) return;
    // Détruire le contenu associé (ex: BimListView)
    BimListView *lv = ui_get_data(BimListView, content);
    if (lv) blv_destroy(lv);   // blv_destroy détruit aussi le HWND
}
```

### Ouvrir un tab dynamique

```c
// Créer le contenu (ex: listview)
BimListView *lv = blv_create(cl_tabpane_get(g_layout)->hwnd,
                              0, 0, 1, 1, db);
blv_set_query(lv, "SELECT id, nom FROM bim_nodes");
blv_set_cb(lv, on_row_dbl, on_row_sel, NULL);

// Ouvrir le tab — closable, contenu associé
int idx = cl_tabpane_add(g_layout, "Noeuds", blv_get_hwnd(lv));
cl_tabpane_select(g_layout, idx);
```

### API complète

```c
// Cycle de vie
CairoTabPane *tp_create (HWND parent, int x, int y, int w, int h);
void          tp_destroy(CairoTabPane *tp);
void          tp_resize (CairoTabPane *tp, int x, int y, int w, int h);
void          tp_set_cb (CairoTabPane *tp, TpEventCb cb, void *ud);

// Gestion des tabs
int   tp_tab_add        (CairoTabPane *tp, const char *label, int closable);
void  tp_tab_close      (CairoTabPane *tp, int idx);
void  tp_tab_select     (CairoTabPane *tp, int idx);
void  tp_tab_set_label  (CairoTabPane *tp, int idx, const char *label);
int   tp_tab_current    (CairoTabPane *tp);   // index actif
int   tp_tab_count      (CairoTabPane *tp);   // nombre total

// Contenu
void  tp_tab_set_content(CairoTabPane *tp, int idx, HWND hwnd_content);
HWND  tp_tab_get_content(CairoTabPane *tp, int idx);
void  tp_get_content_rect(CairoTabPane *tp, RECT *rc);

// Via CairoLayout (préféré)
int           cl_tabpane_add   (CairoLayout *l, const char *label, HWND content);
void          cl_tabpane_close (CairoLayout *l, int idx);
void          cl_tabpane_select(CairoLayout *l, int idx);
CairoTabPane *cl_tabpane_get   (CairoLayout *l);
```

### Événements callback

```c
// TpEventCb : void (*)(int event, int tab_idx, void *userdata)
#define TP_EV_SELECT  1   // tab activé
#define TP_EV_CLOSE   2   // tab fermé (détruire le contenu dans le callback)
```

### Constantes ajustables

```c
#define TP_TAB_H      28    // hauteur de la barre de tabs
#define TP_TAB_MAX_W 200    // largeur max d'un tab
#define TP_TAB_MIN_W  60    // largeur min d'un tab
#define TP_SLANT      10.0  // pente droite du tab (style trapèze)
#define TP_RADIUS      4.0  // rayon du coin haut-gauche
#define TP_MAX_TABS   32    // nombre max de tabs
```

---

## bim_listview.h

### Structure visuelle

```
┌─────┬──────────────────┬────────────┬───────────────────────┐
│  id │ nom            ↑ │ x          │ y                     │ ← header (tri)
├─────┼──────────────────┼────────────┼───────────────────────┤
│   1 │ Noeud_A          │   123.456  │   789.012             │
│   2 │ Vanne_01         │   234.567  │   890.123             │ ← rows
│ ... │ ...              │ ...        │ ...                   │
└─────┴──────────────────┴────────────┴───────────────────────┘ ← scrollbar native
```

### Caractéristiques

- **Virtuelle** — seules les lignes visibles sont dessinées, cache glissant de 512 lignes
- **Query stockée** — la listview garde sa query et peut être rafraîchie
- **Colonnes auto-détectées** depuis `sqlite3_column_name()`
- **Colonnes redimensionnables** par drag du séparateur de header
- **Tri** par clic header (ASC/DESC, `ORDER BY` injecté automatiquement)
- **Scrollbar Win32 native** + molette souris
- **Session uniquement** — largeurs de colonnes non persistées
- **NULL SQLite** affiché en italique gris

### Cycle de vie

```c
BimListView *lv = blv_create(hwnd_parent, x, y, w, h, db_handle);
blv_set_query(lv, "SELECT id, nom, x, y FROM bim_nodes WHERE layer=1");
blv_set_cb(lv, on_row_dblclick, on_row_select, userdata);

// intégration WndProc (si géré manuellement)
case WM_SIZE:    blv_resize(lv, x, y, new_w, new_h); break;
case WM_DESTROY: blv_destroy(lv);                    break;
```

### Requêtes

```c
// Définir ou changer la query (refresh automatique)
blv_set_query(lv, "SELECT id, nom FROM bim_nodes");

// Rafraîchir après INSERT/UPDATE/DELETE
blv_refresh(lv);

// Lire l'état
int total    = blv_row_count(lv);   // nombre total de lignes
int selected = blv_selected(lv);    // index sélectionné (-1 si aucun)
HWND hwnd    = blv_get_hwnd(lv);    // HWND pour tp_tab_set_content
```

### Callbacks

```c
// Double-clic sur une ligne — tous les champs disponibles
void on_row_dblclick(int row, int col_count,
                     const char **col_names,
                     const char **values,
                     void *userdata)
{
    // Chercher la colonne "id" pour centrer le canvas
    for (int i = 0; i < col_count; i++) {
        if (strcmp(col_names[i], "id") == 0 && values[i]) {
            canvas_focus_entity(g_canvas, atoi(values[i]));
            break;
        }
    }
}

// Sélection simple
void on_row_select(int row, void *userdata) {
    // row = index 0-based dans la query
}

blv_set_cb(lv, on_row_dblclick, on_row_select, NULL);
```

### Pattern "sélection canvas → tab listview"

```c
void on_canvas_selection(int *ids, int count)
{
    if (count == 0) return;

    // Construire une query IN (id1, id2, ...)
    char sql[4096];
    int pos = snprintf(sql, sizeof sql,
        "SELECT e.id, l.nom AS layer, e.type "
        "FROM bim_entities e "
        "JOIN bim_layers l ON l.id = e.layer_id "
        "WHERE e.id IN (");
    for (int i = 0; i < count; i++)
        pos += snprintf(sql + pos, sizeof sql - pos,
                        i ? ",%d" : "%d", ids[i]);
    strncat(sql, ")", sizeof sql - pos - 1);

    // Ouvrir un tab avec la sélection
    BimListView *lv = blv_create(
        cl_tabpane_get(g_layout)->hwnd, 0, 0, 1, 1,
        cv->db.handle);
    blv_set_query(lv, sql);
    blv_set_cb(lv, on_row_dblclick, NULL, NULL);

    char label[64];
    snprintf(label, sizeof label, "Sélection (%d)", count);
    cl_tabpane_add(g_layout, label, blv_get_hwnd(lv));
}
```

### Rafraîchir tous les tabs après modification DB

```c
CairoTabPane *tp = cl_tabpane_get(g_layout);
for (int i = 1; i < tp_tab_count(tp); i++) {   // i=1 : skip transcript
    HWND hc = tp_tab_get_content(tp, i);
    if (!hc) continue;
    BimListView *lv = ui_get_data(BimListView, hc);
    if (lv) blv_refresh(lv);
}
```

### Note sur les performances de tri

Le tri injecte `SELECT * FROM (query) ORDER BY "col" ASC|DESC`. SQLite doit
matérialiser le résultat avant de trier — sur de grandes tables sans index,
cela peut être lent. **Solution** : créer des index sur les colonnes triées
fréquemment, ou limiter le tri aux colonnes indexées.

```sql
CREATE INDEX idx_nodes_nom ON bim_nodes(nom);
```

### Constantes ajustables

```c
#define BLV_HEADER_H      24    // hauteur du header
#define BLV_ROW_H         20    // hauteur d'une ligne
#define BLV_COL_DEFAULT_W 120   // largeur par défaut des colonnes
#define BLV_CACHE_ROWS    512   // taille de la fenêtre cache
#define BLV_MAX_COLS       32   // colonnes max par query
```

---

## Architecture complète avec tabpane

```
cairo_layout.h          ← framework principal (menubar + toolbar + splitter)
  ├── cairo_transcript.h  ← Tab 0 fixe (log + input SQL)
  ├── cairo_tabpane.h     ← barre de tabs trapézoïdale Cairo
  │     └── bim_listview.h  ← Tab 1..n dynamiques (virtual list SQLite)
  └── ui_backend.h        ← abstraction Win32/Cairo (UiDrawCtx, macros PAINT)
```

### Structure visuelle complète

```
┌─────────────────────────────────────────────────────────────┐
│  [menu1] [menu2]                               MENUBAR  24px│
├─────────────────────────────────────────────────────────────┤
│  [⊕][⊖][≡] | [▼ Layer 1 ][+][×] | [/][○][✦▼]  TOOLBAR 38px│
├─────────────────────────────────────────────────────────────┤
│                                                             │
│                   CANVAS (BIM/GIS)               variable   │
│                                                             │
├─────── splitter ────────────────────────────────────────────┤
│ Transcript╲  Sélection(3)×╲  Query 1×╲          TABPANE    │
├─────────────────────────────────────────────────────────────┤
│  log Cairo + scrollbar                                      │
│  > _                                             variable   │
├─────────────────────────────────────────────────────────────┤
│  [Prêt          ] [     mode     ] [X: 123  Y: 456]  22px  │
└─────────────────────────────────────────────────────────────┘
```

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
