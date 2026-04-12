/*
 * cairo_layout.h  –  Layout Win32 multi-zones Cairo, self-contained
 * ─────────────────────────────────────────────────────────────────
 *
 *  STRUCTURE
 *  ─────────
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  TOOLBAR  [btn][btn] | [▼ Layer 1          ][+][×][...]    │
 *   ├─────────────────────────────────────────────────────────────┤
 *   │                                                             │
 *   │  CANVAS  (votre fenêtre existante)                         │
 *   │                                                             │
 *   ├─────────────────────────────────────────────────────────────┤
 *   │  STATUSBAR                                                  │
 *   └─────────────────────────────────────────────────────────────┘
 *
 *  UTILISATION MINIMALE
 *  ─────────────────────
 *    #define CAIRO_LAYOUT_IMPLEMENTATION
 *    #include "cairo_layout.h"
 *
 *  CRÉATION
 *  ────────
 *    CairoLayout *layout = cl_create(hwnd_parent, hwnd_canvas);
 *
 *    // Boutons classiques
 *    cl_toolbar_add_btn (layout, 1, "Nouv",  CL_BTN_NORMAL);
 *    cl_toolbar_add_sep (layout);
 *
 *    // Bouton icône SVG (RsvgHandle pré-compilé)
 *    cl_toolbar_add_svg(layout, ID, rsvg_handle, "Tooltip", CL_BTN_NORMAL);
 *    cl_toolbar_add_svg(layout, ID, rsvg_handle, "Tooltip", CL_BTN_SPLIT);
 *    // g_object_unref(rsvg_handle) reste à la charge de l'appelant
 *
 *    // Split button (bouton principal + flèche dropdown)
 *    cl_toolbar_add_btn (layout, ID_PLACE, "⬡ Node", CL_BTN_SPLIT);
 *    // → clic bouton  : on_toolbar reçoit ID_PLACE
 *    // → clic flèche  : on_toolbar reçoit CL_ID_SPLIT_BASE + ID_PLACE
 *
 *    // Combo layer
 *    cl_toolbar_add_combo(layout, ID_COMBO,  160);
 *    cl_toolbar_add_btn  (layout, ID_ADD,    "+",   CL_BTN_NORMAL);
 *    cl_toolbar_add_btn  (layout, ID_DEL,    "×",   CL_BTN_NORMAL);
 *    cl_toolbar_add_btn  (layout, ID_STYLE,  "...", CL_BTN_NORMAL);
 *
 *    // Alimenter la combo
 *    cl_combo_add_layer  (layout, ID_COMBO, "Layer 1", 1);
 *    cl_combo_add_layer  (layout, ID_COMBO, "Layer 2", 0);  // masqué
 *    cl_combo_set_current(layout, ID_COMBO, 0);
 *
 *    // Callbacks
 *    cl_set_toolbar_cb(layout, on_toolbar, userdata);
 *    // on_toolbar reçoit :
 *    //   ID_COMBO          → sélection changée → cl_combo_get_current()
 *    //   CL_ID_EYE_BASE+n  → toggle visibilité layer n
 *    //   ID_ADD            → créer layer
 *    //   ID_DEL            → supprimer layer courant
 *    //   ID_STYLE          → ouvrir éditeur style
 *
 *  LECTURE ÉTAT COMBO
 *  ───────────────────
 *    int idx          = cl_combo_get_current(layout, ID_COMBO);
 *    int visible      = cl_combo_get_visible(layout, ID_COMBO, idx);
 *    const char *name = cl_combo_get_name   (layout, ID_COMBO, idx);
 *    int db_id        = cl_combo_get_db_id  (layout, ID_COMBO, idx);
 *    int geom_type    = cl_combo_get_geom_type(layout, ID_COMBO, idx);
 *    int style_id     = cl_combo_get_style_id (layout, ID_COMBO, idx);
 *
 *  MISE À JOUR COMBO
 *  ──────────────────
 *    cl_combo_clear      (layout, ID_COMBO);
 *    cl_combo_add_layer  (layout, ID_COMBO, "Réseau", 1);
 *    cl_combo_set_current(layout, ID_COMBO, 0);
 *    cl_combo_set_visible(layout, ID_COMBO, 2, 0);  // masquer layer 2
 *    cl_combo_set_name   (layout, ID_COMBO, 0, "Nouveau nom");
 *
 *  INTÉGRATION WndProc (fenêtre PARENT)
 *  ──────────────────────────────────────
 *    case WM_SIZE:    cl_resize(layout);  break;
 *    case WM_DESTROY: cl_destroy(layout); break;
 *
 * ─────────────────────────────────────────────────────────────────
 *  Dépendances : cairo, cairo-win32, gdi32
 *  Compilateur : GCC / MinGW-w64 (MSYS2)
 * ─────────────────────────────────────────────────────────────────
 */

#ifndef CAIRO_LAYOUT_H
#define CAIRO_LAYOUT_H

#include "ui_backend.h"   /* Win32/Cairo abstraction — ui_register_class,
                             ui_subwnd_create, UI_PAINT_BEGIN/END, UiDrawCtx */
#include <windows.h>
#include <windowsx.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <librsvg/rsvg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* cairo_transcript.h doit être inclus AVANT cairo_layout.h
 * si on veut utiliser cl_attach_tabpane / cl_attach_transcript.
 * Si non inclus, les fonctions transcript sont désactivées. */
#ifdef CAIRO_TRANSCRIPT_H
#  define CL_HAS_TRANSCRIPT 1
#else
#  define CL_HAS_TRANSCRIPT 0
   typedef void CairoTranscript;  /* opaque vide si pas inclus */
#endif

/* cairo_tabpane.h doit être inclus AVANT cairo_layout.h
 * si on veut utiliser cl_attach_tabpane.
 * Ordre recommandé dans bim.c :
 *   #define CAIRO_TRANSCRIPT_IMPLEMENTATION
 *   #include "ui/cairo_transcript.h"
 *   #define CAIRO_TABPANE_IMPLEMENTATION
 *   #include "ui/cairo_tabpane.h"
 *   #define CAIRO_LAYOUT_IMPLEMENTATION
 *   #include "ui/cairo_layout.h"
 */
#ifdef CAIRO_TABPANE_H
#  define CL_HAS_TABPANE 1
#else
#  define CL_HAS_TABPANE 0
   typedef void CairoTabPane;     /* opaque vide si pas inclus */
#endif

/* ═══════════════════════════════════════════════════════════════════
   SECTION PUBLIQUE
   ═══════════════════════════════════════════════════════════════════ */

/* ── Hauteurs des zones (px) ─────────────────────────────────────── */
#define CL_MENUBAR_H    24
#define CL_TOOLBAR_H    54
#define CL_STATUS_H     32

/* ── Flags boutons toolbar ───────────────────────────────────────── */
#define CL_BTN_NORMAL   0x00
#define CL_BTN_TOGGLE   0x01
#define CL_BTN_DISABLED 0x02
#define CL_BTN_SPLIT    0x04   /* bouton principal + flèche dropdown */

/* ── ID réservé pour la flèche des split buttons ─────────────────── */
/* on_toolbar reçoit CL_ID_SPLIT_BASE + id_bouton pour le clic flèche */
#define CL_ID_SPLIT_BASE  0x2000

/* ── Type de pane statusbar ──────────────────────────────────────── */
#define CL_PANE_FIXED   0
#define CL_PANE_SPRING  1

/* ── Limites ─────────────────────────────────────────────────────── */
#define CL_MAX_LAYERS   64
#define CL_MAX_ELEMS    80
#define CL_MAX_COMBOS    8
#define CL_MAX_MENUITEMS 16   /* entrées max dans la menubar          */

/* ── ID réservé pour les événements œil ─────────────────────────── */
/* on_toolbar reçoit CL_ID_EYE_BASE + index_layer pour toggle visib. */
#define CL_ID_EYE_BASE  0x4000

/* ── Opaque handle ───────────────────────────────────────────────── */
typedef struct CairoLayout_ CairoLayout;

/* ── Callback toolbar ────────────────────────────────────────────── */
typedef void (*CairoLayoutCb)(int btn_id, void *userdata);

/* ── Fonction icône — mode permet de dessiner différemment selon le mode actif ── */
typedef void (*ClIconFn)(cairo_t *cr, double cx, double cy, int mode);

/* ─── Cycle de vie ──────────────────────────────────────────────── */
CairoLayout *cl_create (HWND parent, HWND canvas);
void         cl_destroy(CairoLayout *l);
void         cl_resize (CairoLayout *l);

/* ─── Toolbar — boutons ─────────────────────────────────────────── */
int  cl_toolbar_add_btn    (CairoLayout *l, int id, const char *label, int flags);
int  cl_toolbar_add_icon   (CairoLayout *l, int id, ClIconFn fn,
                             const char *tooltip, int flags);
int  cl_toolbar_add_icon_multimode(CairoLayout *l, int id, ClIconFn fn,
                             const char *tooltip, int mode_count);
int  cl_toolbar_add_svg    (CairoLayout *l, int id, RsvgHandle *rsvg,
                             const char *tooltip, int flags);
void cl_toolbar_add_sep    (CairoLayout *l);
void cl_toolbar_set_pressed(CairoLayout *l, int id, int pressed);
void cl_toolbar_set_enabled(CairoLayout *l, int id, int enabled);
void cl_toolbar_set_mode   (CairoLayout *l, int id, int mode);
int  cl_toolbar_get_mode   (CairoLayout *l, int id);
void cl_set_toolbar_cb     (CairoLayout *l, CairoLayoutCb cb, void *ud);
int  cl_toolbar_get_btn_screen_rect(CairoLayout *l, int id, RECT *rc_screen);
/* Retourne 1 si trouvé, 0 sinon.
 * rc_screen contient le rectangle écran du bouton —
 * pratique pour positionner un menu sous un bouton toolbar :
 *   RECT rc;
 *   cl_toolbar_get_btn_screen_rect(layout, ID_ZOOM, &rc);
 *   cm_show(menu, rc.left, rc.bottom);                      */

/* ─── Toolbar — combo layer ─────────────────────────────────────── */
int  cl_toolbar_add_combo  (CairoLayout *l, int id, int width);

void        cl_combo_clear      (CairoLayout *l, int id);
int         cl_combo_add_layer     (CairoLayout *l, int id, const char *name, int visible);
void        cl_combo_set_current   (CairoLayout *l, int id, int idx);
void        cl_combo_set_name      (CairoLayout *l, int id, int idx, const char *name);
void        cl_combo_set_visible   (CairoLayout *l, int id, int idx, int visible);
void        cl_combo_set_layer_data(CairoLayout *l, int id, int idx,
                                    int db_id, int geom_type, int style_id);
int         cl_combo_get_current   (CairoLayout *l, int id);
int         cl_combo_get_visible   (CairoLayout *l, int id, int idx);
const char *cl_combo_get_name      (CairoLayout *l, int id, int idx);
int         cl_combo_get_count     (CairoLayout *l, int id);
int         cl_combo_get_db_id     (CairoLayout *l, int id, int idx);
int         cl_combo_get_geom_type (CairoLayout *l, int id, int idx);
int         cl_combo_get_style_id  (CairoLayout *l, int id, int idx);

/* ─── Menubar ───────────────────────────────────────────────────── */
/* Ajouter une entrée dans la barre de menu.
 * Le callback reçoit l'id quand l'entrée est cliquée —
 * c'est là qu'on appelle cm_show() avec cl_menubar_get_item_screen_rect. */
int  cl_menubar_add        (CairoLayout *l, int id, const char *label);
void cl_set_menubar_cb     (CairoLayout *l, CairoLayoutCb cb, void *ud);
int  cl_menubar_get_item_screen_rect(CairoLayout *l, int id, RECT *rc_screen);
void cl_menubar_close      (CairoLayout *l);   /* fermer l'item ouvert */
int  cl_status_add_pane    (CairoLayout *l, int type, int width);
void cl_status_set         (CairoLayout *l, int pane_idx, const char *text);
void cl_status_set_progress(CairoLayout *l, int pane_idx, float progress);
void cl_status_clear       (CairoLayout *l, int pane_idx);

/* ─── Transcript + splitter + tabpane ───────────────────────────── */
/*
 * cl_attach_tabpane — API recommandée quand cairo_tabpane.h est inclus
 * ──────────────────────────────────────────────────────────────────────
 * Attache un CairoTabPane au layout. Le tabpane occupe la partie basse
 * du splitter. Le transcript est automatiquement ajouté en Tab 0
 * (fixe, sans bouton close).
 *
 * init_h         : hauteur initiale de la zone basse (ex: 220)
 * on_tab_close   : callback appelé quand un tab dynamique est fermé
 *                  → bim.c doit y détruire le contenu (BimListView, etc.)
 *
 * Après cet appel, utiliser :
 *   cl_tabpane_add(l, "label", hwnd)  — ouvrir un tab dynamique
 *   cl_tabpane_close(l, idx)          — fermer un tab
 *   cl_tabpane_select(l, idx)         — activer un tab
 *   cl_tabpane_get(l)                 — accès direct si besoin
 */
#if CL_HAS_TABPANE && CL_HAS_TRANSCRIPT
void cl_attach_tabpane (CairoLayout *l, CairoTranscript *ct, int init_h,
                        void (*on_tab_close)(int idx, HWND content, void *ud),
                        void *ud);
#endif

int           cl_tabpane_add   (CairoLayout *l, const char *label, HWND content);
void          cl_tabpane_close (CairoLayout *l, int idx);
void          cl_tabpane_select(CairoLayout *l, int idx);
CairoTabPane *cl_tabpane_get   (CairoLayout *l);

/* ─── API legacy transcript (sans tabpane) ──────────────────────── */
void cl_attach_transcript  (CairoLayout *l, CairoTranscript *ct, int init_h);
void cl_show_transcript    (CairoLayout *l);
void cl_hide_transcript    (CairoLayout *l);
void cl_toggle_transcript  (CairoLayout *l);
void cl_get_transcript_rect(CairoLayout *l, int *x, int *y, int *w, int *h);

/* ═══════════════════════════════════════════════════════════════════
   SECTION IMPLÉMENTATION
   ═══════════════════════════════════════════════════════════════════ */
#ifdef CAIRO_LAYOUT_IMPLEMENTATION

/* ── Couleurs (fonctions inline — évite les macros multi-valeurs) ── */
static inline void cl__col_bg    (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.13, 0.14, 0.18, a); }
static inline void cl__col_bg2   (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.10, 0.11, 0.15, a); }
static inline void cl__col_border(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.22, 0.24, 0.32, a); }
static inline void cl__col_text  (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.90, 0.92, 0.95, a); }
static inline void cl__col_textd (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.42, 0.44, 0.50, a); }
static inline void cl__col_hover (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.25, 0.45, 0.90, a); }
static inline void cl__col_press (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.20, 0.38, 0.80, a); }
static inline void cl__col_sep   (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.28, 0.30, 0.40, a); }
static inline void cl__col_accent(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.30, 0.70, 1.00, a); }

/* ── Dimensions internes ─────────────────────────────────────────── */
#define CL_BTN_W        36
#define CL_BTN_SEP_W    10
#define CL_BTN_ARROW_W  16   /* largeur de la zone flèche dans un split button */
#define CL_MAX_ELEMS    80
#define CL_MAX_PANES    16
#define CL_MAX_COMBOS    8
#define CL_DROP_ITEM_H  26
#define CL_DROP_EYE_W   28

/* ── Types d'éléments ────────────────────────────────────────────── */
typedef enum { CL_ELEM_BTN, CL_ELEM_SEP, CL_ELEM_COMBO } ClElemType;

/* ── Layer ───────────────────────────────────────────────────────── */
typedef struct {
    char name     [64];
    int  visible;
    int  db_id;       /* PK SQLite                    */
    int  geom_type;   /* 0=topologique, 1=blob, ...   */
    int  style_id;    /* FK vers table styles         */
} ClLayer;

/* ── Combo ───────────────────────────────────────────────────────── */
typedef struct {
    int     id;
    int     width;
    int     x;
    int     current;
    int     hovered;
    int     open;
    ClLayer layers[CL_MAX_LAYERS];
    int     count;
    HWND    hwnd_drop;
} ClCombo;

/* ── Élément toolbar ─────────────────────────────────────────────── */
typedef struct {
    ClElemType  type;
    int         id;
    char        label  [24];
    char        tooltip[48];
    ClIconFn    icon_fn;
    RsvgHandle *rsvg;
    int         flags;
    int         pressed;
    int         hovered;
    int         arrow_hovered;
    int         x, w;
    int         combo_idx;
    int         mode;        /* mode courant (0 = défaut)              */
    int         mode_count;  /* nb de modes — 0 = bouton normal        */
} ClElem;

/* ── Pane statusbar ──────────────────────────────────────────────── */
typedef struct {
    int   type, width, computed_x, computed_w;
    char  text[128];
    float progress;      /* 0.0 = caché, 0.0..1.0 = barre active */
} ClPane;

/* ── Entrée menubar ──────────────────────────────────────────────── */
typedef struct {
    int   id;
    char  label[32];
    int   hovered;
    int   x, w;       /* calculés au rendu */
} ClMenuItem;

/* ── Structure principale ────────────────────────────────────────── */
struct CairoLayout_ {
    HWND   parent, canvas;
    HWND   hwnd_toolbar, hwnd_status, hwnd_menubar;

    ClElem  elems [CL_MAX_ELEMS];
    int     elem_count;

    ClCombo combos[CL_MAX_COMBOS];
    int     combo_count;

    CairoLayoutCb tb_cb;
    void         *tb_ud;

    ClPane  panes[CL_MAX_PANES];
    int     pane_count;

    /* Menubar */
    ClMenuItem    menu_items[CL_MAX_MENUITEMS];
    int           menu_count;
    int           menu_open;      /* index item ouvert (-1 = aucun)   */
    CairoLayoutCb menu_cb;
    void         *menu_ud;

    /* Tooltip popup */
    HWND    hwnd_tip;
    int     tip_elem;
    UINT_PTR tip_timer;

    /* Transcript + splitter */
    CairoTranscript *transcript;
    HWND             hwnd_splitter;
    int              transcript_h;
    int              transcript_vis;
    int              split_dragging;
    int              split_drag_y0;
    int              split_drag_h0;

    /* TabPane (partie basse du splitter — remplace le transcript direct) */
#if CL_HAS_TABPANE
    CairoTabPane    *tabpane;
    HWND             hwnd_ct_wrap;   /* conteneur transparent pour transcript */
    void           (*tabpane_close_cb)(int idx, HWND content, void *ud);
    void            *tabpane_close_ud;
#endif
};

/* ── Forward déclarations ────────────────────────────────────────── */
static LRESULT CALLBACK cl__toolbar_proc (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK cl__menubar_proc (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK cl__status_proc  (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK cl__drop_proc    (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK cl__tip_proc     (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK cl__splitter_proc(HWND, UINT, WPARAM, LPARAM);
static void             cl__tip_show     (CairoLayout *l, int elem_idx);
static void             cl__tip_hide     (CairoLayout *l);
static void             cl__do_resize    (CairoLayout *l);

/* ══════════════════════════════════════════════════════════════════
   HELPERS DESSIN
   ══════════════════════════════════════════════════════════════════ */

static void cl__rounded_rect(cairo_t *cr, double x, double y,
                              double w, double h, double r)
{
    if (r <= 0) { cairo_rectangle(cr, x, y, w, h); return; }
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -M_PI/2,  0);
    cairo_arc(cr, x+w-r, y+h-r, r,  0,        M_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r,  M_PI/2,   M_PI);
    cairo_arc(cr, x+r,   y+r,   r,  M_PI,    3*M_PI/2);
    cairo_close_path(cr);
}

static void cl__fill_bg(cairo_t *cr, int w, int h)
{
    cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, 0, h);
    cairo_pattern_add_color_stop_rgb(p, 0.0, 0.17, 0.18, 0.22);
    cairo_pattern_add_color_stop_rgb(p, 1.0, 0.13, 0.14, 0.18);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);
}

/* Icône œil ─────────────────────────────────────────────────────── */
static void cl__draw_eye(cairo_t *cr, double cx, double cy, int visible)
{
    cairo_set_line_width(cr, 1.4);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    if (visible) {
        cairo_move_to(cr, cx-7, cy);
        cairo_curve_to(cr, cx-4, cy-4, cx+4, cy-4, cx+7, cy);
        cairo_curve_to(cr, cx+4, cy+4, cx-4, cy+4, cx-7, cy);
        cl__col_text(cr, 0.75);
        cairo_stroke(cr);
        cairo_arc(cr, cx, cy, 2.5, 0, 2*M_PI);
        cl__col_accent(cr, 0.90);
        cairo_fill(cr);
    } else {
        cairo_move_to(cr, cx-7, cy);
        cairo_curve_to(cr, cx-4, cy-4, cx+4, cy-4, cx+7, cy);
        cairo_curve_to(cr, cx+4, cy+4, cx-4, cy+4, cx-7, cy);
        cl__col_textd(cr, 0.50);
        cairo_stroke(cr);
        cairo_move_to(cr, cx-5, cy-4);
        cairo_line_to(cr, cx+5, cy+4);
        cl__col_textd(cr, 0.50);
        cairo_stroke(cr);
    }
}

/* Flèche dropdown ───────────────────────────────────────────────── */
static void cl__draw_arrow_down(cairo_t *cr, double cx, double cy)
{
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, cx-4, cy-2);
    cairo_line_to(cr, cx,   cy+2);
    cairo_line_to(cr, cx+4, cy-2);
    cl__col_text(cr, 0.70);
    cairo_stroke(cr);
}

/* Rendu SVG centré dans un bouton toolbar ────────────────────────── */
static void cl__draw_svg_icon(cairo_t *cr, RsvgHandle *rsvg,
                               double bx, double by, double bw, double bh)
{
    if (!rsvg) return;

    /* Dimensions robustes via viewBox */
    gdouble svg_w = 24.0, svg_h = 24.0;
#if LIBRSVG_MAJOR_VERSION > 2 || \
    (LIBRSVG_MAJOR_VERSION == 2 && LIBRSVG_MINOR_VERSION >= 52)
    RsvgLength    wl = {0}, hl = {0};
    RsvgRectangle vb = {0};
    gboolean hw = FALSE, hh = FALSE, hv = FALSE;
    rsvg_handle_get_intrinsic_dimensions(rsvg, &hw, &wl, &hh, &hl, &hv, &vb);
    if (hv && vb.width > 0 && vb.height > 0) {
        svg_w = vb.width; svg_h = vb.height;
    } else if (hw && hh && wl.length > 0 && hl.length > 0) {
        svg_w = wl.length; svg_h = hl.length;
    }
#else
    RsvgDimensionData dim = {0};
    rsvg_handle_get_dimensions(rsvg, &dim);
    if (dim.width  > 0) svg_w = dim.width;
    if (dim.height > 0) svg_h = dim.height;
#endif
    if (svg_w <= 0 || !isfinite(svg_w)) svg_w = 24.0;
    if (svg_h <= 0 || !isfinite(svg_h)) svg_h = 24.0;

    /* Taille cible — laisser 4px de marge dans le bouton */
    double target = fmin(bw, bh) - 8.0;
    if (target < 4.0) target = 4.0;
    double scale = target / fmax(svg_w, svg_h);
    if (!isfinite(scale) || scale <= 0) scale = 1.0;

    /* Centrer dans la zone bouton */
    double ox = bx + (bw - svg_w * scale) / 2.0;
    double oy = by + (bh - svg_h * scale) / 2.0;

    cairo_save(cr);
    cairo_translate(cr, ox, oy);
    cairo_scale(cr, scale, scale);

#if LIBRSVG_MAJOR_VERSION > 2 || \
    (LIBRSVG_MAJOR_VERSION == 2 && LIBRSVG_MINOR_VERSION >= 52)
    RsvgRectangle vp = {0, 0, svg_w, svg_h};
    rsvg_handle_render_document(rsvg, cr, &vp, NULL);
#else
    rsvg_handle_render_cairo(rsvg, cr);
#endif
    cairo_restore(cr);
}



static void cl__draw_combo_field(cairo_t *cr, ClCombo *c, int h)
{
    double by = 4, bh = h - 8;
    double cx = c->x, cw = c->width;

    /* Fond champ */
    cl__rounded_rect(cr, cx, by, cw - 20, bh, 4);
    cl__col_bg2(cr, 1.0);
    cairo_fill_preserve(cr);
    cl__col_border(cr, c->open ? 0.9 : 0.6);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    /* Accent si ouvert */
    if (c->open) {
        cl__rounded_rect(cr, cx, by, cw - 20, bh, 4);
        cl__col_accent(cr, 0.20);
        cairo_fill(cr);
    }

    /* Texte layer courant */
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    const char *name = (c->count > 0 && c->current >= 0 && c->current < c->count)
                       ? c->layers[c->current].name : "—";
    cairo_save(cr);
    cairo_rectangle(cr, cx + 6, by, cw - 32, bh);
    cairo_clip(cr);
    cl__col_text(cr, 0.95);
    cairo_move_to(cr, cx + 6, by + bh * 0.72);
    cairo_show_text(cr, name);
    cairo_restore(cr);

    /* Bouton flèche */
    cl__rounded_rect(cr, cx + cw - 20, by, 20, bh, 4);
    cl__col_bg2(cr, 1.0);
    cairo_fill_preserve(cr);
    cl__col_border(cr, 0.5);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cl__draw_arrow_down(cr, cx + cw - 10, by + bh * 0.5);
}

/* ══════════════════════════════════════════════════════════════════
   DESSIN MENUBAR
   ══════════════════════════════════════════════════════════════════ */

static void cl__draw_menubar(CairoLayout *l, cairo_t *cr, int w, int h)
{

    /* Fond légèrement plus sombre que la toolbar */
    cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, 0, h);
    cairo_pattern_add_color_stop_rgb(p, 0.0, 0.09, 0.10, 0.14);
    cairo_pattern_add_color_stop_rgb(p, 1.0, 0.07, 0.08, 0.11);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);

    /* Ligne basse */
    cl__col_border(cr, 0.6);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, h - 0.5);
    cairo_line_to(cr, w, h - 0.5);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.5);

    /* Calcul largeurs et positions */
    int bx = 4;
    for (int i = 0; i < l->menu_count; i++) {
        ClMenuItem *mi = &l->menu_items[i];
        cairo_text_extents_t te;
        cairo_text_extents(cr, mi->label, &te);
        mi->w = (int)te.width + 20;
        mi->x = bx;
        bx   += mi->w;
    }

    /* Dessin des entrées */
    for (int i = 0; i < l->menu_count; i++) {
        ClMenuItem *mi = &l->menu_items[i];
        double cy = h / 2.0;

        /* Fond hover ou ouvert */
        if (mi->hovered || i == l->menu_open) {
            cl__rounded_rect(cr, mi->x + 1, 2, mi->w - 2, h - 4, 3);
            if (i == l->menu_open)
                cl__col_press(cr, 0.55);
            else
                cl__col_hover(cr, 0.22);
            cairo_fill(cr);
        }

        /* Label */
        cairo_text_extents_t te;
        cairo_text_extents(cr, mi->label, &te);
        double tx = mi->x + (mi->w - te.width) / 2.0 - te.x_bearing;
        cl__col_text(cr, mi->hovered || i == l->menu_open ? 1.0 : 0.85);
        cairo_move_to(cr, tx, cy - te.height/2.0 - te.y_bearing);
        cairo_show_text(cr, mi->label);
    }
}

/* ── WndProc menubar ─────────────────────────────────────────────── */
static LRESULT CALLBACK cl__menubar_proc(HWND hwnd, UINT msg,
                                          WPARAM wp, LPARAM lp)
{
    CairoLayout *l = ui_get_data(CairoLayout, hwnd);
    if (!l) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            cl__draw_menubar(l, ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp);
        int changed = 0;
        for (int i = 0; i < l->menu_count; i++) {
            ClMenuItem *mi = &l->menu_items[i];
            int was = mi->hovered;
            mi->hovered = (mx >= mi->x && mx < mi->x + mi->w) ? 1 : 0;
            if (mi->hovered != was) changed = 1;
            /* Si un menu est déjà ouvert et qu'on survole une autre entrée */
            if (mi->hovered && l->menu_open >= 0 && l->menu_open != i) {
                l->menu_open = i;
                if (l->menu_cb) l->menu_cb(mi->id, l->menu_ud);
                changed = 1;
            }
        }
        if (changed) ui_redraw(hwnd);
        TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        for (int i = 0; i < l->menu_count; i++)
            l->menu_items[i].hovered = 0;
        ui_redraw(hwnd);
        return 0;

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp);
        for (int i = 0; i < l->menu_count; i++) {
            ClMenuItem *mi = &l->menu_items[i];
            if (mx >= mi->x && mx < mi->x + mi->w) {
                if (l->menu_open == i) {
                    /* Re-clic sur l'entrée ouverte → fermer */
                    l->menu_open = -1;
                } else {
                    l->menu_open = i;
                    if (l->menu_cb) l->menu_cb(mi->id, l->menu_ud);
                }
                ui_redraw(hwnd);
                return 0;
            }
        }
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void cl__draw_toolbar(CairoLayout *l, cairo_t *cr, int w, int h)
{

    cl__fill_bg(cr, w, h);

    /* Ligne basse */
    cl__col_border(cr, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, h - 0.5);
    cairo_line_to(cr, w, h - 0.5);
    cairo_stroke(cr);

    /* Calcul positions */
    int bx = 4;
    for (int i = 0; i < l->elem_count; i++) {
        ClElem *e = &l->elems[i];
        switch (e->type) {
        case CL_ELEM_SEP:
            e->x = bx; e->w = CL_BTN_SEP_W; bx += CL_BTN_SEP_W;
            break;
        case CL_ELEM_COMBO: {
            ClCombo *c = &l->combos[e->combo_idx];
            e->x = bx; e->w = c->width; c->x = bx;
            bx += c->width + 2;
            break;
        }
        default:
            e->x = bx;
            e->w = (e->flags & CL_BTN_SPLIT) ? CL_BTN_W + CL_BTN_ARROW_W : CL_BTN_W;
            bx += e->w + 2;
            break;
        }
    }

    /* Dessin des éléments */
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    for (int i = 0; i < l->elem_count; i++) {
        ClElem *e = &l->elems[i];

        if (e->type == CL_ELEM_SEP) {
            double sx = e->x + CL_BTN_SEP_W / 2.0;
            cl__col_sep(cr, 0.8);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, sx, 5); cairo_line_to(cr, sx, h-5);
            cairo_stroke(cr);
            continue;
        }

        if (e->type == CL_ELEM_COMBO) {
            cl__draw_combo_field(cr, &l->combos[e->combo_idx], h);
            continue;
        }

        /* Bouton */
        int    disabled = e->flags & CL_BTN_DISABLED;
        double by  = 3, bh = h - 6;
        double bcx = e->x + e->w / 2.0;
        double bcy = by + bh / 2.0;

        /* Disabled — dessiner dans un groupe puis appliquer l'alpha */
        if (disabled) cairo_push_group(cr);

        if (e->pressed) {
            cl__rounded_rect(cr, e->x, by, e->w, bh, 4);
            cl__col_press(cr, 0.85);
            cairo_fill(cr);
        } else if (e->hovered && !disabled) {
            cl__rounded_rect(cr, e->x, by, e->w, bh, 4);
            cl__col_hover(cr, 0.25);
            cairo_fill(cr);
        }

        if (e->rsvg) {
            /* Icône SVG */
            int   icon_w = (e->flags & CL_BTN_SPLIT) ? e->w - CL_BTN_ARROW_W : e->w;
            cl__draw_svg_icon(cr, e->rsvg, e->x, by, icon_w, bh);
            /* Tooltip */
            if (e->hovered && e->tooltip[0]) {
                cairo_set_font_size(cr, 9.5);
                cairo_text_extents_t te;
                cairo_text_extents(cr, e->tooltip, &te);
                double tx = bcx - te.width/2.0 - te.x_bearing;
                cl__col_bg2(cr, 0.92);
                cairo_rectangle(cr, tx-4, by+bh+3, te.width+8, 14);
                cairo_fill(cr);
                cl__col_text(cr, 1.0);
                cairo_move_to(cr, tx, by+bh+13);
                cairo_show_text(cr, e->tooltip);
            }
        } else if (e->icon_fn) {
            cairo_save(cr);
            if (e->flags & CL_BTN_SPLIT) {
                /* Zone icône = tout sauf la flèche */
                cairo_rectangle(cr, e->x+2, by+2, e->w - CL_BTN_ARROW_W - 4, bh-4);
            } else {
                cairo_rectangle(cr, e->x+2, by+2, e->w-4, bh-4);
            }
            cairo_clip(cr);
            double icon_cx = (e->flags & CL_BTN_SPLIT)
                ? e->x + (e->w - CL_BTN_ARROW_W) / 2.0
                : bcx;
            e->icon_fn(cr, icon_cx, bcy, e->mode);
            cairo_restore(cr);
            /* Tooltip */
            if (e->hovered && e->tooltip[0]) {
                cairo_set_font_size(cr, 9.5);
                cairo_text_extents_t te;
                cairo_text_extents(cr, e->tooltip, &te);
                double tx = bcx - te.width/2.0 - te.x_bearing;
                cl__col_bg2(cr, 0.92);
                cairo_rectangle(cr, tx-4, by+bh+3, te.width+8, 14);
                cairo_fill(cr);
                cl__col_text(cr, 1.0);
                cairo_move_to(cr, tx, by+bh+13);
                cairo_show_text(cr, e->tooltip);
            }
        } else {
            int   label_w = (e->flags & CL_BTN_SPLIT) ? e->w - CL_BTN_ARROW_W : e->w;
            double label_cx = e->x + label_w / 2.0;
            cairo_set_font_size(cr, 11.0);
            cairo_text_extents_t te;
            cairo_text_extents(cr, e->label, &te);
            double tx = label_cx - te.width/2.0 - te.x_bearing;
            double ty = bcy - te.height/2.0 - te.y_bearing;
            if (disabled) cl__col_textd(cr, 1.0);
            else          cl__col_text (cr, 1.0);
            cairo_move_to(cr, tx, ty);
            cairo_show_text(cr, e->label);
        }

        /* Zone flèche split button */
        if (e->flags & CL_BTN_SPLIT) {
            double ax = e->x + e->w - CL_BTN_ARROW_W;
            /* Séparateur vertical */
            cl__col_sep(cr, 0.8);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, ax + 0.5, by + 4);
            cairo_line_to(cr, ax + 0.5, by + bh - 4);
            cairo_stroke(cr);
            /* Fond hover flèche */
            if (e->arrow_hovered && !disabled) {
                cl__rounded_rect(cr, ax + 1, by, CL_BTN_ARROW_W - 1, bh, 4);
                cl__col_hover(cr, 0.30);
                cairo_fill(cr);
            }
            /* Flèche ▼ */
            cl__draw_arrow_down(cr, ax + CL_BTN_ARROW_W / 2.0, bcy);
        }

        /* Fin groupe disabled — appliquer transparence */
        if (disabled) {
            cairo_pop_group_to_source(cr);
            cairo_paint_with_alpha(cr, 0.32);
        }
    }

}

/* ══════════════════════════════════════════════════════════════════
   DESSIN DROPDOWN
   ══════════════════════════════════════════════════════════════════ */

static void cl__draw_dropdown(ClCombo *c, cairo_t *cr, int w, int h)
{

    /* Fond + bordure */
    cl__rounded_rect(cr, 0, 0, w, h, 5);
    cl__col_bg2(cr, 0.97);
    cairo_fill_preserve(cr);
    cl__col_border(cr, 0.9);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    /* Ombre haute */
    cairo_pattern_t *shad = cairo_pattern_create_linear(0, 0, 0, 8);
    cairo_pattern_add_color_stop_rgba(shad, 0, 0, 0, 0, 0.25);
    cairo_pattern_add_color_stop_rgba(shad, 1, 0, 0, 0, 0.00);
    cairo_rectangle(cr, 1, 1, w-2, 8);
    cairo_set_source(cr, shad);
    cairo_fill(cr);
    cairo_pattern_destroy(shad);

    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.5);

    for (int i = 0; i < c->count; i++) {
        double iy = i * CL_DROP_ITEM_H + 4;
        double ih = CL_DROP_ITEM_H - 2;

        if (i == c->current) {
            cl__rounded_rect(cr, 3, iy, w-6, ih, 4);
            cl__col_press(cr, 0.55);
            cairo_fill(cr);
        } else if (i == c->hovered) {
            cl__rounded_rect(cr, 3, iy, w-6, ih, 4);
            cl__col_hover(cr, 0.22);
            cairo_fill(cr);
        }

        /* Icône œil */
        cl__draw_eye(cr, CL_DROP_EYE_W / 2.0, iy + ih/2.0,
                     c->layers[i].visible);

        /* Séparateur œil / nom */
        cl__col_border(cr, 0.4);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, CL_DROP_EYE_W + 0.5, iy + 4);
        cairo_line_to(cr, CL_DROP_EYE_W + 0.5, iy + ih - 4);
        cairo_stroke(cr);

        /* Nom */
        cairo_save(cr);
        cairo_rectangle(cr, CL_DROP_EYE_W + 8, iy, w - CL_DROP_EYE_W - 12, ih);
        cairo_clip(cr);
        if (c->layers[i].visible) cl__col_text (cr, 1.0);
        else                      cl__col_textd(cr, 0.8);
        cairo_move_to(cr, CL_DROP_EYE_W + 10, iy + ih * 0.72);
        cairo_show_text(cr, c->layers[i].name);
        cairo_restore(cr);
    }

}

/* ══════════════════════════════════════════════════════════════════
   DESSIN STATUSBAR
   ══════════════════════════════════════════════════════════════════ */

static void cl__draw_status(CairoLayout *l, cairo_t *cr, int w, int h)
{

    cairo_pattern_t *p = cairo_pattern_create_linear(0, 0, 0, h);
    cairo_pattern_add_color_stop_rgb(p, 0.0, 0.10, 0.11, 0.15);
    cairo_pattern_add_color_stop_rgb(p, 1.0, 0.08, 0.09, 0.12);
    cairo_rectangle(cr, 0, 0, w, h);
    cairo_set_source(cr, p);
    cairo_fill(cr);
    cairo_pattern_destroy(p);

    cl__col_border(cr, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, 0.5); cairo_line_to(cr, w, 0.5);
    cairo_stroke(cr);

    /* Calcul panes */
    int spring_count = 0, fixed_total = 0;
    for (int i = 0; i < l->pane_count; i++) {
        if (l->panes[i].type == CL_PANE_SPRING) spring_count++;
        else fixed_total += l->panes[i].width;
    }
    int spring_w = spring_count > 0
        ? (w - fixed_total - (l->pane_count - 1)) / spring_count : 0;

    int px = 0;
    for (int i = 0; i < l->pane_count; i++) {
        l->panes[i].computed_x = px;
        l->panes[i].computed_w = (l->panes[i].type == CL_PANE_SPRING)
                                 ? spring_w : l->panes[i].width;
        px += l->panes[i].computed_w + 1;
    }

    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.5);

    for (int i = 0; i < l->pane_count; i++) {
        ClPane *pn = &l->panes[i];
        if (i > 0) {
            cl__col_border(cr, 0.7);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, pn->computed_x - 0.5, 3);
            cairo_line_to(cr, pn->computed_x - 0.5, h - 3);
            cairo_stroke(cr);
        }

        /* Progressbar — dessinée sous le texte */
        if (pn->progress > 0.0f) {
            double pw = (pn->computed_w - 8) * (double)pn->progress;
            /* Fond barre */
            cl__rounded_rect(cr, pn->computed_x + 4, 2, pn->computed_w - 8, h - 4, 2);
            cairo_set_source_rgba(cr, 0.20, 0.22, 0.30, 1.0);
            cairo_fill(cr);
            /* Barre de progression */
            if (pw > 0) {
                cl__rounded_rect(cr, pn->computed_x + 4, 2, pw, h - 4, 2);
                cairo_pattern_t *pg = cairo_pattern_create_linear(
                    pn->computed_x + 4, 0,
                    pn->computed_x + 4 + pw, 0);
                cairo_pattern_add_color_stop_rgb(pg, 0.0, 0.20, 0.50, 0.90);
                cairo_pattern_add_color_stop_rgb(pg, 1.0, 0.30, 0.70, 1.00);
                cairo_set_source(cr, pg);
                cairo_fill(cr);
                cairo_pattern_destroy(pg);
            }
        }

        /* Texte */
        cairo_save(cr);
        cairo_rectangle(cr, pn->computed_x + 4, 0, pn->computed_w - 8, h);
        cairo_clip(cr);
        if (pn->progress > 0.0f)
            cl__col_text(cr, 1.0);   /* texte blanc sur barre */
        else
            cl__col_text(cr, 0.85);
        cairo_move_to(cr, pn->computed_x + 6, h * 0.72);
        cairo_show_text(cr, pn->text);
        cairo_restore(cr);
    }

}

/* ══════════════════════════════════════════════════════════════════
   TOOLTIP POPUP
   ══════════════════════════════════════════════════════════════════ */

static void cl__tip_draw(const char *text, cairo_t *cr, int w, int h)
{

    /* Fond + bordure */
    cl__rounded_rect(cr, 0.5, 0.5, w-1, h-1, 4);
    cl__col_bg2(cr, 0.97);
    cairo_fill_preserve(cr);
    cl__col_accent(cr, 0.55);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    /* Texte */
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    cl__col_text(cr, 0.95);
    cairo_move_to(cr, 8, h * 0.72);
    cairo_show_text(cr, text);

}

static LRESULT CALLBACK cl__tip_proc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    (void)wp; (void)lp;
    switch (msg) {
    case WM_PAINT: {
        const char *text = ui_get_data(const char, hwnd);
        UI_PAINT_BEGIN(hwnd, ctx);
            cl__tip_draw(text ? text : "", ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

static void cl__tip_show(CairoLayout *l, int elem_idx)
{
    if (elem_idx < 0 || elem_idx >= l->elem_count) return;
    ClElem *e = &l->elems[elem_idx];
    if (!e->tooltip[0]) return;
    if (l->tip_elem == elem_idx && l->hwnd_tip) return;

    cl__tip_hide(l);

    /* Mesurer le texte */
    /* Mesure du texte via UiDrawCtx (GetDC temporaire) */
    HDC hdc = GetDC(l->hwnd_toolbar);
    UiDrawCtx mctx; ui_draw_begin(&mctx, hdc, 0, 0);
    cairo_select_font_face(mctx.cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(mctx.cr, 11.0);
    cairo_text_extents_t te;
    cairo_text_extents(mctx.cr, e->tooltip, &te);
    ui_draw_end(&mctx);
    ReleaseDC(l->hwnd_toolbar, hdc);

    int tw = (int)te.width  + 18;
    int th = (int)te.height + 10;

    /* Position : centré sous le bouton, en coordonnées écran */
    POINT pt = { e->x + e->w/2, CL_MENUBAR_H + CL_TOOLBAR_H };
    ClientToScreen(l->hwnd_toolbar, &pt);
    int tx = pt.x - tw/2;
    int ty = pt.y + 4;


    static int reg = 0;
    if (!reg) {
        reg = 1;
        ui_register_class("CL_Tooltip", cl__tip_proc, 0, NULL);
    }

    /* Stocker le pointeur texte — valide tant que ClElem existe */
    l->hwnd_tip = ui_popup_create("CL_Tooltip", l->parent,
        tx, ty, tw, th,
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        (void*)e->tooltip);
    ShowWindow(l->hwnd_tip, SW_SHOWNOACTIVATE);
    l->tip_elem = elem_idx;
}

static void cl__tip_hide(CairoLayout *l)
{
    if (l->hwnd_tip) {
        DestroyWindow(l->hwnd_tip);
        l->hwnd_tip = NULL;
    }
    if (l->tip_timer) {
        KillTimer(l->hwnd_toolbar, l->tip_timer);
        l->tip_timer = 0;
    }
    l->tip_elem = -1;
}

/* ══════════════════════════════════════════════════════════════════
   WNDPROC DROPDOWN
   ══════════════════════════════════════════════════════════════════ */

static LRESULT CALLBACK cl__drop_proc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp)
{
    ClCombo     *c = (ClCombo*)    GetWindowLongPtr(hwnd, GWLP_USERDATA);
    CairoLayout *l = (CairoLayout*)GetProp(hwnd, "cl_layout");
    (void)wp;

    switch (msg) {
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            cl__draw_dropdown(c, ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int my = GET_Y_LPARAM(lp);
        int prev = c->hovered;
        c->hovered = -1;
        for (int i = 0; i < c->count; i++) {
            int iy = i * CL_DROP_ITEM_H + 4;
            if (my >= iy && my < iy + CL_DROP_ITEM_H - 2)
                c->hovered = i;
        }
        if (c->hovered != prev) ui_redraw(hwnd);
        return 0;
    }
    case WM_MOUSEACTIVATE:
        /* Empêcher le dropdown de voler le focus sur clic —
         * sinon WM_KILLFOCUS arrive avant WM_LBUTTONUP */
        return MA_NOACTIVATE;
    case WM_LBUTTONUP: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        for (int i = 0; i < c->count; i++) {
            int iy = i * CL_DROP_ITEM_H + 4;
            if (my >= iy && my < iy + CL_DROP_ITEM_H - 2) {
                if (mx < CL_DROP_EYE_W) {
                    /* Toggle visibilité */
                    c->layers[i].visible = !c->layers[i].visible;
                    if (l && l->tb_cb)
                        l->tb_cb(CL_ID_EYE_BASE + i, l->tb_ud);
                    /* Ne pas fermer : on reste ouvert pour multi-toggle */
                    ui_redraw(hwnd);
                    if (l) InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
                } else {
                    /* Sélection layer → ferme le dropdown */
                    c->current = i;
                    if (l && l->tb_cb) l->tb_cb(c->id, l->tb_ud);
                    c->open = 0;
                    DestroyWindow(hwnd);
                    c->hwnd_drop = NULL;
                    if (l) {
                        InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
                        UpdateWindow(l->hwnd_toolbar);
                    }
                }
                break;
            }
        }
        return 0;
    }
    case WM_KILLFOCUS:
        c->open = 0;
        DestroyWindow(hwnd);
        c->hwnd_drop = NULL;
        if (l) InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ══════════════════════════════════════════════════════════════════
   OUVERTURE DROPDOWN
   ══════════════════════════════════════════════════════════════════ */

static void cl__open_dropdown(CairoLayout *l, ClCombo *c)
{
    if (c->hwnd_drop) { DestroyWindow(c->hwnd_drop); c->hwnd_drop = NULL; }

    int drop_h = c->count * CL_DROP_ITEM_H + 8;
    int drop_w = c->width;

    /* Position écran : sous la toolbar */
    POINT pt = {c->x, 0};
    ClientToScreen(l->hwnd_toolbar, &pt);
    RECT trc; GetWindowRect(l->hwnd_toolbar, &trc);


    static int reg = 0;
    if (!reg) {
        reg = 1;
        WNDCLASSEX wc = {0};
        wc.cbSize        = sizeof wc;
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        ui_register_class("CL_Dropdown", cl__drop_proc, 0, NULL);
    }

    c->hwnd_drop = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        "CL_Dropdown", NULL,
        WS_POPUP | WS_VISIBLE,
        pt.x, trc.bottom, drop_w, drop_h,
        l->parent, NULL, GetModuleHandle(NULL), NULL);
    SetWindowLongPtr(c->hwnd_drop, GWLP_USERDATA, (LONG_PTR)c);
    SetProp(c->hwnd_drop, "cl_layout", (HANDLE)l);
    c->hovered = -1;
    c->open    = 1;
    SetFocus(c->hwnd_drop);
}

/* ══════════════════════════════════════════════════════════════════
   WNDPROC TOOLBAR
   ══════════════════════════════════════════════════════════════════ */

static LRESULT CALLBACK cl__toolbar_proc(HWND hwnd, UINT msg,
                                          WPARAM wp, LPARAM lp)
{
    CairoLayout *l = ui_get_data(CairoLayout, hwnd);
    if (!l) return DefWindowProc(hwnd, msg, wp, lp);
    (void)wp;

    switch (msg) {
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            cl__draw_toolbar(l, ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp);
        int hovered_elem = -1;
        for (int i = 0; i < l->elem_count; i++) {
            ClElem *e = &l->elems[i];
            if (e->type != CL_ELEM_SEP) {
                int was       = e->hovered;
                int was_arrow = e->arrow_hovered;
                int in_btn    = (mx >= e->x && mx < e->x + e->w);
                e->hovered = in_btn ? 1 : 0;
                if ((e->flags & CL_BTN_SPLIT) && in_btn)
                    e->arrow_hovered = (mx >= e->x + e->w - CL_BTN_ARROW_W) ? 1 : 0;
                else
                    e->arrow_hovered = 0;
                if (e->hovered != was || e->arrow_hovered != was_arrow)
                    ui_redraw(hwnd);
                if (e->hovered) hovered_elem = i;
            }
        }
        /* Tooltip — délai 600ms */
        if (hovered_elem != l->tip_elem) {
            cl__tip_hide(l);
            if (hovered_elem >= 0 && l->elems[hovered_elem].tooltip[0]) {
                l->tip_elem  = hovered_elem;
                l->tip_timer = SetTimer(hwnd, 1, 600, NULL);
            }
        }
        TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }
    case WM_MOUSELEAVE:
        for (int i = 0; i < l->elem_count; i++) l->elems[i].hovered = 0;
        cl__tip_hide(l);
        ui_redraw(hwnd);
        return 0;
    case WM_TIMER:
        if (wp == 1) {
            KillTimer(hwnd, 1);
            l->tip_timer = 0;
            cl__tip_show(l, l->tip_elem);
        }
        return 0;
    case WM_LBUTTONDOWN: {
        cl__tip_hide(l);
        int mx = GET_X_LPARAM(lp);
        for (int i = 0; i < l->elem_count; i++) {
            ClElem *e = &l->elems[i];
            if (e->type == CL_ELEM_SEP) continue;
            if (mx < e->x || mx >= e->x + e->w) continue;

            if (e->type == CL_ELEM_COMBO) {
                ClCombo *c = &l->combos[e->combo_idx];
                if (c->open) {
                    if (c->hwnd_drop) DestroyWindow(c->hwnd_drop);
                    c->hwnd_drop = NULL; c->open = 0;
                } else {
                    cl__open_dropdown(l, c);
                }
                ui_redraw(hwnd);
                return 0;
            }

            if (e->flags & CL_BTN_DISABLED) continue;

            /* Split button : deux zones */
            if (e->flags & CL_BTN_SPLIT) {
                int arrow_x = e->x + e->w - CL_BTN_ARROW_W;
                if (mx >= arrow_x) {
                    if (e->mode_count > 0) {
                        /* Multimode — cycling du mode */
                        e->mode = (e->mode + 1) % e->mode_count;
                        ui_redraw(hwnd);
                        //if (l->tb_cb) l->tb_cb(e->id, l->tb_ud);
                    } else {
                        /* Split normal → CL_ID_SPLIT_BASE + id */
                        if (l->tb_cb) l->tb_cb(CL_ID_SPLIT_BASE + e->id, l->tb_ud);
                    }
                } else {
                    /* Clic bouton principal → id normal */
                    if (e->flags & CL_BTN_TOGGLE) e->pressed = !e->pressed;
                    else e->pressed = 1;
                    ui_redraw(hwnd);
                    if (l->tb_cb) l->tb_cb(e->id, l->tb_ud);
                }
                return 0;
            }

            if (e->flags & CL_BTN_TOGGLE) e->pressed = !e->pressed;
            else e->pressed = 1;
            ui_redraw(hwnd);
            if (l->tb_cb) l->tb_cb(e->id, l->tb_ud);
        }
        return 0;
    }
    case WM_LBUTTONUP:
        for (int i = 0; i < l->elem_count; i++) {
            ClElem *e = &l->elems[i];
            if (e->type == CL_ELEM_BTN && !(e->flags & CL_BTN_TOGGLE))
                e->pressed = 0;
        }
        ui_redraw(hwnd);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ══════════════════════════════════════════════════════════════════
   WNDPROC STATUSBAR
   ══════════════════════════════════════════════════════════════════ */

static LRESULT CALLBACK cl__status_proc(HWND hwnd, UINT msg,
                                         WPARAM wp, LPARAM lp)
{
    CairoLayout *l = ui_get_data(CairoLayout, hwnd);
    if (!l) return DefWindowProc(hwnd, msg, wp, lp);
    (void)wp; (void)lp;
    switch (msg) {
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            cl__draw_status(l, ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ══════════════════════════════════════════════════════════════════
   WNDPROC SPLITTER
   ══════════════════════════════════════════════════════════════════ */

static void cl__draw_splitter(cairo_t *cr, int w, int h)
{

    /* Fond */
    cairo_set_source_rgb(cr, 0.08, 0.09, 0.12);
    cairo_paint(cr);

    /* Ligne haute et basse */
    cairo_set_source_rgba(cr, 0.22, 0.25, 0.35, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, 0.5);   cairo_line_to(cr, w, 0.5);
    cairo_move_to(cr, 0, h-0.5); cairo_line_to(cr, w, h-0.5);
    cairo_stroke(cr);

    /* Poignée centrale — 3 points */
    double cx = w / 2.0;
    double cy = h / 2.0;
    cairo_set_source_rgba(cr, 0.35, 0.40, 0.55, 0.9);
    for (int i = -1; i <= 1; i++) {
        cairo_arc(cr, cx + i * 6, cy, 1.5, 0, 2*M_PI);
        cairo_fill(cr);
    }

}

static LRESULT CALLBACK cl__splitter_proc(HWND hwnd, UINT msg,
                                           WPARAM wp, LPARAM lp)
{
    CairoLayout *l = ui_get_data(CairoLayout, hwnd);
    if (!l) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            cl__draw_splitter(ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursor(NULL, IDC_SIZENS));
        return TRUE;

    case WM_LBUTTONDOWN: {
        l->split_dragging  = 1;
        l->split_drag_y0   = GET_Y_LPARAM(lp);
        l->split_drag_h0   = l->transcript_h;
        /* Convertir en coordonnées parent */
        POINT pt = {0, GET_Y_LPARAM(lp)};
        ClientToScreen(hwnd, &pt);
        ScreenToClient(l->parent, &pt);
        l->split_drag_y0 = pt.y;
        SetCapture(hwnd);
        return 0;
    }
    case WM_MOUSEMOVE:
        if (l->split_dragging) {
            POINT pt = {0, GET_Y_LPARAM(lp)};
            ClientToScreen(hwnd, &pt);
            ScreenToClient(l->parent, &pt);
            int dy = l->split_drag_y0 - pt.y;   /* vers haut = agrandir */
            l->transcript_h = l->split_drag_h0 + dy;
            cl__do_resize(l);
            InvalidateRect(l->canvas, NULL, FALSE);
        }
        return 0;
    case WM_LBUTTONUP:
        if (l->split_dragging) {
            l->split_dragging = 0;
            ReleaseCapture();
        }
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ══════════════════════════════════════════════════════════════════
   ENREGISTREMENT CLASSES
   ══════════════════════════════════════════════════════════════════ */

/* ── Conteneur transparent pour le transcript sous le tabpane ──────
 * Ce HWND regroupe hwnd_log + hwnd_input du CairoTranscript.
 * tp_tab_set_content gère ce HWND (show/hide/MoveWindow).
 * Quand il reçoit WM_SIZE, il relaye à ct_resize via le layout.    */
#if CL_HAS_TABPANE && CL_HAS_TRANSCRIPT
static LRESULT CALLBACK cl__ct_wrap_proc(HWND hwnd, UINT msg,
                                          WPARAM wp, LPARAM lp)
{
    CairoLayout *l = ui_get_data(CairoLayout, hwnd);

    switch (msg) {
    case WM_SIZE: {
        /* Relayer au transcript : repositionner ses deux HWNDs
         * dans les nouvelles dimensions du wrap.                    */
        if (l && l->transcript) {
            RECT rc; GetClientRect(hwnd, &rc);
            ct_resize(l->transcript, 0, 0, rc.right, rc.bottom);
        }
        return 0;
    }
    case WM_CTLCOLOREDIT: {
        /* L'EDIT du transcript est enfant du wrap — il remonte
         * WM_CTLCOLOREDIT ici. On le relaye à ct_on_ctlcolor.      */
        if (l && l->transcript)
            return ct_on_ctlcolor(l->transcript, (HDC)wp, (HWND)lp);
        return (LRESULT)GetStockObject(BLACK_BRUSH);
    }
    case WM_ERASEBKGND:
        return 1;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}
#endif

static void cl__register_classes(void)
{
    static int done = 0;
    if (done) return; done = 1;

    ui_register_class("CL_Toolbar",  cl__toolbar_proc,  0, NULL);
    ui_register_class("CL_Menubar",  cl__menubar_proc,  0, NULL);
    ui_register_class("CL_Status",   cl__status_proc,   0, NULL);
    ui_register_class("CL_Splitter", cl__splitter_proc, 0,
                       LoadCursor(NULL, IDC_SIZENS));
#if CL_HAS_TABPANE && CL_HAS_TRANSCRIPT
    ui_register_class("BIM_CtWrap",  cl__ct_wrap_proc,  0, NULL);
#endif
}

/* ══════════════════════════════════════════════════════════════════
   API PUBLIQUE — IMPLÉMENTATION
   ══════════════════════════════════════════════════════════════════ */

CairoLayout *cl_create(HWND parent, HWND canvas)
{
    CairoLayout *l = (CairoLayout*)calloc(1, sizeof(CairoLayout));
    l->parent = parent;
    l->canvas = canvas;

    cl__register_classes();

    RECT rc; GetClientRect(parent, &rc);
    int w = rc.right;

    l->hwnd_menubar = ui_subwnd_create("CL_Menubar", parent, 0, 0, w, CL_MENUBAR_H, l);

    l->hwnd_toolbar = ui_subwnd_create("CL_Toolbar", parent, 0, CL_MENUBAR_H, w, CL_TOOLBAR_H, l);

    l->hwnd_status = ui_subwnd_create("CL_Status", parent, 0, rc.bottom - CL_STATUS_H, w, CL_STATUS_H, l);

    cl_resize(l);
    l->tip_elem = -1;   /* pas de tooltip actif */
    return l;
}

void cl_destroy(CairoLayout *l)
{
    if (!l) return;
    cl__tip_hide(l);
    for (int i = 0; i < l->combo_count; i++)
        if (l->combos[i].hwnd_drop) DestroyWindow(l->combos[i].hwnd_drop);
#if CL_HAS_TABPANE
    if (l->tabpane) tp_destroy(l->tabpane);
#endif
    if (l->hwnd_splitter) DestroyWindow(l->hwnd_splitter);
    if (l->hwnd_menubar)  DestroyWindow(l->hwnd_menubar);
    if (l->hwnd_toolbar)  DestroyWindow(l->hwnd_toolbar);
    if (l->hwnd_status)   DestroyWindow(l->hwnd_status);
    free(l);
}

#define CL_SPLITTER_H  5   /* épaisseur bande splitter px */

static void cl__do_resize(CairoLayout *l)
{
    RECT rc; GetClientRect(l->parent, &rc);
    int w = rc.right, h = rc.bottom;
    int top    = CL_MENUBAR_H + CL_TOOLBAR_H;
    int bottom = h - CL_STATUS_H;
    int avail  = bottom - top;
    if (avail < 0) avail = 0;

    /* ── Fenêtres fixes — toujours aux mêmes positions ── */
    if (l->hwnd_menubar)
        SetWindowPos(l->hwnd_menubar, NULL, 0, 0,            w, CL_MENUBAR_H, SWP_NOZORDER);
    SetWindowPos(l->hwnd_toolbar,     NULL, 0, CL_MENUBAR_H, w, CL_TOOLBAR_H, SWP_NOZORDER);
    SetWindowPos(l->hwnd_status,      NULL, 0, bottom,       w, CL_STATUS_H,  SWP_NOZORDER);

    /* ── Canvas + transcript — variables ── */
    if (l->transcript && l->transcript_vis && l->hwnd_splitter) {
        int min_tc = 60;
        int max_tc = avail - 40 - CL_SPLITTER_H;
        if (max_tc < min_tc) max_tc = min_tc;
        if (l->transcript_h < min_tc) l->transcript_h = min_tc;
        if (l->transcript_h > max_tc) l->transcript_h = max_tc;

        int canvas_h     = avail - CL_SPLITTER_H - l->transcript_h;
        int splitter_y   = top + canvas_h;
        int transcript_y = splitter_y + CL_SPLITTER_H;

        SetWindowPos(l->canvas,        NULL, 0, top,        w, canvas_h,      SWP_NOZORDER);
        SetWindowPos(l->hwnd_splitter, NULL, 0, splitter_y, w, CL_SPLITTER_H, SWP_NOZORDER | SWP_SHOWWINDOW);

#if CL_HAS_TABPANE
        /* TabPane présent : il gère le transcript en Tab 0 */
        if (l->tabpane) {
            /* S'assurer que le tabpane est au-dessus du canvas */
            SetWindowPos(l->tabpane->hwnd, HWND_TOP,
                         0, transcript_y, w, l->transcript_h,
                         SWP_SHOWWINDOW);
        }
        #if CL_HAS_TRANSCRIPT
        else {
            ct_resize(l->transcript, 0, transcript_y, w, l->transcript_h);
            ShowWindow(((struct CairoTranscript_ *)l->transcript)->hwnd_log,   SW_SHOW);
            ShowWindow(((struct CairoTranscript_ *)l->transcript)->hwnd_input, SW_SHOW);
        }
        #endif
#elif CL_HAS_TRANSCRIPT
        ct_resize(l->transcript, 0, transcript_y, w, l->transcript_h);
        ShowWindow(((struct CairoTranscript_ *)l->transcript)->hwnd_log,   SW_SHOW);
        ShowWindow(((struct CairoTranscript_ *)l->transcript)->hwnd_input, SW_SHOW);
#endif

    } else {
        SetWindowPos(l->canvas, NULL, 0, top, w, avail, SWP_NOZORDER);

        if (l->hwnd_splitter)
            ShowWindow(l->hwnd_splitter, SW_HIDE);

#if CL_HAS_TABPANE
        if (l->tabpane)
            SetWindowPos(l->tabpane->hwnd, NULL,
                         0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW);
#elif CL_HAS_TRANSCRIPT
        if (l->transcript) {
            ShowWindow(((struct CairoTranscript_ *)l->transcript)->hwnd_log,   SW_HIDE);
            ShowWindow(((struct CairoTranscript_ *)l->transcript)->hwnd_input, SW_HIDE);
        }
#endif
    }

    InvalidateRect(l->hwnd_menubar, NULL, FALSE);
    InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
    InvalidateRect(l->hwnd_status,  NULL, FALSE);
}

void cl_resize(CairoLayout *l)
{
    cl__do_resize(l);
}

/* ── Toolbar — boutons ──────────────────────────────────────────── */
static ClElem *cl__new_elem(CairoLayout *l)
{
    if (l->elem_count >= CL_MAX_ELEMS) return NULL;
    ClElem *e = &l->elems[l->elem_count++];
    memset(e, 0, sizeof *e);
    return e;
}

int cl_toolbar_add_btn(CairoLayout *l, int id, const char *label, int flags)
{
    ClElem *e = cl__new_elem(l); if (!e) return -1;
    e->type = CL_ELEM_BTN; e->id = id; e->flags = flags;
    strncpy(e->label, label ? label : "", 23);
    return l->elem_count - 1;
}

int cl_toolbar_add_icon(CairoLayout *l, int id, ClIconFn fn,
                         const char *tooltip, int flags)
{
    ClElem *e = cl__new_elem(l); if (!e) return -1;
    e->type = CL_ELEM_BTN; e->id = id; e->flags = flags; e->icon_fn = fn;
    strncpy(e->tooltip, tooltip ? tooltip : "", 47);
    return l->elem_count - 1;
}

int cl_toolbar_add_icon_multimode(CairoLayout *l, int id, ClIconFn fn,
                                   const char *tooltip, int mode_count)
{
    ClElem *e = cl__new_elem(l); if (!e) return -1;
    e->type       = CL_ELEM_BTN;
    e->id         = id;
    e->flags      = CL_BTN_SPLIT;   /* visuellement identique au split */
    e->icon_fn    = fn;
    e->mode       = 0;
    e->mode_count = mode_count > 0 ? mode_count : 1;
    strncpy(e->tooltip, tooltip ? tooltip : "", 47);
    return l->elem_count - 1;
}

void cl_toolbar_set_mode(CairoLayout *l, int id, int mode)
{
    for (int i = 0; i < l->elem_count; i++) {
        if (l->elems[i].id == id && l->elems[i].mode_count > 0) {
            l->elems[i].mode = mode % l->elems[i].mode_count;
            InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
            return;
        }
    }
}

int cl_toolbar_get_mode(CairoLayout *l, int id)
{
    for (int i = 0; i < l->elem_count; i++)
        if (l->elems[i].id == id) return l->elems[i].mode;
    return 0;
}

int cl_toolbar_add_svg(CairoLayout *l, int id, RsvgHandle *rsvg,
                        const char *tooltip, int flags)
{
    ClElem *e = cl__new_elem(l); if (!e) return -1;
    e->type  = CL_ELEM_BTN;
    e->id    = id;
    e->flags = flags;
    e->rsvg  = rsvg;   /* pointeur direct — pas de copie, pas de unref */
    strncpy(e->tooltip, tooltip ? tooltip : "", 47);
    return l->elem_count - 1;
}

void cl_toolbar_add_sep(CairoLayout *l)
{
    ClElem *e = cl__new_elem(l); if (!e) return;
    e->type = CL_ELEM_SEP;
}

int cl_toolbar_add_combo(CairoLayout *l, int id, int width)
{
    if (l->combo_count >= CL_MAX_COMBOS) return -1;
    int ci = l->combo_count++;
    ClCombo *c = &l->combos[ci];
    memset(c, 0, sizeof *c);
    c->id = id; c->width = width; c->current = -1; c->hovered = -1;

    ClElem *e = cl__new_elem(l); if (!e) return -1;
    e->type = CL_ELEM_COMBO; e->id = id; e->combo_idx = ci;
    return ci;
}

void cl_toolbar_set_pressed(CairoLayout *l, int id, int pressed)
{
    for (int i = 0; i < l->elem_count; i++)
        if (l->elems[i].id == id && l->elems[i].type == CL_ELEM_BTN) {
            l->elems[i].pressed = pressed;
            InvalidateRect(l->hwnd_toolbar, NULL, FALSE); return;
        }
}

void cl_toolbar_set_enabled(CairoLayout *l, int id, int enabled)
{
    for (int i = 0; i < l->elem_count; i++)
        if (l->elems[i].id == id && l->elems[i].type == CL_ELEM_BTN) {
            if (enabled) l->elems[i].flags &= ~CL_BTN_DISABLED;
            else         l->elems[i].flags |=  CL_BTN_DISABLED;
            InvalidateRect(l->hwnd_toolbar, NULL, FALSE); return;
        }
}

void cl_set_toolbar_cb(CairoLayout *l, CairoLayoutCb cb, void *ud)
{
    l->tb_cb = cb; l->tb_ud = ud;
}

int cl_toolbar_get_btn_screen_rect(CairoLayout *l, int id, RECT *rc_screen)
{
    for (int i = 0; i < l->elem_count; i++) {
        ClElem *e = &l->elems[i];
        if (e->type != CL_ELEM_SEP && e->id == id) {
            /* Rectangle client dans la toolbar */
            RECT rc_client = { e->x, 0, e->x + e->w, CL_TOOLBAR_H };
            /* Convertir en coordonnées écran */
            POINT pt_tl = { rc_client.left,  rc_client.top    };
            POINT pt_br = { rc_client.right, rc_client.bottom };
            ClientToScreen(l->hwnd_toolbar, &pt_tl);
            ClientToScreen(l->hwnd_toolbar, &pt_br);
            rc_screen->left   = pt_tl.x;
            rc_screen->top    = pt_tl.y;
            rc_screen->right  = pt_br.x;
            rc_screen->bottom = pt_br.y;
            return 1;
        }
    }
    return 0;
}

/* ── Combo — helper ─────────────────────────────────────────────── */
static ClCombo *cl__find_combo(CairoLayout *l, int id)
{
    for (int i = 0; i < l->combo_count; i++)
        if (l->combos[i].id == id) return &l->combos[i];
    return NULL;
}

/* ── Combo — API ────────────────────────────────────────────────── */
void cl_combo_clear(CairoLayout *l, int id)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return;
    c->count = 0; c->current = -1;
    InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
}

int cl_combo_add_layer(CairoLayout *l, int id, const char *name, int visible)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return -1;
    if (c->count >= CL_MAX_LAYERS) return -1;
    int idx = c->count++;
    strncpy(c->layers[idx].name, name ? name : "", 63);
    c->layers[idx].visible = visible;
    if (c->current < 0) c->current = 0;
    InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
    return idx;
}

void cl_combo_set_current(CairoLayout *l, int id, int idx)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return;
    if (idx >= 0 && idx < c->count) c->current = idx;
    InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
}

void cl_combo_set_name(CairoLayout *l, int id, int idx, const char *name)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return;
    if (idx < 0 || idx >= c->count) return;
    strncpy(c->layers[idx].name, name ? name : "", 63);
    InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
    if (c->hwnd_drop) InvalidateRect(c->hwnd_drop, NULL, FALSE);
}

void cl_combo_set_visible(CairoLayout *l, int id, int idx, int visible)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return;
    if (idx < 0 || idx >= c->count) return;
    c->layers[idx].visible = visible;
    InvalidateRect(l->hwnd_toolbar, NULL, FALSE);
    if (c->hwnd_drop) InvalidateRect(c->hwnd_drop, NULL, FALSE);
}

int cl_combo_get_current(CairoLayout *l, int id)
{
    ClCombo *c = cl__find_combo(l, id); return c ? c->current : -1;
}

int cl_combo_get_visible(CairoLayout *l, int id, int idx)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return 1;
    if (idx < 0 || idx >= c->count) return 1;
    return c->layers[idx].visible;
}

const char *cl_combo_get_name(CairoLayout *l, int id, int idx)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return "";
    if (idx < 0 || idx >= c->count) return "";
    return c->layers[idx].name;
}

int cl_combo_get_count(CairoLayout *l, int id)
{
    ClCombo *c = cl__find_combo(l, id); return c ? c->count : 0;
}

void cl_combo_set_layer_data(CairoLayout *l, int id, int idx,
                              int db_id, int geom_type, int style_id)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return;
    if (idx < 0 || idx >= c->count) return;
    c->layers[idx].db_id     = db_id;
    c->layers[idx].geom_type = geom_type;
    c->layers[idx].style_id  = style_id;
}

int cl_combo_get_db_id(CairoLayout *l, int id, int idx)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return -1;
    if (idx < 0 || idx >= c->count) return -1;
    return c->layers[idx].db_id;
}

int cl_combo_get_geom_type(CairoLayout *l, int id, int idx)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return -1;
    if (idx < 0 || idx >= c->count) return -1;
    return c->layers[idx].geom_type;
}

int cl_combo_get_style_id(CairoLayout *l, int id, int idx)
{
    ClCombo *c = cl__find_combo(l, id); if (!c) return -1;
    if (idx < 0 || idx >= c->count) return -1;
    return c->layers[idx].style_id;
}

/* ── Statusbar ──────────────────────────────────────────────────── */
int cl_status_add_pane(CairoLayout *l, int type, int width)
{
    if (l->pane_count >= CL_MAX_PANES) return -1;
    ClPane *p = &l->panes[l->pane_count++];
    p->type = type; p->width = width; p->text[0] = '\0';
    return l->pane_count - 1;
}

void cl_status_set(CairoLayout *l, int idx, const char *text)
{
    if (idx < 0 || idx >= l->pane_count) return;
    strncpy(l->panes[idx].text, text ? text : "", 127);
    InvalidateRect(l->hwnd_status, NULL, FALSE);
}

void cl_status_set_progress(CairoLayout *l, int idx, float progress)
{
    if (idx < 0 || idx >= l->pane_count) return;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    l->panes[idx].progress = progress;
    InvalidateRect(l->hwnd_status, NULL, FALSE);
    UpdateWindow(l->hwnd_status);  /* forcer repaint immédiat — utile pendant chargement */
}

void cl_status_clear(CairoLayout *l, int idx)
{
    if (idx < 0 || idx >= l->pane_count) return;
    l->panes[idx].progress = 0.0f;
    l->panes[idx].text[0]  = '\0';
    InvalidateRect(l->hwnd_status, NULL, FALSE);
}

/* ── Menubar ────────────────────────────────────────────────────── */
int cl_menubar_add(CairoLayout *l, int id, const char *label)
{
    if (!l || l->menu_count >= CL_MAX_MENUITEMS) return -1;
    ClMenuItem *mi = &l->menu_items[l->menu_count++];
    mi->id      = id;
    mi->hovered = 0;
    mi->x = mi->w = 0;   /* calculés au prochain paint */
    strncpy(mi->label, label ? label : "", 31);
    return l->menu_count - 1;
}

void cl_set_menubar_cb(CairoLayout *l, CairoLayoutCb cb, void *ud)
{
    if (!l) return;
    l->menu_cb = cb;
    l->menu_ud = ud;
}

int cl_menubar_get_item_screen_rect(CairoLayout *l, int id, RECT *rc_screen)
{
    if (!l || !l->hwnd_menubar) return 0;
    for (int i = 0; i < l->menu_count; i++) {
        if (l->menu_items[i].id == id) {
            ClMenuItem *mi = &l->menu_items[i];
            POINT pt_tl = { mi->x,        0              };
            POINT pt_br = { mi->x + mi->w, CL_MENUBAR_H  };
            ClientToScreen(l->hwnd_menubar, &pt_tl);
            ClientToScreen(l->hwnd_menubar, &pt_br);
            rc_screen->left   = pt_tl.x;
            rc_screen->top    = pt_tl.y;
            rc_screen->right  = pt_br.x;
            rc_screen->bottom = pt_br.y;
            return 1;
        }
    }
    return 0;
}

void cl_menubar_close(CairoLayout *l)
{
    if (!l) return;
    l->menu_open = -1;
    if (l->hwnd_menubar) InvalidateRect(l->hwnd_menubar, NULL, FALSE);
}

/* ── Transcript + splitter ──────────────────────────────────────── */
void cl_attach_transcript(CairoLayout *l, CairoTranscript *ct, int init_h)
{
    if (!l || !ct) return;
    l->transcript     = ct;
    l->transcript_h   = init_h > 0 ? init_h : 200;
    l->transcript_vis = 0;   /* caché par défaut */


    /* Créer la fenêtre splitter */
    l->hwnd_splitter = ui_subwnd_create("CL_Splitter", l->parent,
        0, 0, 1, CL_SPLITTER_H, l);
    ShowWindow(l->hwnd_splitter, SW_HIDE);  /* caché par défaut */
}

void cl_show_transcript(CairoLayout *l)
{
    if (!l || !l->transcript) return;
    l->transcript_vis = 1;
    cl__do_resize(l);
    InvalidateRect(l->canvas, NULL, FALSE);
}

void cl_hide_transcript(CairoLayout *l)
{
    if (!l || !l->transcript) return;
    l->transcript_vis = 0;
    cl__do_resize(l);
    InvalidateRect(l->canvas, NULL, FALSE);
}

void cl_toggle_transcript(CairoLayout *l)
{
    if (!l || !l->transcript) return;
    if (l->transcript_vis)
        cl_hide_transcript(l);
    else
        cl_show_transcript(l);
}

/* ── cl_get_transcript_rect (legacy) ────────────────────────────── */
void cl_get_transcript_rect(CairoLayout *l, int *rx, int *ry, int *rw, int *rh)
{
    if (rx) *rx = 0;
    if (ry) *ry = 0;
    if (rw) *rw = 0;
    if (rh) *rh = 0;

    if (!l || !l->transcript || !l->transcript_vis) return;

    RECT rc;
    GetClientRect(l->parent, &rc);
    int w      = rc.right;
    int top    = CL_MENUBAR_H + CL_TOOLBAR_H;
    int bottom = rc.bottom - CL_STATUS_H;
    int avail  = bottom - top;

    int min_tc = 60;
    int max_tc = avail - 40 - CL_SPLITTER_H;
    if (max_tc < min_tc) max_tc = min_tc;

    int th = l->transcript_h;
    if (th < min_tc) th = min_tc;
    if (th > max_tc) th = max_tc;

    int canvas_h     = avail - CL_SPLITTER_H - th;
    int splitter_y   = top + canvas_h;
    int transcript_y = splitter_y + CL_SPLITTER_H;

    if (rx) *rx = 0;
    if (ry) *ry = transcript_y;
    if (rw) *rw = w;
    if (rh) *rh = th;
}

/* ── Tabpane helpers ─────────────────────────────────────────────── */
int cl_tabpane_add(CairoLayout *l, const char *label, HWND content)
{
#if CL_HAS_TABPANE
    if (!l || !l->tabpane) return -1;
    int idx = tp_tab_add(l->tabpane, label, 1);   /* closable */
    if (idx >= 0 && content)
        tp_tab_set_content(l->tabpane, idx, content);
    return idx;
#else
    (void)l; (void)label; (void)content;
    return -1;
#endif
}

void cl_tabpane_close(CairoLayout *l, int idx)
{
#if CL_HAS_TABPANE
    if (l && l->tabpane) tp_tab_close(l->tabpane, idx);
#else
    (void)l; (void)idx;
#endif
}

void cl_tabpane_select(CairoLayout *l, int idx)
{
#if CL_HAS_TABPANE
    if (l && l->tabpane) tp_tab_select(l->tabpane, idx);
#else
    (void)l; (void)idx;
#endif
}

CairoTabPane *cl_tabpane_get(CairoLayout *l)
{
#if CL_HAS_TABPANE
    return l ? l->tabpane : NULL;
#else
    (void)l;
    return NULL;
#endif
}

/* ── cl_attach_tabpane ───────────────────────────────────────────── */
#if CL_HAS_TABPANE && CL_HAS_TRANSCRIPT

/* Callback interne : relaie TP_EV_CLOSE vers bim.c */
typedef struct { CairoLayout *l; } ClTpUd;

static void cl__tp_event(int event, int tab_idx, void *ud)
{
    CairoLayout *l = (CairoLayout *)ud;
    if (!l) return;

    if (event == TP_EV_CLOSE) {
        HWND content = tp_tab_get_content(l->tabpane, tab_idx);
        if (l->tabpane_close_cb)
            l->tabpane_close_cb(tab_idx, content, l->tabpane_close_ud);
        /* le contenu sera détruit par bim.c dans le callback */
    }
}

void cl_attach_tabpane(CairoLayout *l, CairoTranscript *ct, int init_h,
                       void (*on_tab_close)(int idx, HWND content, void *ud),
                       void *ud)
{
    if (!l || !ct) return;

    /* stocker transcript + hauteur comme avant */
    l->transcript   = ct;
    l->transcript_h = init_h > 0 ? init_h : 200;

    /* créer le splitter */
    l->hwnd_splitter = ui_subwnd_create("CL_Splitter", l->parent,
                                         0, 0, 1, 1, l);
    ShowWindow(l->hwnd_splitter, SW_HIDE);

    /* créer le tabpane — géométrie provisoire, cl_resize s'en charge */
    l->tabpane = tp_create(l->parent, 0, 0, 1, 1);
    l->tabpane_close_cb = on_tab_close;
    l->tabpane_close_ud = ud;
    tp_set_cb(l->tabpane, cl__tp_event, l);

    /* Tab 0 : transcript fixe, sans close.
     * Le transcript a deux HWNDs (hwnd_log + hwnd_input) sans conteneur
     * commun. On crée un HWND conteneur transparent qui les englobe —
     * c'est ce HWND que tp_tab_set_content gère (show/hide/resize).     */

    /* créer le conteneur transcript — userdata = layout pour WM_SIZE */
    RECT rc_p; GetClientRect(l->parent, &rc_p);
    HWND hwnd_ct_wrap = ui_subwnd_create("BIM_CtWrap", l->parent,
                                          0, 0, rc_p.right, l->transcript_h,
                                          l);

    /* re-parenter les deux HWNDs transcript sous le conteneur */
    struct CairoTranscript_ *ct_ = (struct CairoTranscript_ *)ct;
    SetParent(ct_->hwnd_log,   hwnd_ct_wrap);
    SetParent(ct_->hwnd_input, hwnd_ct_wrap);
    /* SetParent cache les fenêtres — les remettre visibles */
    ShowWindow(ct_->hwnd_log,   SW_SHOW);
    ShowWindow(ct_->hwnd_input, SW_SHOW);

    /* mémoriser le wrap dans la struct pour ct_resize via WM_SIZE */
    l->hwnd_ct_wrap = hwnd_ct_wrap;

    int t0 = tp_tab_add(l->tabpane, "Transcript", 0);
    tp_tab_set_content(l->tabpane, t0, hwnd_ct_wrap);

    /* caché par défaut — cl_toggle_transcript() l'ouvre comme avant */
    l->transcript_vis = 0;
    SetWindowPos(l->tabpane->hwnd, NULL, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_HIDEWINDOW | SWP_NOZORDER);
    cl_resize(l);
}

#endif /* CL_HAS_TABPANE && CL_HAS_TRANSCRIPT */

#endif /* CAIRO_LAYOUT_IMPLEMENTATION */
#endif /* CAIRO_LAYOUT_H */
