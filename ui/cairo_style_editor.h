/*
 * cairo_style_editor.h  –  Éditeur de style modal, self-contained
 * ─────────────────────────────────────────────────────────────────
 *
 *  STRUCTURE DU DIALOGUE
 *  ──────────────────────
 *   ┌─────────────────────────────────────────┐
 *   │  Nom du style  [___________________]    │
 *   ├─────────────────────────────────────────┤
 *   │  TRAIT                                  │
 *   │  Couleur   [███] [Choisir...]           │
 *   │  Épaisseur [══════●══] 2.0 px           │
 *   │  Style     [━━━][···][─·─]              │
 *   ├─────────────────────────────────────────┤
 *   │  REMPLISSAGE                            │
 *   │  Couleur   [███] [Choisir...]           │
 *   │  Actif     [ ✓ ]                        │
 *   ├─────────────────────────────────────────┤
 *   │  TRANSPARENCE                           │
 *   │  Alpha     [══════●══] 100%             │
 *   ├─────────────────────────────────────────┤
 *   │  APERÇU ───────────────────────────────│
 *   │  ╔══════════════════════════════════╗  │
 *   │  ║   ~~~~~~ preview live ~~~~~      ║  │
 *   │  ╚══════════════════════════════════╝  │
 *   ├─────────────────────────────────────────┤
 *   │              [Annuler]  [Appliquer]     │
 *   └─────────────────────────────────────────┘
 *
 *  STRUCTURE Style
 *  ────────────────
 *    Style s;
 *    style_init_default(&s, "Mon style");
 *
 *  OUVERTURE DU DIALOGUE
 *  ──────────────────────
 *    #define CAIRO_STYLE_EDITOR_IMPLEMENTATION
 *    #include "cairo_style_editor.h"
 *
 *    if (cse_edit(hwnd_parent, &style)) {
 *        // l'utilisateur a cliqué Appliquer
 *        // style contient les nouvelles valeurs
 *        InvalidateRect(hwnd_canvas, NULL, FALSE);
 *    }
 *
 *  UTILISATION DANS draw_cb
 *  ─────────────────────────
 *    style_apply_stroke(cr, &style);
 *    cairo_stroke(cr);
 *
 *    if (style.fill_active) {
 *        style_apply_fill(cr, &style);
 *        cairo_fill(cr);
 *    }
 *
 * ─────────────────────────────────────────────────────────────────
 *  Dépendances : cairo, cairo-win32, gdi32, comdlg32
 *  Compilateur : GCC / MinGW-w64 (MSYS2)
 *  Link        : -lcairo -lgdi32 -lcomdlg32 -lm -mwindows
 * ─────────────────────────────────────────────────────────────────
 */

#ifndef CAIRO_STYLE_EDITOR_H
#define CAIRO_STYLE_EDITOR_H

#include "ui_backend.h"

#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <commdlg.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════
   SECTION PUBLIQUE — Structure Style
   ═══════════════════════════════════════════════════════════════════ */

/* ── Types de trait ──────────────────────────────────────────────── */
typedef enum {
    STROKE_SOLID = 0,   /* ————————  trait plein                     */
    STROKE_DASH,        /* - - - - -  tirets                          */
    STROKE_DOT,         /* · · · · ·  pointillés                      */
    STROKE_DASHDOT,     /* -·-·-·-    tiret-point                     */
    STROKE_COUNT
} StrokeStyle;

/* ── Structure Style ─────────────────────────────────────────────── */
typedef struct {
    char        name[64];       /* nom du style                       */

    /* Trait */
    double      stroke_r, stroke_g, stroke_b;  /* couleur trait       */
    double      stroke_width;                  /* épaisseur px         */
    StrokeStyle stroke_style;                  /* type de trait        */

    /* Remplissage */
    double      fill_r, fill_g, fill_b;        /* couleur remplissage  */
    int         fill_active;                   /* remplissage actif    */

    /* Transparence globale */
    double      alpha;                         /* 0.0 – 1.0            */
} Style;

/* ── Helpers Style → Cairo ───────────────────────────────────────── */
/* Appliquer le style de trait avant cairo_stroke()                   */
static inline void style_apply_stroke(cairo_t *cr, const Style *s)
{
    cairo_set_source_rgba(cr, s->stroke_r, s->stroke_g, s->stroke_b, s->alpha);
    cairo_set_line_width(cr, s->stroke_width);
    cairo_set_line_cap (cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    switch (s->stroke_style) {
    case STROKE_DASH:
        { double d[] = {8.0, 4.0};
          cairo_set_dash(cr, d, 2, 0); break; }
    case STROKE_DOT:
        { double d[] = {2.0, 4.0};
          cairo_set_dash(cr, d, 2, 0); break; }
    case STROKE_DASHDOT:
        { double d[] = {8.0, 4.0, 2.0, 4.0};
          cairo_set_dash(cr, d, 4, 0); break; }
    default:
        cairo_set_dash(cr, NULL, 0, 0); break;
    }
}

/* Appliquer la couleur de remplissage avant cairo_fill()             */
static inline void style_apply_fill(cairo_t *cr, const Style *s)
{
    cairo_set_source_rgba(cr, s->fill_r, s->fill_g, s->fill_b,
                          s->alpha * 0.4);  /* remplissage plus transparent */
}

/* Initialisation avec valeurs par défaut */
static inline void style_init_default(Style *s, const char *name)
{
    memset(s, 0, sizeof *s);
    strncpy(s->name, name ? name : "Style", 63);
    s->stroke_r     = 0.30; s->stroke_g = 0.70; s->stroke_b = 1.00;
    s->stroke_width = 2.0;
    s->stroke_style = STROKE_SOLID;
    s->fill_r       = 0.30; s->fill_g   = 0.70; s->fill_b   = 1.00;
    s->fill_active  = 0;
    s->alpha        = 1.0;
}

/* ── Ouverture du dialogue ───────────────────────────────────────── */
/* Retourne 1 si Appliquer, 0 si Annuler                              */
int cse_edit(HWND parent, Style *style);

/* ═══════════════════════════════════════════════════════════════════
   SECTION IMPLÉMENTATION
   ═══════════════════════════════════════════════════════════════════ */
#ifdef CAIRO_STYLE_EDITOR_IMPLEMENTATION

/* ── Dimensions dialogue ─────────────────────────────────────────── */
#define CSE_W           300
#define CSE_H           440
#define CSE_PADDING      20
#define CSE_ROW_H        28
#define CSE_SWATCH_W     28
#define CSE_SWATCH_H     20
#define CSE_PREVIEW_H    60
#define CSE_BTN_W        70
#define CSE_BTN_H        28

/* ── Couleurs UI (fonctions inline — évite les macros multi-valeurs) */
static inline void cse__col_bg     (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.13, 0.14, 0.18, a); }
static inline void cse__col_bg2    (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.10, 0.11, 0.15, a); }
static inline void cse__col_section(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.20, 0.22, 0.30, a); }
static inline void cse__col_text   (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.90, 0.92, 0.95, a); }
static inline void cse__col_text2  (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.55, 0.60, 0.75, a); }
static inline void cse__col_border (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.25, 0.27, 0.38, a); }
static inline void cse__col_hover  (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.25, 0.45, 0.90, a); }
static inline void cse__col_press  (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.20, 0.38, 0.80, a); }
static inline void cse__col_accent (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.30, 0.70, 1.00, a); }

/* ── IDs contrôles Win32 ─────────────────────────────────────────── */
#define ID_NAME_EDIT     100
#define ID_STROKE_COLOR  101
#define ID_FILL_COLOR    102
#define ID_FILL_CHECK    103
#define ID_APPLY         104
#define ID_CANCEL        105

/* ── État interne du dialogue ────────────────────────────────────── */
typedef struct {
    Style       work;        /* copie de travail                      */
    Style      *target;      /* pointeur vers le style original       */
    int         applied;     /* 1 si Appliquer cliqué                 */

    /* Zones de hit pour les contrôles Cairo */
    RECT        rc_stroke_swatch;
    RECT        rc_fill_swatch;
    RECT        rc_stroke_btn;
    RECT        rc_fill_btn;
    RECT        rc_slider_width;
    RECT        rc_slider_alpha;
    RECT        rc_stroke_style[STROKE_COUNT];
    RECT        rc_fill_check;
    RECT        rc_apply;
    RECT        rc_cancel;

    int         dragging_width;
    int         dragging_alpha;
    int         hovered_id;   /* ID zone survolée                     */
} CseState;

/* ── Conversion couleur Cairo ↔ COLORREF ────────────────────────── */
static COLORREF cse__to_colorref(double r, double g, double b)
{
    return RGB((int)(r*255), (int)(g*255), (int)(b*255));
}
static void cse__from_colorref(COLORREF c, double *r, double *g, double *b)
{
    *r = GetRValue(c) / 255.0;
    *g = GetGValue(c) / 255.0;
    *b = GetBValue(c) / 255.0;
}

/* ── Dialogue couleur Windows ────────────────────────────────────── */
static int cse__pick_color(HWND hwnd, double *r, double *g, double *b)
{
    static COLORREF custom[16] = {0};
    CHOOSECOLOR cc = {0};
    cc.lStructSize  = sizeof cc;
    cc.hwndOwner    = hwnd;
    cc.rgbResult    = cse__to_colorref(*r, *g, *b);
    cc.lpCustColors = custom;
    cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
    if (ChooseColor(&cc)) {
        cse__from_colorref(cc.rgbResult, r, g, b);
        return 1;
    }
    return 0;
}

/* ── Helpers dessin Cairo ────────────────────────────────────────── */
static void cse__rounded_rect(cairo_t *cr, double x, double y,
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

static void cse__label(cairo_t *cr, double x, double y, const char *text,
                        double size, double alpha)
{
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, size);
    cse__col_text(cr, alpha);
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}

static void cse__section_header(cairo_t *cr, double y, int w, const char *title)
{
    /* Bande de fond section */
    cse__col_section(cr, 1.0);
    cairo_rectangle(cr, 0, y, w, 22);
    cairo_fill(cr);
    /* Accent gauche */
    cse__col_accent(cr, 1.0);
    cairo_rectangle(cr, 0, y, 3, 22);
    cairo_fill(cr);
    /* Titre */
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);
    cse__col_text(cr, 0.85);
    cairo_move_to(cr, 12, y + 15);
    cairo_show_text(cr, title);
}

static void cse__swatch(cairo_t *cr, double x, double y,
                         double w, double h, double r, double g, double b)
{
    /* Damier pour visualiser la transparence */
    for (int ix = 0; ix < (int)w; ix += 6)
        for (int iy = 0; iy < (int)h; iy += 6) {
            cairo_set_source_rgb(cr, ((ix+iy)/6 % 2) ? 0.75 : 0.55, 
                                     ((ix+iy)/6 % 2) ? 0.75 : 0.55,
                                     ((ix+iy)/6 % 2) ? 0.75 : 0.55);
            cairo_rectangle(cr, x+ix, y+iy,
                            fmin(6, w-ix), fmin(6, h-iy));
            cairo_fill(cr);
        }
    cse__rounded_rect(cr, x, y, w, h, 3);
    cairo_set_source_rgb(cr, r, g, b);
    cairo_fill(cr);
    cse__rounded_rect(cr, x+0.5, y+0.5, w-1, h-1, 3);
    cse__col_border(cr, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
}

static void cse__button(cairo_t *cr, double x, double y,
                         double w, double h, const char *label,
                         int hovered, int pressed, int accent)
{
    cse__rounded_rect(cr, x, y, w, h, 5);
    if (accent)
        cse__col_press(cr, pressed ? 1.0 : (hovered ? 0.90 : 0.75));
    else
        cse__col_section(cr, 1.0);
    cairo_fill_preserve(cr);
    cse__col_border(cr, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    cairo_text_extents_t te;
    cairo_text_extents(cr, label, &te);
    cse__col_text(cr, 1.0);
    cairo_move_to(cr, x + (w - te.width) / 2.0 - te.x_bearing,
                      y + (h - te.height) / 2.0 - te.y_bearing);
    cairo_show_text(cr, label);
}

static void cse__small_btn(cairo_t *cr, double x, double y,
                            double w, double h, const char *label,
                            int hovered)
{
    cse__rounded_rect(cr, x, y, w, h, 4);
    cse__col_section(cr, hovered ? 0.85 : 1.0);
    cairo_fill_preserve(cr);
    cse__col_border(cr, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);
    cairo_text_extents_t te;
    cairo_text_extents(cr, label, &te);
    cse__col_text(cr, 1.0);
    cairo_move_to(cr, x + (w - te.width) / 2.0 - te.x_bearing,
                      y + (h - te.height) / 2.0 - te.y_bearing);
    cairo_show_text(cr, label);
}

static void cse__slider(cairo_t *cr, double x, double y,
                         double w, double val, double vmin, double vmax,
                         int hovered)
{
    double h = 6.0;
    double cy = y + h / 2.0;
    double t  = (val - vmin) / (vmax - vmin);
    double kx = x + t * w;

    /* Rail */
    cse__rounded_rect(cr, x, cy - h/2, w, h, 3);
    cse__col_bg2(cr, 1.0);
    cairo_fill(cr);

    /* Rempli */
    if (t > 0) {
        cse__rounded_rect(cr, x, cy - h/2, t * w, h, 3);
        cse__col_accent(cr, 0.85);
        cairo_fill(cr);
    }

    /* Knob */
    cairo_arc(cr, kx, cy, hovered ? 7.0 : 5.5, 0, 2*M_PI);
    cse__col_text(cr, 1.0);
    cairo_fill(cr);
    cairo_arc(cr, kx, cy, hovered ? 7.0 : 5.5, 0, 2*M_PI);
    cse__col_accent(cr, 1.0);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
}

static void cse__stroke_style_btn(cairo_t *cr, double x, double y,
                                   double w, double h, StrokeStyle ss,
                                   int selected, int hovered)
{
    cse__rounded_rect(cr, x, y, w, h, 4);
    if (selected)
        cse__col_press(cr, 0.90);
    else if (hovered)
        cse__col_hover(cr, 0.25);
    else
        cse__col_section(cr, 1.0);
    cairo_fill_preserve(cr);
    cse__col_border(cr, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    /* Aperçu du type de trait */
    double mx = x + w/2;
    double my = y + h/2;
    cse__col_text(cr, selected ? 1.0 : 0.70);
    cairo_set_line_width(cr, 1.8);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    switch (ss) {
    case STROKE_SOLID:
        cairo_set_dash(cr, NULL, 0, 0);
        break;
    case STROKE_DASH:
        { double d[] = {5,3}; cairo_set_dash(cr, d, 2, 0); break; }
    case STROKE_DOT:
        { double d[] = {1.5,3}; cairo_set_dash(cr, d, 2, 0); break; }
    case STROKE_DASHDOT:
        { double d[] = {5,3,1.5,3}; cairo_set_dash(cr, d, 4, 0); break; }
    default: break;
    }
    cairo_move_to(cr, x+6, my);
    cairo_line_to(cr, x+w-6, my);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
}

static void cse__checkbox(cairo_t *cr, double x, double y,
                           double sz, int checked, int hovered)
{
    cse__rounded_rect(cr, x, y, sz, sz, 3);
    if (hovered)      cse__col_hover(cr, 0.40);
    else if (checked) cse__col_press(cr, 1.0);
    else              cse__col_section(cr, 1.0);
    cairo_fill_preserve(cr);
    cse__col_border(cr, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    if (checked) {
        cse__col_text(cr, 1.0);
        cairo_set_line_width(cr, 2.0);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
        cairo_move_to(cr, x+3,       y+sz*0.55);
        cairo_line_to(cr, x+sz*0.42, y+sz-4);
        cairo_line_to(cr, x+sz-3,    y+3);
        cairo_stroke(cr);
    }
}

/* ── Aperçu style ────────────────────────────────────────────────── */
static void cse__draw_preview(cairo_t *cr, double x, double y,
                               double w, double h, const Style *s)
{
    /* Fond */
    cse__rounded_rect(cr, x, y, w, h, 5);
    cairo_set_source_rgb(cr, 0.08, 0.09, 0.12);
    cairo_fill_preserve(cr);
    cse__col_border(cr, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    /* Clip dans le preview */
    cairo_save(cr);
    cse__rounded_rect(cr, x+1, y+1, w-2, h-2, 4);
    cairo_clip(cr);

    double mx = x + w/2, my = y + h/2;

    /* Polygone de test */
    if (s->fill_active) {
        cairo_move_to(cr, mx-50, my+18);
        cairo_line_to(cr, mx-20, my-20);
        cairo_line_to(cr, mx+20, my-22);
        cairo_line_to(cr, mx+52, my+15);
        cairo_line_to(cr, mx+10, my+22);
        cairo_close_path(cr);
        style_apply_fill(cr, s);
        cairo_fill(cr);
    }

    /* Polyligne de test */
    double pts[][2] = {
        {mx-60, my+10}, {mx-30, my-15}, {mx,    my+5},
        {mx+30, my-18}, {mx+60, my+8}
    };
    cairo_move_to(cr, pts[0][0], pts[0][1]);
    for (int i = 1; i < 5; i++)
        cairo_line_to(cr, pts[i][0], pts[i][1]);
    style_apply_stroke(cr, s);
    cairo_stroke(cr);

    /* Nœuds */
    cairo_set_dash(cr, NULL, 0, 0);
    for (int i = 0; i < 5; i++) {
        cairo_arc(cr, pts[i][0], pts[i][1], 3.0, 0, 2*M_PI);
        cairo_set_source_rgba(cr, s->stroke_r, s->stroke_g, s->stroke_b, s->alpha);
        cairo_fill(cr);
    }

    cairo_restore(cr);
}

/* ── Rendu principal du dialogue ─────────────────────────────────── */
static void cse__render(CseState *st, cairo_t *cr, int w, int h)
{

    /* Fond global */
    cse__col_bg(cr, 1.0);
    cairo_paint(cr);

    Style *s = &st->work;
    double px = CSE_PADDING;
    double cw = w - CSE_PADDING * 2;
    double y  = 12;

    /* ── Nom du style ── */
    //cse__label(cr, px, y + 14, "Nom du style", 11.5, 0.70);
    /* L'HWND Edit Win32 se dessine par-dessus, on laisse la zone */
    //y += CSE_ROW_H + 8;

    /* ══ SECTION TRAIT ══ */
    cse__section_header(cr, y, w, "TRAIT");
    y += 26;

    /* Couleur de trait */
    cse__label(cr, px, y + 14, "Couleur", 12.0, 0.85);
    double sw_x = px + 70;
    cse__swatch(cr, sw_x, y + 2, CSE_SWATCH_W, CSE_SWATCH_H,
                s->stroke_r, s->stroke_g, s->stroke_b);
    SetRect(&st->rc_stroke_swatch, (int)sw_x, (int)(y+2),
            (int)(sw_x+CSE_SWATCH_W), (int)(y+2+CSE_SWATCH_H));

    double sb_x = sw_x + CSE_SWATCH_W + 8;
    cse__small_btn(cr, sb_x, y, 70, 22, "Choisir...",
                   st->hovered_id == ID_STROKE_COLOR);
    SetRect(&st->rc_stroke_btn, (int)sb_x, (int)y,
            (int)(sb_x+70), (int)(y+22));
    y += CSE_ROW_H;

    /* Épaisseur */
    cse__label(cr, px, y + 14, "Épaisseur", 12.0, 0.85);
    double sl_x = px + 90, sl_w = cw - 90 - 50;
    cse__slider(cr, sl_x, y + 8, sl_w, s->stroke_width, 0.5, 10.0,
                st->dragging_width || st->hovered_id == 201);
    SetRect(&st->rc_slider_width, (int)sl_x, (int)y,
            (int)(sl_x+sl_w), (int)(y+CSE_ROW_H));

    char buf[16];
    snprintf(buf, sizeof buf, "%.1f px", s->stroke_width);
    cse__label(cr, sl_x + sl_w + 8, y + 14, buf, 11.5, 0.70);
    y += CSE_ROW_H;

    /* Style de trait */
    cse__label(cr, px, y + 14, "Style", 12.0, 0.85);
    double ss_x = px + 70, ss_w = 42, ss_gap = 6;
    const char *ss_tips[] = {"Plein", "Tirets", "Points", "Tiret-pt"};
    (void)ss_tips;
    for (int i = 0; i < STROKE_COUNT; i++) {
        double bx = ss_x + i * (ss_w + ss_gap);
        cse__stroke_style_btn(cr, bx, y, ss_w, 24,
                              (StrokeStyle)i,
                              s->stroke_style == i,
                              st->hovered_id == 210 + i);
        SetRect(&st->rc_stroke_style[i], (int)bx, (int)y,
                (int)(bx+ss_w), (int)(y+24));
    }
    y += CSE_ROW_H + 4;

    /* ══ SECTION REMPLISSAGE ══ */
    cse__section_header(cr, y, w, "REMPLISSAGE");
    y += 26;

    /* Couleur de remplissage */
    cse__label(cr, px, y + 14, "Couleur", 12.0, 0.85);
    cse__swatch(cr, sw_x, y + 2, CSE_SWATCH_W, CSE_SWATCH_H,
                s->fill_r, s->fill_g, s->fill_b);
    SetRect(&st->rc_fill_swatch, (int)sw_x, (int)(y+2),
            (int)(sw_x+CSE_SWATCH_W), (int)(y+2+CSE_SWATCH_H));

    cse__small_btn(cr, sb_x, y, 70, 22, "Choisir...",
                   st->hovered_id == ID_FILL_COLOR);
    SetRect(&st->rc_fill_btn, (int)sb_x, (int)y,
            (int)(sb_x+70), (int)(y+22));
    y += CSE_ROW_H;

    /* Actif */
    cse__label(cr, px, y + 14, "Actif", 12.0, 0.85);
    cse__checkbox(cr, px + 70, y + 2, 18, s->fill_active,
                  st->hovered_id == ID_FILL_CHECK);
    SetRect(&st->rc_fill_check, (int)(px+70), (int)(y+2),
            (int)(px+88), (int)(y+20));
    y += CSE_ROW_H + 4;

    /* ══ SECTION TRANSPARENCE ══ */
    cse__section_header(cr, y, w, "TRANSPARENCE");
    y += 26;

    cse__label(cr, px, y + 14, "Alpha", 12.0, 0.85);
    cse__slider(cr, sl_x, y + 8, sl_w, s->alpha, 0.0, 1.0,
                st->dragging_alpha || st->hovered_id == 202);
    SetRect(&st->rc_slider_alpha, (int)sl_x, (int)y,
            (int)(sl_x+sl_w), (int)(y+CSE_ROW_H));

    snprintf(buf, sizeof buf, "%.0f%%", s->alpha * 100.0);
    cse__label(cr, sl_x + sl_w + 8, y + 14, buf, 11.5, 0.70);
    y += CSE_ROW_H + 8;

    /* ══ APERÇU ══ */
    cse__section_header(cr, y, w, "APERÇU");
    y += 26;
    cse__draw_preview(cr, px, y, cw, CSE_PREVIEW_H, s);
    y += CSE_PREVIEW_H + 12;

    /* ══ BOUTONS ══ */
    double btn_y = h - CSE_BTN_H - 12;
    double apply_x  = w - CSE_PADDING - CSE_BTN_W;
    double cancel_x = apply_x - CSE_BTN_W - 10;

    cse__button(cr, cancel_x, btn_y, CSE_BTN_W, CSE_BTN_H, "Annuler",
                st->hovered_id == ID_CANCEL, 0, 0);
    SetRect(&st->rc_cancel, (int)cancel_x, (int)btn_y,
            (int)(cancel_x+CSE_BTN_W), (int)(btn_y+CSE_BTN_H));

    cse__button(cr, apply_x, btn_y, CSE_BTN_W, CSE_BTN_H, "Appliquer",
                st->hovered_id == ID_APPLY, 0, 1);
    SetRect(&st->rc_apply, (int)apply_x, (int)btn_y,
            (int)(apply_x+CSE_BTN_W), (int)(btn_y+CSE_BTN_H));

    /* Ligne de séparation boutons */
    cse__col_border(cr, 0.6);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, btn_y - 8);
    cairo_line_to(cr, w, btn_y - 8);
    cairo_stroke(cr);

}

/* ── Hit testing ─────────────────────────────────────────────────── */
static int cse__pt_in_rect(RECT *r, int x, int y)
{
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

static double cse__slider_val(RECT *rc, int mx, double vmin, double vmax)
{
    double t = (double)(mx - rc->left) / (rc->right - rc->left);
    if (t < 0) t = 0; 
    if (t > 1) t = 1;
    return vmin + t * (vmax - vmin);
}

/* ── WndProc dialogue ────────────────────────────────────────────── */
static LRESULT CALLBACK cse__dlg_proc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp)
{
    CseState *st = (CseState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_CREATE: {
        /* Créer le HWND Edit pour le nom */
        /*HWND edit = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            CSE_PADDING + 90, 8, CSE_W - CSE_PADDING*2 - 90, 22,
            hwnd, (HMENU)ID_NAME_EDIT,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
        // Fond sombre sur l'edit 
        (void)edit;*/
        return 0;
    }
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            cse__render(st, ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }
    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        int prev = st->hovered_id;
        st->hovered_id = 0;

        if (cse__pt_in_rect(&st->rc_stroke_btn,   mx, my)) st->hovered_id = ID_STROKE_COLOR;
        if (cse__pt_in_rect(&st->rc_fill_btn,      mx, my)) st->hovered_id = ID_FILL_COLOR;
        if (cse__pt_in_rect(&st->rc_fill_check,    mx, my)) st->hovered_id = ID_FILL_CHECK;
        if (cse__pt_in_rect(&st->rc_slider_width,  mx, my)) st->hovered_id = 201;
        if (cse__pt_in_rect(&st->rc_slider_alpha,  mx, my)) st->hovered_id = 202;
        if (cse__pt_in_rect(&st->rc_apply,         mx, my)) st->hovered_id = ID_APPLY;
        if (cse__pt_in_rect(&st->rc_cancel,        mx, my)) st->hovered_id = ID_CANCEL;
        for (int i = 0; i < STROKE_COUNT; i++)
            if (cse__pt_in_rect(&st->rc_stroke_style[i], mx, my))
                st->hovered_id = 210 + i;

        /* Drag sliders */
        if (st->dragging_width)
            st->work.stroke_width = cse__slider_val(&st->rc_slider_width,
                                                     mx, 0.5, 10.0);
        if (st->dragging_alpha)
            st->work.alpha = cse__slider_val(&st->rc_slider_alpha, mx, 0.0, 1.0);

        if (st->hovered_id != prev || st->dragging_width || st->dragging_alpha)
            ui_redraw(hwnd);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        SetCapture(hwnd);

        if (cse__pt_in_rect(&st->rc_slider_width, mx, my)) {
            st->dragging_width = 1;
            st->work.stroke_width = cse__slider_val(&st->rc_slider_width,
                                                     mx, 0.5, 10.0);
            ui_redraw(hwnd);
        }
        else if (cse__pt_in_rect(&st->rc_slider_alpha, mx, my)) {
            st->dragging_alpha = 1;
            st->work.alpha = cse__slider_val(&st->rc_slider_alpha, mx, 0.0, 1.0);
            ui_redraw(hwnd);
        }
        else if (cse__pt_in_rect(&st->rc_stroke_btn, mx, my)) {
            cse__pick_color(hwnd, &st->work.stroke_r,
                                  &st->work.stroke_g,
                                  &st->work.stroke_b);
            ui_redraw(hwnd);
        }
        else if (cse__pt_in_rect(&st->rc_fill_btn, mx, my)) {
            cse__pick_color(hwnd, &st->work.fill_r,
                                  &st->work.fill_g,
                                  &st->work.fill_b);
            ui_redraw(hwnd);
        }
        else if (cse__pt_in_rect(&st->rc_fill_check, mx, my)) {
            st->work.fill_active = !st->work.fill_active;
            ui_redraw(hwnd);
        }
        else {
            for (int i = 0; i < STROKE_COUNT; i++)
                if (cse__pt_in_rect(&st->rc_stroke_style[i], mx, my)) {
                    st->work.stroke_style = (StrokeStyle)i;
                    ui_redraw(hwnd);
                }
        }
        return 0;
    }
    case WM_LBUTTONUP: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        ReleaseCapture();
        st->dragging_width = 0;
        st->dragging_alpha = 0;

        if (cse__pt_in_rect(&st->rc_apply, mx, my)) {
            /* Récupérer le nom depuis l'HWND Edit */
            HWND edit = GetDlgItem(hwnd, ID_NAME_EDIT);
            char buf[64] = {0};
            GetWindowText(edit, buf, 63);
            if (buf[0]) strncpy(st->work.name, buf, 63);
            st->applied = 1;
            DestroyWindow(hwnd);
        }
        else if (cse__pt_in_rect(&st->rc_cancel, mx, my)) {
            DestroyWindow(hwnd);
        }
        return 0;
    }
    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) DestroyWindow(hwnd);
        if (wp == VK_RETURN) {
            HWND edit = GetDlgItem(hwnd, ID_NAME_EDIT);
            char buf[64] = {0};
            GetWindowText(edit, buf, 63);
            if (buf[0]) strncpy(st->work.name, buf, 63);
            st->applied = 1;
            DestroyWindow(hwnd);
        }
        return 0;
    case WM_DESTROY:
       return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ── Point d'entrée public ───────────────────────────────────────── */
int cse_edit(HWND parent, Style *style)
{
    /* État */
    CseState st = {0};
    st.work    = *style;
    st.target  = style;
    st.applied = 0;


    /* Enregistrement classe (une seule fois) */
    static int reg = 0;
    if (!reg) {
        reg = 1;
        WNDCLASSEX wc = {0};
        wc.cbSize        = sizeof wc;
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        ui_register_class("CSE_Dialog", cse__dlg_proc, 0, NULL);
    }

    /* Centrer sur le parent */
    RECT pr; GetWindowRect(parent, &pr);
    int x = pr.left + (pr.right  - pr.left  - CSE_W) / 2;
    int y = pr.top  + (pr.bottom - pr.top   - CSE_H) / 2;

    HWND hwnd = ui_popup_create("CSE_Dialog", parent,
        x, y, CSE_W, CSE_H,
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, &st);
    ShowWindow(hwnd, SW_SHOW);
    ui_set_dark_mode(parent);

    /* Pré-remplir le nom dans l'Edit */
    HWND edit = GetDlgItem(hwnd, ID_NAME_EDIT);
    SetWindowText(edit, st.work.name);

    /* Désactiver le parent (comportement modal) */
    EnableWindow(parent, FALSE);
    ShowWindow(hwnd, SW_SHOW);

    ui_set_dark_mode(hwnd);
    UpdateWindow(hwnd);

    /* Boucle modale */
    MSG msg;
    while (IsWindow(hwnd) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    EnableWindow(parent, TRUE);
    SetForegroundWindow(parent);

    if (st.applied)
        *style = st.work;

    return st.applied;
}

#endif /* CAIRO_STYLE_EDITOR_IMPLEMENTATION */
#endif /* CAIRO_STYLE_EDITOR_H */
