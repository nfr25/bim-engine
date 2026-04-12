/*
 * cairo_tabpane.h  –  Tab pane Cairo/Win32, self-contained
 * ──────────────────────────────────────────────────────────────────
 *
 *  STRUCTURE
 *  ─────────
 *   ┌──────┬──────────────┬──────────────┬──────────────┬──┬──┐
 *   │Transcr│  Sélection  ×│  Query 1    ×│  Query 2    ×│ <│ >│
 *   ├────────────────────────────────────────────────────────────┤
 *   │                                                            │
 *   │   contenu de l'onglet actif (HWND enfant géré par bim.c)  │
 *   │                                                            │
 *   └────────────────────────────────────────────────────────────┘
 *
 *  RÈGLES DE DESIGN
 *  ─────────────────
 *   - Tab 0   : fixe, jamais de bouton close (ex: Transcript)
 *   - Tab 1..n: dynamiques, bouton close visible au hover
 *   - Largeur fixe : tous les tabs ont la même largeur, ils s'étalent
 *     pour remplir le conteneur
 *   - Overflow : flèches < > apparaissent si les tabs ne tiennent pas
 *   - Style fin et classy : coins arrondis en haut, séparateurs subtils
 *
 *  UTILISATION MINIMALE
 *  ─────────────────────
 *    #define CAIRO_TABPANE_IMPLEMENTATION
 *    #include "cairo_tabpane.h"
 *
 *  CRÉATION
 *    CairoTabPane *tp = tp_create(hwnd_parent, x, y, w, h);
 *    tp_set_cb(tp, on_tab_event, userdata);
 *
 *  GESTION DES TABS
 *    int idx = tp_tab_add(tp, "Transcript", 0);   // 0 = sans close
 *    int idx = tp_tab_add(tp, "Sélection",  1);   // 1 = avec close
 *    tp_tab_set_label(tp, idx, "nouveau label");
 *    tp_tab_close(tp, idx);                        // ferme l'onglet
 *    tp_tab_select(tp, idx);                       // active l'onglet
 *    int cur = tp_tab_current(tp);
 *    int n   = tp_tab_count(tp);
 *
 *  CONTENU
 *  ────────
 *  Le TabPane gère UNE zone de contenu (un HWND enfant à la fois).
 *  Chaque tab a un HWND de contenu associé. Le TabPane les show/hide
 *  automatiquement lors du switch.
 *
 *    tp_tab_set_content(tp, idx, hwnd_content);
 *    HWND hw = tp_tab_get_content(tp, idx);
 *
 *  CALLBACK
 *  ─────────
 *    typedef void (*TpEventCb)(int event, int tab_idx, void *ud);
 *    events : TP_EV_SELECT   — tab activé
 *             TP_EV_CLOSE    — tab fermé (bim.c doit détruire le contenu)
 *
 *  INTÉGRATION WndProc
 *    case WM_SIZE:    tp_resize(tp, x, y, new_w, new_h); break;
 *    case WM_DESTROY: tp_destroy(tp);                    break;
 *
 * ──────────────────────────────────────────────────────────────────
 *  Dépendances : cairo, cairo-win32, ui_backend.h
 *  Compilateur : GCC / MinGW-w64 (MSYS2)
 * ──────────────────────────────────────────────────────────────────
 */

#ifndef CAIRO_TABPANE_H
#define CAIRO_TABPANE_H

#include "ui_backend.h"
#include <windows.h>
#include <windowsx.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* ── Dimensions ────────────────────────────────────────────────────── */
#define TP_TAB_H        28      /* hauteur de la barre de tabs         */
#define TP_ARROW_W      22      /* largeur d'une flèche de scroll      */
#define TP_CLOSE_SIZE   14      /* zone cliquable du bouton close      */
#define TP_CLOSE_ICON    8      /* taille de la croix dessinée         */
#define TP_TAB_MIN_W    60      /* largeur min d'un tab                */
#define TP_TAB_MAX_W   200      /* largeur max d'un tab                */
#define TP_MAX_TABS     32      /* nombre max de tabs                  */
#define TP_LABEL_MAX    64      /* longueur max du label               */
#define TP_RADIUS        4.0    /* rayon des coins arrondis en haut    */
#define TP_SLANT        10.0    /* décalage horizontal de la pente droite */

/* ── Événements callback ────────────────────────────────────────────── */
#define TP_EV_SELECT    1       /* tab activé                          */
#define TP_EV_CLOSE     2       /* tab fermé (détruire le contenu)     */

/* ── Couleurs (dark theme cohérent avec Pottery) ───────────────────── */
#define TP_COL_BG           0.10, 0.10, 0.11   /* fond général (barre) */
#define TP_COL_TAB_ACTIVE   0.19, 0.19, 0.22   /* tab actif            */
#define TP_COL_TAB_HOVER    0.16, 0.16, 0.19   /* tab survolé          */
#define TP_COL_TAB_NORMAL   0.13, 0.13, 0.15   /* tab inactif          */
#define TP_COL_BORDER       0.28, 0.28, 0.32   /* séparateurs          */
#define TP_COL_TEXT_ACTIVE  0.95, 0.95, 1.00   /* texte tab actif      */
#define TP_COL_TEXT_NORMAL  0.58, 0.58, 0.63   /* texte tab inactif    */
#define TP_COL_ACCENT       0.35, 0.60, 1.00   /* ligne accent active  */
#define TP_COL_CLOSE_HOVER  0.80, 0.30, 0.30   /* croix au hover       */
#define TP_COL_ARROW        0.60, 0.60, 0.65   /* flèches              */
#define TP_COL_ARROW_HOVER  0.85, 0.85, 0.90   /* flèches survolées    */

/* ── Types ─────────────────────────────────────────────────────────── */
typedef struct CairoTabPane_ CairoTabPane;
typedef void (*TpEventCb)(int event, int tab_idx, void *userdata);

/* ── API publique ───────────────────────────────────────────────────── */
CairoTabPane *tp_create (HWND parent, int x, int y, int w, int h);
void          tp_destroy(CairoTabPane *tp);
void          tp_resize (CairoTabPane *tp, int x, int y, int w, int h);
void          tp_set_cb (CairoTabPane *tp, TpEventCb cb, void *ud);

/* gestion des tabs */
int   tp_tab_add        (CairoTabPane *tp, const char *label, int closable);
void  tp_tab_close      (CairoTabPane *tp, int idx);
void  tp_tab_select     (CairoTabPane *tp, int idx);
void  tp_tab_set_label  (CairoTabPane *tp, int idx, const char *label);
int   tp_tab_current    (CairoTabPane *tp);
int   tp_tab_count      (CairoTabPane *tp);

/* contenu : le TabPane show/hide automatiquement */
void  tp_tab_set_content(CairoTabPane *tp, int idx, HWND hwnd_content);
HWND  tp_tab_get_content(CairoTabPane *tp, int idx);

/* zone de contenu (pour placer les contrôles enfants) */
void  tp_get_content_rect(CairoTabPane *tp, RECT *rc);

/* ══════════════════════════════════════════════════════════════════════
   IMPLÉMENTATION
   ══════════════════════════════════════════════════════════════════════ */
#ifdef CAIRO_TABPANE_IMPLEMENTATION

/* ── Structure interne d'un tab ─────────────────────────────────────── */
typedef struct {
    char  label[TP_LABEL_MAX];
    int   closable;             /* 0 = pas de bouton close             */
    HWND  hwnd_content;         /* fenêtre de contenu (peut être NULL) */

    /* état hover (mis à jour dans WM_MOUSEMOVE) */
    int   hover;                /* survol du tab lui-même              */
    int   close_hover;          /* survol du bouton close              */

    /* géométrie calculée à chaque layout */
    int   x, w;                 /* position et largeur dans la barre   */
} TpTab;

/* ── Structure principale ───────────────────────────────────────────── */
struct CairoTabPane_ {
    HWND        hwnd;           /* fenêtre principale du contrôle      */
    HWND        parent;

    TpTab       tabs[TP_MAX_TABS];
    int         tab_count;
    int         current;        /* index du tab actif                  */

    int         x, y, w, h;    /* rect du contrôle entier             */

    /* scroll des tabs si overflow */
    int         scroll_offset;  /* index du premier tab visible        */
    int         arrow_left_hover;
    int         arrow_right_hover;
    int         show_arrows;    /* 1 si les flèches sont nécessaires   */

    /* tab_w : largeur d'un tab (calculée dynamiquement) */
    int         tab_w;

    TpEventCb   cb;
    void       *cb_ud;

    /* tracking souris */
    int         mouse_tracked;
};

/* ── Forward declarations ───────────────────────────────────────────── */
static LRESULT CALLBACK tp__wnd_proc(HWND, UINT, WPARAM, LPARAM);
static void tp__draw        (CairoTabPane *tp, cairo_t *cr, int w, int h);
static void tp__layout_tabs (CairoTabPane *tp);
static void tp__show_content(CairoTabPane *tp, int idx);
static int  tp__hit_tab     (CairoTabPane *tp, int mx, int my);
static int  tp__hit_close   (CairoTabPane *tp, int tab_idx, int mx, int my);
static int  tp__hit_arrow_l (CairoTabPane *tp, int mx, int my);
static int  tp__hit_arrow_r (CairoTabPane *tp, int mx, int my);

/* ── Enregistrement de la classe ────────────────────────────────────── */
static void tp__register_class(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    ui_register_class("BIM_TabPane", tp__wnd_proc, 0, NULL);
}

/* ── Création ───────────────────────────────────────────────────────── */
CairoTabPane *tp_create(HWND parent, int x, int y, int w, int h)
{
    tp__register_class();

    CairoTabPane *tp = (CairoTabPane*)calloc(1, sizeof *tp);
    if (!tp) return NULL;

    tp->parent   = parent;
    tp->x = x; tp->y = y; tp->w = w; tp->h = h;
    tp->current  = -1;
    tp->tab_count = 0;

    tp->hwnd = ui_subwnd_create("BIM_TabPane", parent, x, y, w, h, tp);
    if (!tp->hwnd) { free(tp); return NULL; }

    return tp;
}

/* ── Destruction ────────────────────────────────────────────────────── */
void tp_destroy(CairoTabPane *tp)
{
    if (!tp) return;
    if (tp->hwnd) DestroyWindow(tp->hwnd);
    free(tp);
}

/* ── Resize ─────────────────────────────────────────────────────────── */
void tp_resize(CairoTabPane *tp, int x, int y, int w, int h)
{
    if (!tp) return;
    tp->x = x; tp->y = y; tp->w = w; tp->h = h;
    MoveWindow(tp->hwnd, x, y, w, h, TRUE);
    tp__layout_tabs(tp);
    /* redimensionner le contenu actif */
    if (tp->current >= 0 && tp->current < tp->tab_count) {
        HWND hc = tp->tabs[tp->current].hwnd_content;
        if (hc) {
            int ch = h - TP_TAB_H;
            MoveWindow(hc, 0, TP_TAB_H, w, ch > 0 ? ch : 0, TRUE);
        }
    }
}

/* ── Callback ───────────────────────────────────────────────────────── */
void tp_set_cb(CairoTabPane *tp, TpEventCb cb, void *ud)
{
    if (!tp) return;
    tp->cb    = cb;
    tp->cb_ud = ud;
}

/* ── Ajout d'un tab ─────────────────────────────────────────────────── */
int tp_tab_add(CairoTabPane *tp, const char *label, int closable)
{
    if (!tp || tp->tab_count >= TP_MAX_TABS) return -1;

    int idx = tp->tab_count++;
    TpTab *t = &tp->tabs[idx];

    strncpy(t->label, label ? label : "", TP_LABEL_MAX - 1);
    t->label[TP_LABEL_MAX - 1] = '\0';
    t->closable     = closable;
    t->hwnd_content = NULL;
    t->hover        = 0;
    t->close_hover  = 0;

    /* activer automatiquement le premier tab */
    if (tp->current < 0) tp->current = 0;

    tp__layout_tabs(tp);
    ui_redraw(tp->hwnd);
    return idx;
}

/* ── Fermeture d'un tab ─────────────────────────────────────────────── */
void tp_tab_close(CairoTabPane *tp, int idx)
{
    if (!tp || idx < 0 || idx >= tp->tab_count) return;
    if (!tp->tabs[idx].closable) return;

    /* notifier avant suppression */
    if (tp->cb) tp->cb(TP_EV_CLOSE, idx, tp->cb_ud);

    /* décaler les tabs suivants */
    for (int i = idx; i < tp->tab_count - 1; i++)
        tp->tabs[i] = tp->tabs[i + 1];
    tp->tab_count--;

    /* ajuster current */
    if (tp->current >= tp->tab_count)
        tp->current = tp->tab_count - 1;
    if (tp->current < 0 && tp->tab_count > 0)
        tp->current = 0;

    /* s'assurer que le scroll reste valide */
    if (tp->scroll_offset >= tp->tab_count)
        tp->scroll_offset = tp->tab_count > 0 ? tp->tab_count - 1 : 0;

    tp__layout_tabs(tp);
    tp__show_content(tp, tp->current);
    if (tp->cb && tp->current >= 0) tp->cb(TP_EV_SELECT, tp->current, tp->cb_ud);
    ui_redraw(tp->hwnd);
}

/* ── Sélection d'un tab ─────────────────────────────────────────────── */
void tp_tab_select(CairoTabPane *tp, int idx)
{
    if (!tp || idx < 0 || idx >= tp->tab_count) return;
    if (tp->current == idx) return;

    tp->current = idx;
    tp__show_content(tp, idx);
    if (tp->cb) tp->cb(TP_EV_SELECT, idx, tp->cb_ud);
    ui_redraw(tp->hwnd);
}

/* ── Label ──────────────────────────────────────────────────────────── */
void tp_tab_set_label(CairoTabPane *tp, int idx, const char *label)
{
    if (!tp || idx < 0 || idx >= tp->tab_count) return;
    strncpy(tp->tabs[idx].label, label ? label : "", TP_LABEL_MAX - 1);
    tp->tabs[idx].label[TP_LABEL_MAX - 1] = '\0';
    ui_redraw(tp->hwnd);
}

/* ── Accesseurs ─────────────────────────────────────────────────────── */
int  tp_tab_current(CairoTabPane *tp) { return tp ? tp->current : -1; }
int  tp_tab_count  (CairoTabPane *tp) { return tp ? tp->tab_count : 0; }

void tp_tab_set_content(CairoTabPane *tp, int idx, HWND hwnd_content)
{
    if (!tp || idx < 0 || idx >= tp->tab_count) return;

    tp->tabs[idx].hwnd_content = hwnd_content;

    /* positionner le contenu dans la zone */
    if (hwnd_content) {
        int ch = tp->h - TP_TAB_H;
        SetParent(hwnd_content, tp->hwnd);
        MoveWindow(hwnd_content, 0, TP_TAB_H, tp->w, ch > 0 ? ch : 0, TRUE);
        ShowWindow(hwnd_content, idx == tp->current ? SW_SHOW : SW_HIDE);
    }
}

HWND tp_tab_get_content(CairoTabPane *tp, int idx)
{
    if (!tp || idx < 0 || idx >= tp->tab_count) return NULL;
    return tp->tabs[idx].hwnd_content;
}

void tp_get_content_rect(CairoTabPane *tp, RECT *rc)
{
    if (!tp || !rc) return;
    rc->left   = 0;
    rc->top    = TP_TAB_H;
    rc->right  = tp->w;
    rc->bottom = tp->h;
}

/* ═══════════════════════════════════════════════════════════════════
   FONCTIONS INTERNES
   ═══════════════════════════════════════════════════════════════════ */

/* ── Calcul de la largeur des tabs et de la nécessité des flèches ── */
static void tp__layout_tabs(CairoTabPane *tp)
{
    if (!tp || tp->tab_count == 0) return;

    /* zone disponible pour les tabs (potentiellement sans les flèches) */
    int avail = tp->w;

    /* largeur idéale : répartir équitablement */
    int ideal_w = avail / tp->tab_count;
    if (ideal_w < TP_TAB_MIN_W) ideal_w = TP_TAB_MIN_W;
    if (ideal_w > TP_TAB_MAX_W) ideal_w = TP_TAB_MAX_W;

    int total_w = ideal_w * tp->tab_count;

    if (total_w <= avail) {
        /* tous les tabs tiennent — on les étale MAIS pas au-delà de TP_TAB_MAX_W
         * pour garder la forme tab visible (sinon 1 tab = toute la largeur) */
        tp->show_arrows = 0;
        int spread_w = avail / tp->tab_count;
        if (spread_w > TP_TAB_MAX_W) spread_w = TP_TAB_MAX_W;
        tp->tab_w = spread_w;
    } else {
        /* overflow : flèches nécessaires */
        tp->show_arrows = 1;
        avail -= 2 * TP_ARROW_W;
        tp->tab_w = ideal_w;
    }

    /* s'assurer que scroll_offset est valide */
    if (tp->scroll_offset < 0) tp->scroll_offset = 0;

    /* calculer x de chaque tab */
    int offset_x = tp->show_arrows ? TP_ARROW_W : 0;
    for (int i = 0; i < tp->tab_count; i++) {
        tp->tabs[i].x = offset_x + (i - tp->scroll_offset) * tp->tab_w;
        tp->tabs[i].w = tp->tab_w;
    }
}

/* ── Show/hide des contenus ─────────────────────────────────────────── */
static void tp__show_content(CairoTabPane *tp, int active)
{
    for (int i = 0; i < tp->tab_count; i++) {
        HWND hc = tp->tabs[i].hwnd_content;
        if (hc) ShowWindow(hc, i == active ? SW_SHOW : SW_HIDE);
    }
}

/* ── Hit tests ──────────────────────────────────────────────────────── */
static int tp__hit_tab(CairoTabPane *tp, int mx, int my)
{
    if (my < 0 || my >= TP_TAB_H) return -1;
    for (int i = 0; i < tp->tab_count; i++) {
        TpTab *t = &tp->tabs[i];
        if (mx >= t->x && mx < t->x + t->w)
            return i;
    }
    return -1;
}

static int tp__hit_close(CairoTabPane *tp, int tab_idx, int mx, int my)
{
    if (tab_idx < 0 || tab_idx >= tp->tab_count) return 0;
    if (!tp->tabs[tab_idx].closable) return 0;

    TpTab *t = &tp->tabs[tab_idx];
    /* le bouton close est à droite dans le tab */
    int close_x = t->x + t->w - TP_CLOSE_SIZE - 4;
    int close_y = (TP_TAB_H - TP_CLOSE_SIZE) / 2;
    return (mx >= close_x && mx < close_x + TP_CLOSE_SIZE &&
            my >= close_y && my < close_y + TP_CLOSE_SIZE);
}

static int tp__hit_arrow_l(CairoTabPane *tp, int mx, int my)
{
    if (!tp->show_arrows) return 0;
    return (mx >= 0 && mx < TP_ARROW_W && my >= 0 && my < TP_TAB_H);
}

static int tp__hit_arrow_r(CairoTabPane *tp, int mx, int my)
{
    if (!tp->show_arrows) return 0;
    return (mx >= tp->w - TP_ARROW_W && mx < tp->w && my >= 0 && my < TP_TAB_H);
}

/* ═══════════════════════════════════════════════════════════════════
   DESSIN CAIRO
   ═══════════════════════════════════════════════════════════════════ */

/* ── Forme tab avec pente à droite (style trapèze) ─────────────────────
 *
 *   ╭────────╲
 *   │  label  ╲
 * ──┘          ────
 *
 * x,y = coin haut-gauche, w,h = dimensions, r = rayon coin haut-gauche
 * slant = décalage horizontal de la pente droite (ex: 10px)
 * ─────────────────────────────────────────────────────────────────────── */
static void tp__tab_shape(cairo_t *cr, double x, double y,
                           double w, double h, double r, double slant)
{
    cairo_new_sub_path(cr);
    cairo_move_to(cr, x,            y + h);          /* bas-gauche       */
    cairo_line_to(cr, x,            y + r);          /* montée gauche    */
    cairo_arc    (cr, x + r,        y + r, r,        /* coin haut-gauche */
                  M_PI, -M_PI / 2.0);
    cairo_line_to(cr, x + w - slant, y);             /* haut             */
    cairo_line_to(cr, x + w,        y + h);          /* pente droite     */
    cairo_close_path(cr);
}

/* wrapper pour compatibilité (slant=0 = tab rectangulaire) */
static void tp__rounded_top(cairo_t *cr, double x, double y,
                             double w, double h, double r)
{
    tp__tab_shape(cr, x, y, w, h, r, 0.0);
}

/* ── Dessin d'un tab individuel — style Eclipse ──────────────────────── */
static void tp__draw_tab(cairo_t *cr, CairoTabPane *tp, int i)
{
    TpTab *t   = &tp->tabs[i];
    int active = (i == tp->current);

    /* Géométrie Eclipse :
     * - tab inactif : plus petit, décalé vers le bas (ty = 4)
     * - tab actif   : pleine hauteur (ty = 0), descend 1px sous la ligne
     *   de séparation pour "fusionner" avec le contenu               */
    double tx = (double)t->x;
    double tw = (double)t->w - 1.0;
    double ty, th;

    if (active) {
        ty = 1.0;
        th = (double)TP_TAB_H;     /* descend jusqu'en bas + 1px          */
    } else {
        ty = 4.0;
        th = (double)TP_TAB_H - 4.0;
    }

    double r = TP_RADIUS + (active ? 1.0 : 0.0);

    /* ── fond du tab ── */
    if (active)
        cairo_set_source_rgb(cr, TP_COL_TAB_ACTIVE);
    else if (t->hover)
        cairo_set_source_rgb(cr, TP_COL_TAB_HOVER);
    else
        cairo_set_source_rgb(cr, TP_COL_TAB_NORMAL);

    tp__tab_shape(cr, tx, ty, tw, th, r, TP_SLANT);
    cairo_fill(cr);

    /* ── bordure Eclipse : gauche + haut + droite ── */
    if (active) {
        /* bordure couleur accent */
        cairo_set_source_rgb(cr, TP_COL_ACCENT);
        cairo_set_line_width(cr, 1.5);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);

        /* contour tab actif avec pente droite */
        cairo_new_sub_path(cr);
        cairo_move_to(cr, tx,             ty + th);   /* bas-gauche      */
        cairo_line_to(cr, tx,             ty + r);
        cairo_arc    (cr, tx + r,         ty + r, r, M_PI, -M_PI / 2.0);
        cairo_line_to(cr, tx + tw - TP_SLANT, ty);   /* haut            */
        cairo_line_to(cr, tx + tw,        ty + th);   /* pente droite    */
        cairo_stroke(cr);

        /* ligne d'accent en haut — suit la forme (s'arrête avant la pente) */
        cairo_set_source_rgb(cr, TP_COL_ACCENT);
        cairo_set_line_width(cr, 2.5);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, tx + r,                 ty + 0.75);
        cairo_line_to(cr, tx + tw - TP_SLANT - 2, ty + 0.75);
        cairo_stroke(cr);

    } else {
        /* bordure subtile pour les tabs inactifs */
        cairo_set_source_rgba(cr, TP_COL_BORDER, 0.6);
        cairo_set_line_width(cr, 1.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
        cairo_new_sub_path(cr);
        cairo_move_to(cr, tx,             ty + th);
        cairo_line_to(cr, tx,             ty + r);
        cairo_arc    (cr, tx + r,         ty + r, r, M_PI, -M_PI / 2.0);
        cairo_line_to(cr, tx + tw - TP_SLANT, ty);   /* pente droite    */
        cairo_line_to(cr, tx + tw,        ty + th);
        cairo_stroke(cr);
    }

    /* calcul de la zone de texte — réduire pour la pente droite */
    double text_x  = tx + 10.0;
    double text_rw = tw - 12.0 - TP_SLANT;   /* marge droite + pente */
    if (t->closable) text_rw -= (double)(TP_CLOSE_SIZE + 6);

    /* texte tronqué avec "…" */
    cairo_set_font_size(cr, 11.5);
    cairo_select_font_face(cr, "Segoe UI",
                           CAIRO_FONT_SLANT_NORMAL,
                           active ? CAIRO_FONT_WEIGHT_BOLD
                                  : CAIRO_FONT_WEIGHT_NORMAL);

    if (active)
        cairo_set_source_rgb(cr, TP_COL_TEXT_ACTIVE);
    else
        cairo_set_source_rgb(cr, TP_COL_TEXT_NORMAL);

    /* mesure + troncature */
    char display[TP_LABEL_MAX + 4];
    strncpy(display, t->label, TP_LABEL_MAX);
    display[TP_LABEL_MAX] = '\0';

    cairo_text_extents_t ext;
    cairo_text_extents(cr, display, &ext);

    if (ext.width > text_rw && text_rw > 20.0) {
        /* troncature : retire des caractères jusqu'à ce que ça tienne */
        int len = (int)strlen(display);
        while (len > 1) {
            display[len - 1] = '\0';
            char tmp[TP_LABEL_MAX + 4];
            snprintf(tmp, sizeof tmp, "%s…", display);
            cairo_text_extents(cr, tmp, &ext);
            if (ext.width <= text_rw) {
                strncpy(display, tmp, sizeof display - 1);
                break;
            }
            len--;
        }
    }

    /* centrage vertical */
    cairo_font_extents_t fext;
    cairo_font_extents(cr, &fext);
    double text_y = ty + (th - fext.height) / 2.0 + fext.ascent;

    cairo_move_to(cr, text_x, text_y);
    cairo_show_text(cr, display);

    /* bouton close */
    if (t->closable) {
        double cx = tx + tw - TP_CLOSE_SIZE - 4.0;
        double cy = ty + (th - TP_CLOSE_SIZE) / 2.0;
        double cc = TP_CLOSE_SIZE / 2.0;  /* centre relatif */
        double ci = TP_CLOSE_ICON / 2.0;

        /* fond du bouton close au hover */
        if (t->close_hover) {
            cairo_set_source_rgba(cr, 0.70, 0.20, 0.20, 0.85);
            cairo_arc(cr, cx + cc, cy + cc, cc - 1.0, 0, 2 * M_PI);
            cairo_fill(cr);
            cairo_set_source_rgb(cr, 1.0, 0.85, 0.85);
        } else if (t->hover || active) {
            cairo_set_source_rgba(cr, TP_COL_TEXT_NORMAL, 0.7);
        } else {
            return;  /* pas de croix si tab inactif non survolé */
        }

        cairo_set_line_width(cr, 1.5);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        /* diagonale \ */
        cairo_move_to(cr, cx + cc - ci, cy + cc - ci);
        cairo_line_to(cr, cx + cc + ci, cy + cc + ci);
        cairo_stroke(cr);
        /* diagonale / */
        cairo_move_to(cr, cx + cc + ci, cy + cc - ci);
        cairo_line_to(cr, cx + cc - ci, cy + cc + ci);
        cairo_stroke(cr);
    }
}

/* ── Dessin d'une flèche ────────────────────────────────────────────── */
static void tp__draw_arrow(cairo_t *cr, double x, double w, double h,
                            int left, int hover)
{
    if (hover)
        cairo_set_source_rgb(cr, TP_COL_ARROW_HOVER);
    else
        cairo_set_source_rgb(cr, TP_COL_ARROW);

    double cx = x + w / 2.0;
    double cy = h / 2.0;
    double sz = 5.0;

    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    if (left) {
        cairo_move_to(cr, cx + sz * 0.5, cy - sz);
        cairo_line_to(cr, cx - sz * 0.5, cy);
        cairo_line_to(cr, cx + sz * 0.5, cy + sz);
    } else {
        cairo_move_to(cr, cx - sz * 0.5, cy - sz);
        cairo_line_to(cr, cx + sz * 0.5, cy);
        cairo_line_to(cr, cx - sz * 0.5, cy + sz);
    }
    cairo_stroke(cr);
}

/* ── Fonction de dessin principale ─────────────────────────────────── */
static void tp__draw(CairoTabPane *tp, cairo_t *cr, int w, int h)
{
    /* fond général */
    cairo_set_source_rgb(cr, TP_COL_BG);
    cairo_paint(cr);

    /* ligne de séparation bas de la barre — interrompue sous le tab actif
     * (style Eclipse : le tab actif "fusionne" avec la zone de contenu) */
    {
        double sep_y = (double)TP_TAB_H - 0.5;
        cairo_set_source_rgb(cr, TP_COL_BORDER);
        cairo_set_line_width(cr, 1.0);

        if (tp->current >= 0 && tp->current < tp->tab_count) {
            TpTab *act = &tp->tabs[tp->current];
            /* segment gauche */
            if (act->x > 0) {
                cairo_move_to(cr, 0, sep_y);
                cairo_line_to(cr, (double)act->x + 1.0, sep_y);
                cairo_stroke(cr);
            }
            /* segment droit */
            int rx = act->x + act->w - 1;
            if (rx < w) {
                cairo_move_to(cr, (double)rx, sep_y);
                cairo_line_to(cr, (double)w,  sep_y);
                cairo_stroke(cr);
            }
            /* sous le tab actif : fond tab pour effacer la ligne */
            cairo_set_source_rgb(cr, TP_COL_TAB_ACTIVE);
            cairo_set_line_width(cr, 2.0);
            cairo_move_to(cr, (double)act->x + 1.5, sep_y);
            cairo_line_to(cr, (double)(act->x + act->w) - 2.5, sep_y);
            cairo_stroke(cr);
        } else {
            cairo_move_to(cr, 0,          sep_y);
            cairo_line_to(cr, (double)w,  sep_y);
            cairo_stroke(cr);
        }
    }

    /* clip : ne dessiner les tabs que dans la zone tabs */
    int tabs_x0 = tp->show_arrows ? TP_ARROW_W : 0;
    int tabs_x1 = tp->show_arrows ? w - TP_ARROW_W : w;

    cairo_save(cr);
    cairo_rectangle(cr, (double)tabs_x0, 0, (double)(tabs_x1 - tabs_x0),
                    (double)TP_TAB_H);
    cairo_clip(cr);

    /* dessiner les tabs inactifs d'abord, puis l'actif par-dessus */
    for (int i = 0; i < tp->tab_count; i++)
        if (i != tp->current) tp__draw_tab(cr, tp, i);
    if (tp->current >= 0 && tp->current < tp->tab_count)
        tp__draw_tab(cr, tp, tp->current);

    cairo_restore(cr);

    /* flèches */
    if (tp->show_arrows) {
        /* fond des zones flèches */
        cairo_set_source_rgb(cr, TP_COL_BG);
        cairo_rectangle(cr, 0, 0, TP_ARROW_W, TP_TAB_H);
        cairo_fill(cr);
        cairo_rectangle(cr, w - TP_ARROW_W, 0, TP_ARROW_W, TP_TAB_H);
        cairo_fill(cr);

        /* séparateurs */
        cairo_set_source_rgba(cr, TP_COL_BORDER, 0.8);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, TP_ARROW_W + 0.5, 4);
        cairo_line_to(cr, TP_ARROW_W + 0.5, TP_TAB_H - 4);
        cairo_stroke(cr);
        cairo_move_to(cr, w - TP_ARROW_W - 0.5, 4);
        cairo_line_to(cr, w - TP_ARROW_W - 0.5, TP_TAB_H - 4);
        cairo_stroke(cr);

        tp__draw_arrow(cr, 0,               TP_ARROW_W, TP_TAB_H, 1,
                       tp->arrow_left_hover  && tp->scroll_offset > 0);
        tp__draw_arrow(cr, w - TP_ARROW_W,  TP_ARROW_W, TP_TAB_H, 0,
                       tp->arrow_right_hover && tp->scroll_offset < tp->tab_count - 1);

        /* flèche désactivée = plus sombre */
        if (tp->scroll_offset == 0) {
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.4);
            cairo_rectangle(cr, 0, 0, TP_ARROW_W, TP_TAB_H);
            cairo_fill(cr);
        }
        if (tp->scroll_offset >= tp->tab_count - 1) {
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.4);
            cairo_rectangle(cr, w - TP_ARROW_W, 0, TP_ARROW_W, TP_TAB_H);
            cairo_fill(cr);
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
   WndProc
   ═══════════════════════════════════════════════════════════════════ */
static LRESULT CALLBACK tp__wnd_proc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    CairoTabPane *tp = ui_get_data(CairoTabPane, hwnd);
    if (!tp) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {

    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            tp__draw(tp, ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);

        /* tracking pour WM_MOUSELEAVE */
        if (!tp->mouse_tracked) {
            TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            tp->mouse_tracked = 1;
        }

        int need_redraw = 0;

        /* flèches */
        int al = tp__hit_arrow_l(tp, mx, my);
        int ar = tp__hit_arrow_r(tp, mx, my);
        if (al != tp->arrow_left_hover  ||
            ar != tp->arrow_right_hover) {
            tp->arrow_left_hover  = al;
            tp->arrow_right_hover = ar;
            need_redraw = 1;
        }

        /* tabs */
        int hit = tp__hit_tab(tp, mx, my);
        for (int i = 0; i < tp->tab_count; i++) {
            int h_tab   = (i == hit);
            int h_close = tp__hit_close(tp, i, mx, my);
            if (tp->tabs[i].hover       != h_tab   ||
                tp->tabs[i].close_hover != h_close) {
                tp->tabs[i].hover       = h_tab;
                tp->tabs[i].close_hover = h_close;
                need_redraw = 1;
            }
        }

        if (need_redraw) ui_redraw(hwnd);
        return 0;
    }

    case WM_MOUSELEAVE: {
        tp->mouse_tracked        = 0;
        tp->arrow_left_hover     = 0;
        tp->arrow_right_hover    = 0;
        for (int i = 0; i < tp->tab_count; i++) {
            tp->tabs[i].hover       = 0;
            tp->tabs[i].close_hover = 0;
        }
        ui_redraw(hwnd);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);

        /* flèche gauche */
        if (tp__hit_arrow_l(tp, mx, my) && tp->scroll_offset > 0) {
            tp->scroll_offset--;
            tp__layout_tabs(tp);
            ui_redraw(hwnd);
            return 0;
        }

        /* flèche droite */
        if (tp__hit_arrow_r(tp, mx, my) &&
            tp->scroll_offset < tp->tab_count - 1) {
            tp->scroll_offset++;
            tp__layout_tabs(tp);
            ui_redraw(hwnd);
            return 0;
        }

        /* bouton close */
        int hit = tp__hit_tab(tp, mx, my);
        if (hit >= 0 && tp__hit_close(tp, hit, mx, my)) {
            tp_tab_close(tp, hit);
            return 0;
        }

        /* sélection tab */
        if (hit >= 0) {
            tp_tab_select(tp, hit);
            return 0;
        }
        return 0;
    }

    case WM_SIZE: {
        RECT rc; GetClientRect(hwnd, &rc);
        tp->w = rc.right;
        tp->h = rc.bottom;
        tp__layout_tabs(tp);

        /* redimensionner le contenu actif */
        if (tp->current >= 0 && tp->current < tp->tab_count) {
            HWND hc = tp->tabs[tp->current].hwnd_content;
            if (hc) {
                int ch = tp->h - TP_TAB_H;
                MoveWindow(hc, 0, TP_TAB_H, tp->w, ch > 0 ? ch : 0, TRUE);
            }
        }
        ui_redraw(hwnd);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;   /* on gère le fond dans WM_PAINT */

    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

#endif /* CAIRO_TABPANE_IMPLEMENTATION */
#endif /* CAIRO_TABPANE_H */

/*
 * ════════════════════════════════════════════════════════════════════
 * GUIDE D'INTÉGRATION dans bim.c
 * ════════════════════════════════════════════════════════════════════
 *
 *  1. INCLUDES (ordre)
 *     #include "ui/ui_backend.h"
 *     #define CAIRO_TABPANE_IMPLEMENTATION
 *     #include "ui/cairo_tabpane.h"
 *     #define CAIRO_TRANSCRIPT_IMPLEMENTATION
 *     #include "ui/cairo_transcript.h"
 *     // ... autres headers
 *
 *  2. CRÉATION (dans WM_CREATE ou après le splitter)
 *
 *     // Le TabPane prend toute la partie basse du splitter
 *     CairoTabPane *g_tabpane = tp_create(hwnd, x, y, w, bottom_h);
 *     tp_set_cb(g_tabpane, on_tab_event, NULL);
 *
 *     // Tab 0 : Transcript (fixe, sans close)
 *     int t0 = tp_tab_add(g_tabpane, "Transcript", 0);
 *     CairoTranscript *ct = ct_create(g_tabpane->hwnd, 0, TP_TAB_H, w, bottom_h - TP_TAB_H);
 *     tp_tab_set_content(g_tabpane, t0, ct_get_hwnd(ct));
 *
 *  3. AJOUT DYNAMIQUE D'UN TAB (ex: ouverture d'une query)
 *
 *     int idx = tp_tab_add(g_tabpane, "SELECT * FROM nodes", 1);
 *     BimListView *lv = blv_create(g_tabpane->hwnd, 0, TP_TAB_H, w, bottom_h - TP_TAB_H, db);
 *     blv_set_query(lv, "SELECT id, nom, x, y FROM nodes WHERE layer=1");
 *     tp_tab_set_content(g_tabpane, idx, blv_get_hwnd(lv));
 *     tp_tab_select(g_tabpane, idx);
 *
 *  4. CALLBACK
 *
 *     static void on_tab_event(int event, int tab_idx, void *ud) {
 *         switch (event) {
 *         case TP_EV_SELECT:
 *             // tab activé — rafraîchir si besoin
 *             break;
 *         case TP_EV_CLOSE:
 *             // détruire le contenu associé
 *             HWND hc = tp_tab_get_content(g_tabpane, tab_idx);
 *             if (hc) DestroyWindow(hc);
 *             break;
 *         }
 *     }
 *
 *  5. RESIZE (dans WM_SIZE du parent)
 *
 *     tp_resize(g_tabpane, x, y, new_w, new_bottom_h);
 *
 * ════════════════════════════════════════════════════════════════════
 */
