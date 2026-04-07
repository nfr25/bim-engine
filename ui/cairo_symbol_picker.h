/*
 * cairo_symbol_picker.h  –  Sélecteur de symbole SVG modal, self-contained
 * ─────────────────────────────────────────────────────────────────────────
 *
 *  STRUCTURE DU DIALOGUE
 *  ──────────────────────
 *   ┌─────────────────────────────────────────────────┐
 *   │  ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐ ┌──────┐  │
 *   │  │ SVG  │ │ SVG  │ │ SVG  │ │ SVG  │ │ SVG  │  │
 *   │  │      │ │      │ │      │ │      │ │      │  │
 *   │  │Vanne │ │Inter.│ │Transf│ │ Nœud │ │Angle │  │
 *   │  └──────┘ └──────┘ └──────┘ └──────┘ └──────┘  │
 *   ├── APERÇU ────────────────────────────────────── │
 *   │  ┌────────────┐   Nom du symbole               │
 *   │  │            │   Ports : E W (bitmask)         │
 *   │  │  preview   │   [↺]  0°  [↻]                 │
 *   │  │            │                                 │
 *   │  └────────────┘                                 │
 *   ├─────────────────────────────────────────────────┤
 *   │                   [Annuler]  [Sélectionner]     │
 *   └─────────────────────────────────────────────────┘
 *
 *  UTILISATION
 *  ────────────
 *    #define CAIRO_SYMBOL_PICKER_IMPLEMENTATION
 *    #include "cairo_symbol_picker.h"
 *
 *    CspSymbol my_symbols[] = {
 *        // Depuis SVG inline
 *        { "Vanne", "Hydraulique", "<svg...>", NULL,
 *          PORT_BIT(PORT_E)|PORT_BIT(PORT_W), 20, 20, 1 },
 *        // Depuis handle pré-compilé (ex: cache SQLite)
 *        { "Nœud",  "Topologie",   NULL, my_rsvg_handle,
 *          PORT_BIT(PORT_N)|PORT_BIT(PORT_S), 20, 20, 1 },
 *        { NULL }  // sentinel
 *    };
 *
 *    CspResult result;
 *    if (csp_pick(hwnd_parent, my_symbols, &result)) {
 *        // result.symbol_idx  index dans my_symbols
 *        // result.angle       angle initial (radians)
 *    }
 *
 *  PORTS
 *  ──────
 *    PORT_N=0 PORT_NE=1 PORT_E=2 PORT_SE=3
 *    PORT_S=4 PORT_SW=5 PORT_W=6 PORT_NW=7
 *    #define PORT_BIT(p) (1 << (p))
 *
 * ─────────────────────────────────────────────────────────────────────────
 *  Dépendances : cairo, cairo-win32, librsvg-2.0, gdi32, glib-2.0
 *  Compilateur : GCC / MinGW-w64 (MSYS2)
 *  Packages    : pacman -S mingw-w64-x86_64-librsvg
 *  Link        : $(shell pkg-config --cflags --libs librsvg-2.0 glib-2.0)
 *                -lcairo -lgdi32 -lcomdlg32 -ldwmapi -lm -mwindows
 * ─────────────────────────────────────────────────────────────────────────
 */

#ifndef CAIRO_SYMBOL_PICKER_H
#define CAIRO_SYMBOL_PICKER_H

#include <windows.h>
#include <windowsx.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <librsvg/rsvg.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════════
   SECTION PUBLIQUE
   ═══════════════════════════════════════════════════════════════════════ */

typedef enum {
    PORT_N=0, PORT_NE=1, PORT_E=2, PORT_SE=3,
    PORT_S=4, PORT_SW=5, PORT_W=6, PORT_NW=7
} CspPortPos;

#define PORT_BIT(p)  (1 << (p))

typedef struct {
    const char *name;
    const char *category;
    const char *svg_text;    /* SVG inline — utilisé si rsvg == NULL        */
    RsvgHandle *rsvg;        /* handle pré-compilé — prioritaire sur svg_text */
    int         ports_mask;
    double      width;
    double      height;
    int         port_max;
    int         key;
} CspSymbol;

typedef struct {
    int    symbol_idx;
    double angle;
} CspResult;

/* Retourne 1 si Sélectionner, 0 si Annuler */
int csp_pick(HWND parent, const CspSymbol *symbols, CspResult *result);

/* ═══════════════════════════════════════════════════════════════════════
   SECTION IMPLÉMENTATION
   ═══════════════════════════════════════════════════════════════════════ */
#ifdef CAIRO_SYMBOL_PICKER_IMPLEMENTATION

/* ── Dimensions ──────────────────────────────────────────────────────── */
#define CSP_W           520
#define CSP_H           520
#define CSP_PADDING      16
#define CSP_THUMB_W      80
#define CSP_THUMB_H      80
#define CSP_THUMB_PAD     6
#define CSP_THUMB_LABEL  16
#define CSP_COLS          5
#define CSP_PREVIEW_SZ  120
#define CSP_PREVIEW_X    16
#define CSP_PREVIEW_Y   340
#define CSP_BTN_W        90
#define CSP_BTN_H        28
#define CSP_GRID_Y       12

/* ── Couleurs ────────────────────────────────────────────────────────── */
static inline void csp__col_bg    (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.13, 0.14, 0.18, a); }
static inline void csp__col_bg2   (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.10, 0.11, 0.15, a); }
static inline void csp__col_border(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.25, 0.27, 0.38, a); }
static inline void csp__col_text  (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.90, 0.92, 0.95, a); }
static inline void csp__col_text2 (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.55, 0.60, 0.75, a); }
static inline void csp__col_hover (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.25, 0.45, 0.90, a); }
static inline void csp__col_press (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.20, 0.38, 0.80, a); }
static inline void csp__col_accent(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.30, 0.70, 1.00, a); }
static inline void csp__col_sel   (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.20, 0.55, 1.00, a); }

static const char *CSP_PORT_NAMES[] = { "N","NE","E","SE","S","SW","W","NW" };
static const double CSP_PORT_OX[]   = { 0,  1,  1,  1,  0, -1, -1, -1 };
static const double CSP_PORT_OY[]   = {-1, -1,  0,  1,  1,  1,  0, -1 };

/* ── État interne ────────────────────────────────────────────────────── */
typedef struct {
    const CspSymbol *symbols;
    int              sym_count;
    CspResult       *result;
    int              confirmed;
    int              selected;
    int              hovered;
    double           angle;
    RsvgHandle     **rsvg_cache;
    RECT             rc_apply;
    RECT             rc_cancel;
    RECT             rc_rot_left;
    RECT             rc_rot_right;
    RECT            *rc_thumbs;
    int              grid_rows;
    int              scroll_y;
} CspState;

/* ── Helpers dessin ──────────────────────────────────────────────────── */
static void csp__rounded_rect(cairo_t *cr, double x, double y,
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

static void csp__section_header(cairo_t *cr, double y, int w, const char *title)
{
    cairo_set_source_rgba(cr, 0.20, 0.22, 0.30, 1.0);
    cairo_rectangle(cr, 0, y, w, 20);
    cairo_fill(cr);
    cairo_set_source_rgba(cr, 0.30, 0.70, 1.00, 1.0);
    cairo_rectangle(cr, 0, y, 3, 20);
    cairo_fill(cr);
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 10.5);
    csp__col_text(cr, 0.80);
    cairo_move_to(cr, 10, y + 14);
    cairo_show_text(cr, title);
}

/* ── Chargement RsvgHandle ───────────────────────────────────────────── */
static RsvgHandle *csp__get_rsvg(CspState *st, int idx)
{
    /* Handle pré-compilé fourni directement — on ne touche pas au cache */
    if (st->symbols[idx].rsvg)
        return st->symbols[idx].rsvg;

    /* Sinon charger depuis svg_text avec cache interne */
    if (!st->rsvg_cache[idx]) {
        const char *svg = st->symbols[idx].svg_text;
        if (!svg || !svg[0]) return NULL;
        GError *err = NULL;
        st->rsvg_cache[idx] = rsvg_handle_new_from_data(
            (const guint8*)svg, (gsize)strlen(svg), &err);
        if (err) {
            fprintf(stderr, "SVG[%d] erreur: %s\n", idx, err->message);
            g_error_free(err);
            st->rsvg_cache[idx] = NULL;
        }
    }
    return st->rsvg_cache[idx];
}

/* ── Dimensions SVG robuste (viewBox ou width/height) ────────────────── */
static void csp__svg_size(RsvgHandle *rsvg, gdouble *out_w, gdouble *out_h)
{
    *out_w = 50.0;
    *out_h = 50.0;

#if LIBRSVG_MAJOR_VERSION > 2 || \
    (LIBRSVG_MAJOR_VERSION == 2 && LIBRSVG_MINOR_VERSION >= 52)
    /* API moderne : get_intrinsic_dimensions lit le viewBox */
    RsvgLength  w_len = {0}, h_len = {0};
    RsvgRectangle vb  = {0};
    gboolean has_w = FALSE, has_h = FALSE, has_vb = FALSE;

    rsvg_handle_get_intrinsic_dimensions(rsvg,
        &has_w, &w_len,
        &has_h, &h_len,
        &has_vb, &vb);

    if (has_vb && vb.width > 0 && vb.height > 0) {
        *out_w = vb.width;
        *out_h = vb.height;
    } else if (has_w && has_h && w_len.length > 0 && h_len.length > 0) {
        *out_w = w_len.length;
        *out_h = h_len.length;
    }
#else
    /* API ancienne */
    RsvgDimensionData dim = {0};
    rsvg_handle_get_dimensions(rsvg, &dim);
    if (dim.width  > 0) *out_w = dim.width;
    if (dim.height > 0) *out_h = dim.height;
#endif

    /* Garde-fous */
    if (*out_w <= 0 || !isfinite(*out_w)) *out_w = 50.0;
    if (*out_h <= 0 || !isfinite(*out_h)) *out_h = 50.0;
}

/* ── Rendu SVG centré dans une zone ──────────────────────────────────── */
static void csp__draw_svg(cairo_t *cr, RsvgHandle *rsvg,
                           double cx, double cy, double size, double angle)
{
    if (!rsvg) {
        /* Placeholder */
        csp__rounded_rect(cr, cx - size/2, cy - size/2, size, size, 4);
        cairo_set_source_rgba(cr, 0.3, 0.3, 0.4, 0.5);
        cairo_fill(cr);
        cairo_select_font_face(cr, "Segoe UI",
            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 10.0);
        cairo_set_source_rgba(cr, 0.5, 0.5, 0.6, 0.8);
        cairo_move_to(cr, cx - 8, cy + 4);
        cairo_show_text(cr, "SVG");
        return;
    }

    gdouble svg_w, svg_h;
    csp__svg_size(rsvg, &svg_w, &svg_h);

    double scale = (size * 0.75) / fmax(svg_w, svg_h);
    if (!isfinite(scale) || scale <= 0) scale = 1.0;

    cairo_save(cr);
    cairo_translate(cr, cx, cy);
    cairo_rotate(cr, angle);
    cairo_scale(cr, scale, scale);
    cairo_translate(cr, -svg_w / 2.0, -svg_h / 2.0);

#if LIBRSVG_MAJOR_VERSION > 2 || \
    (LIBRSVG_MAJOR_VERSION == 2 && LIBRSVG_MINOR_VERSION >= 52)
    RsvgRectangle viewport = { 0, 0, svg_w, svg_h };
    rsvg_handle_render_document(rsvg, cr, &viewport, NULL);
#else
    rsvg_handle_render_cairo(rsvg, cr);
#endif

    cairo_restore(cr);
}

/* ── Dessin des ports ────────────────────────────────────────────────── */
static void csp__draw_ports(cairo_t *cr, int ports_mask,
                             double cx, double cy,
                             double pw, double ph, double angle)
{
    for (int p = 0; p < 8; p++) {
        if (!(ports_mask & (1 << p))) continue;
        double lx = CSP_PORT_OX[p] * pw;
        double ly = CSP_PORT_OY[p] * ph;
        double cos_a = cos(angle), sin_a = sin(angle);
        double rx = lx * cos_a - ly * sin_a;
        double ry = lx * sin_a + ly * cos_a;
        double px = cx + rx, py = cy + ry;
        cairo_arc(cr, px, py, 4.0, 0, 2*M_PI);
        csp__col_accent(cr, 0.90);
        cairo_fill(cr);
        cairo_arc(cr, px, py, 4.0, 0, 2*M_PI);
        csp__col_bg2(cr, 1.0);
        cairo_set_line_width(cr, 1.5);
        cairo_stroke(cr);
    }
}

/* ── Rendu principal ─────────────────────────────────────────────────── */
static void csp__render(CspState *st, HDC hdc, int w, int h)
{
    cairo_surface_t *surf = cairo_win32_surface_create(hdc);
    cairo_t         *cr   = cairo_create(surf);

    /* Fond */
    csp__col_bg(cr, 1.0);
    cairo_paint(cr);

    /* ══ GRILLE ══ */
    csp__section_header(cr, CSP_GRID_Y, w, "SYMBOLES");
    double gy = CSP_GRID_Y + 24;
    double grid_area_h = CSP_PREVIEW_Y - gy - 8;

    cairo_save(cr);
    cairo_rectangle(cr, 0, gy, w, grid_area_h);
    cairo_clip(cr);

    for (int i = 0; i < st->sym_count; i++) {
        int col = i % CSP_COLS;
        int row = i / CSP_COLS;

        double tx = CSP_PADDING + col * (CSP_THUMB_W + CSP_THUMB_PAD);
        double ty = gy + row * (CSP_THUMB_H + CSP_THUMB_PAD + CSP_THUMB_LABEL)
                    + CSP_THUMB_PAD - st->scroll_y;

        SetRect(&st->rc_thumbs[i],
                (int)tx, (int)ty,
                (int)(tx + CSP_THUMB_W),
                (int)(ty + CSP_THUMB_H + CSP_THUMB_LABEL));

        /* Fond vignette */
        csp__rounded_rect(cr, tx, ty, CSP_THUMB_W, CSP_THUMB_H, 6);
        if (i == st->selected) {
            csp__col_sel(cr, 0.30);
            cairo_fill_preserve(cr);
            csp__col_sel(cr, 0.90);
            cairo_set_line_width(cr, 1.5);
        } else if (i == st->hovered) {
            csp__col_hover(cr, 0.20);
            cairo_fill_preserve(cr);
            csp__col_border(cr, 0.60);
            cairo_set_line_width(cr, 1.0);
        } else {
            csp__col_bg2(cr, 1.0);
            cairo_fill_preserve(cr);
            csp__col_border(cr, 0.40);
            cairo_set_line_width(cr, 1.0);
        }
        cairo_stroke(cr);

        /* SVG */
        double tcx = tx + CSP_THUMB_W / 2.0;
        double tcy = ty + CSP_THUMB_H / 2.0;
        RsvgHandle *rsvg = csp__get_rsvg(st, i);

        cairo_save(cr);
        csp__rounded_rect(cr, tx+2, ty+2, CSP_THUMB_W-4, CSP_THUMB_H-4, 4);
        cairo_clip(cr);
        csp__draw_svg(cr, rsvg, tcx, tcy, CSP_THUMB_W - 16, 0.0);
        cairo_restore(cr);

        /* Petits ports sur vignette sélectionnée/survolée */
        if (i == st->selected || i == st->hovered) {
            double pw = st->symbols[i].width  > 0 ? st->symbols[i].width  : 15.0;
            double ph = st->symbols[i].height > 0 ? st->symbols[i].height : 15.0;
            double ps = (CSP_THUMB_W - 16) * 0.5 / fmax(pw, ph);
            for (int p = 0; p < 8; p++) {
                if (!(st->symbols[i].ports_mask & (1 << p))) continue;
                double px2 = tcx + CSP_PORT_OX[p] * pw * ps;
                double py2 = tcy + CSP_PORT_OY[p] * ph * ps;
                cairo_arc(cr, px2, py2, 2.5, 0, 2*M_PI);
                csp__col_accent(cr, 0.80);
                cairo_fill(cr);
            }
        }

        /* Label */
        cairo_select_font_face(cr, "Segoe UI",
            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 9.5);
        cairo_text_extents_t te;
        cairo_text_extents(cr, st->symbols[i].name, &te);
        double lx2 = tx + (CSP_THUMB_W - te.width) / 2.0 - te.x_bearing;
        if (lx2 < tx + 2) lx2 = tx + 2;

        /* Clip label dans la vignette */
        cairo_save(cr);
        cairo_rectangle(cr, tx, ty + CSP_THUMB_H, CSP_THUMB_W, CSP_THUMB_LABEL);
        cairo_clip(cr);
        if (i == st->selected) csp__col_accent(cr, 1.0);
        else                   csp__col_text2 (cr, 0.90);
        cairo_move_to(cr, lx2, ty + CSP_THUMB_H + 11);
        cairo_show_text(cr, st->symbols[i].name);
        cairo_restore(cr);
    }

    cairo_restore(cr);  /* fin clip grille */

    /* ══ APERÇU ══ */
    csp__section_header(cr, CSP_PREVIEW_Y, w, "APER\xc3\x87U");
    double py = CSP_PREVIEW_Y + 26;

    if (st->selected >= 0 && st->selected < st->sym_count) {
        const CspSymbol *sym  = &st->symbols[st->selected];
        RsvgHandle      *rsvg = csp__get_rsvg(st, st->selected);

        double pvx = CSP_PREVIEW_X, pvy = py;
        double pvw = CSP_PREVIEW_SZ, pvh = CSP_PREVIEW_SZ;

        /* Fond aperçu */
        csp__rounded_rect(cr, pvx, pvy, pvw, pvh, 6);
        cairo_set_source_rgb(cr, 0.07, 0.08, 0.11);
        cairo_fill_preserve(cr);
        csp__col_border(cr, 0.7);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        /* Axes */
        double pcx = pvx + pvw/2.0, pcy = pvy + pvh/2.0;
        csp__col_border(cr, 0.25);
        cairo_set_line_width(cr, 0.5);
        cairo_move_to(cr, pvx+4, pcy); cairo_line_to(cr, pvx+pvw-4, pcy);
        cairo_move_to(cr, pcx, pvy+4); cairo_line_to(cr, pcx, pvy+pvh-4);
        cairo_stroke(cr);

        /* SVG + ports */
        cairo_save(cr);
        csp__rounded_rect(cr, pvx+1, pvy+1, pvw-2, pvh-2, 5);
        cairo_clip(cr);
        csp__draw_svg(cr, rsvg, pcx, pcy, pvw - 20, st->angle);
        double pw2 = sym->width  > 0 ? sym->width  : 15.0;
        double ph2 = sym->height > 0 ? sym->height : 15.0;
        double ps2 = (pvw - 24) * 0.5 / fmax(pw2, ph2);
        csp__draw_ports(cr, sym->ports_mask, pcx, pcy,
                        pw2 * ps2, ph2 * ps2, st->angle);
        cairo_restore(cr);

        /* Infos à droite */
        double ix = CSP_PREVIEW_X + CSP_PREVIEW_SZ + 16;
        double iy = py + 12;

        cairo_select_font_face(cr, "Segoe UI",
            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        cairo_set_font_size(cr, 13.0);
        csp__col_text(cr, 1.0);
        cairo_move_to(cr, ix, iy); cairo_show_text(cr, sym->name);
        iy += 20;

        cairo_select_font_face(cr, "Segoe UI",
            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0);
        csp__col_text2(cr, 0.80);
        cairo_move_to(cr, ix, iy);
        cairo_show_text(cr, sym->category ? sym->category : "\xe2\x80\x94");
        iy += 22;

        /* Ports */
        csp__col_text2(cr, 0.70);
        cairo_move_to(cr, ix, iy); cairo_show_text(cr, "Ports :");
        iy += 16;
        char port_buf[64] = "";
        for (int p = 0; p < 8; p++) {
            if (!(sym->ports_mask & (1 << p))) continue;
            if (port_buf[0]) strcat(port_buf, "  ");
            strcat(port_buf, CSP_PORT_NAMES[p]);
        }
        csp__col_accent(cr, 0.90);
        cairo_move_to(cr, ix + 4, iy);
        cairo_show_text(cr, port_buf[0] ? port_buf : "aucun");
        iy += 26;

        /* Rotation */
        csp__col_text2(cr, 0.70);
        cairo_set_font_size(cr, 11.0);
        cairo_move_to(cr, ix, iy); cairo_show_text(cr, "Rotation :");
        iy += 18;

        double bw = 28, bh = 24;

        /* ↺ */
        csp__rounded_rect(cr, ix, iy, bw, bh, 4);
        csp__col_bg2(cr, 1.0);
        cairo_fill_preserve(cr);
        csp__col_border(cr, 0.7);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
        cairo_set_font_size(cr, 14.0);
        csp__col_text(cr, 0.90);
        cairo_move_to(cr, ix + 5, iy + 17);
        cairo_show_text(cr, "\xe2\x86\xba");
        SetRect(&st->rc_rot_left, (int)ix, (int)iy,
                (int)(ix+bw), (int)(iy+bh));

        /* Angle */
        char ang_buf[16];
        int deg = (int)round(st->angle * 180.0 / M_PI) % 360;
        if (deg < 0) deg += 360;
        snprintf(ang_buf, sizeof ang_buf, " %d\xc2\xb0 ", deg);
        cairo_set_font_size(cr, 12.0);
        csp__col_text(cr, 1.0);
        cairo_text_extents_t tea;
        cairo_text_extents(cr, ang_buf, &tea);
        cairo_move_to(cr, ix + bw + 4, iy + bh * 0.72);
        cairo_show_text(cr, ang_buf);

        /* ↻ */
        double bx2 = ix + bw + 8 + tea.width;
        csp__rounded_rect(cr, bx2, iy, bw, bh, 4);
        csp__col_bg2(cr, 1.0);
        cairo_fill_preserve(cr);
        csp__col_border(cr, 0.7);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);
        cairo_set_font_size(cr, 14.0);
        csp__col_text(cr, 0.90);
        cairo_move_to(cr, bx2 + 5, iy + 17);
        cairo_show_text(cr, "\xe2\x86\xbb");
        SetRect(&st->rc_rot_right, (int)bx2, (int)iy,
                (int)(bx2+bw), (int)(iy+bh));

    } else {
        cairo_select_font_face(cr, "Segoe UI",
            CAIRO_FONT_SLANT_ITALIC, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12.0);
        csp__col_text2(cr, 0.50);
        cairo_move_to(cr, CSP_PREVIEW_X + 10, py + 30);
        cairo_show_text(cr, "Cliquez sur un symbole");
    }

    /* ══ SÉPARATION + BOUTONS ══ */
    double btn_y = h - CSP_BTN_H - 12;
    csp__col_border(cr, 0.5);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, btn_y - 8); cairo_line_to(cr, w, btn_y - 8);
    cairo_stroke(cr);

    double apply_x  = w - CSP_PADDING - CSP_BTN_W;
    double cancel_x = apply_x - CSP_BTN_W - 10;

    /* Annuler */
    csp__rounded_rect(cr, cancel_x, btn_y, CSP_BTN_W, CSP_BTN_H, 5);
    csp__col_bg2(cr, 1.0);
    cairo_fill_preserve(cr);
    csp__col_border(cr, 1.0);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    cairo_text_extents_t te2;
    cairo_text_extents(cr, "Annuler", &te2);
    csp__col_text(cr, 1.0);
    cairo_move_to(cr, cancel_x + (CSP_BTN_W - te2.width)/2.0 - te2.x_bearing,
                      btn_y   + (CSP_BTN_H - te2.height)/2.0 - te2.y_bearing);
    cairo_show_text(cr, "Annuler");
    SetRect(&st->rc_cancel, (int)cancel_x, (int)btn_y,
            (int)(cancel_x+CSP_BTN_W), (int)(btn_y+CSP_BTN_H));

    /* Sélectionner */
    int has_sel = (st->selected >= 0);
    csp__rounded_rect(cr, apply_x, btn_y, CSP_BTN_W, CSP_BTN_H, 5);
    if (has_sel) csp__col_press(cr, 0.80);
    else         csp__col_bg2  (cr, 1.0);
    cairo_fill_preserve(cr);
    csp__col_border(cr, has_sel ? 1.0 : 0.4);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
    const char *sel_label = "S\xc3\xa9lectionner";
    cairo_text_extents_t te3;
    cairo_text_extents(cr, sel_label, &te3);
    if (has_sel) csp__col_text (cr, 1.0);
    else         csp__col_text2(cr, 0.5);
    cairo_move_to(cr, apply_x + (CSP_BTN_W - te3.width)/2.0 - te3.x_bearing,
                      btn_y   + (CSP_BTN_H - te3.height)/2.0 - te3.y_bearing);
    cairo_show_text(cr, sel_label);
    SetRect(&st->rc_apply, (int)apply_x, (int)btn_y,
            (int)(apply_x+CSP_BTN_W), (int)(btn_y+CSP_BTN_H));

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

/* ── Hit testing ─────────────────────────────────────────────────────── */
static int csp__pt_in(RECT *r, int x, int y) {
    return x >= r->left && x < r->right && y >= r->top && y < r->bottom;
}

/* ── WndProc ─────────────────────────────────────────────────────────── */
static LRESULT CALLBACK csp__dlg_proc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp)
{
    CspState *st = (CspState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    (void)lp;

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int cw = rc.right, ch = rc.bottom;
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, cw, ch);
        HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
        if (st) csp__render(st, mem, cw, ch);
        BitBlt(hdc, 0, 0, cw, ch, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE: {
        if (!st) return 0;
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        int prev = st->hovered;
        st->hovered = -1;
        for (int i = 0; i < st->sym_count; i++)
            if (csp__pt_in(&st->rc_thumbs[i], mx, my))
                st->hovered = i;
        if (st->hovered != prev) InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        if (!st) return 0;
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);

        for (int i = 0; i < st->sym_count; i++) {
            if (csp__pt_in(&st->rc_thumbs[i], mx, my)) {
                st->selected = i;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
        }
        if (csp__pt_in(&st->rc_rot_left, mx, my)) {
            st->angle -= M_PI / 2.0;
            if (st->angle < 0) st->angle += 2 * M_PI;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (csp__pt_in(&st->rc_rot_right, mx, my)) {
            st->angle += M_PI / 2.0;
            if (st->angle >= 2 * M_PI) st->angle -= 2 * M_PI;
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (csp__pt_in(&st->rc_cancel, mx, my)) {
            DestroyWindow(hwnd);
            return 0;
        }
        if (csp__pt_in(&st->rc_apply, mx, my) && st->selected >= 0) {
            st->result->symbol_idx = st->selected;
            st->result->angle      = st->angle;
            st->confirmed = 1;
            DestroyWindow(hwnd);
            return 0;
        }
        return 0;
    }
    case WM_LBUTTONDBLCLK: {
        if (!st) return 0;
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        for (int i = 0; i < st->sym_count; i++) {
            if (csp__pt_in(&st->rc_thumbs[i], mx, my)) {
                st->selected           = i;
                st->result->symbol_idx = i;
                st->result->angle      = st->angle;
                st->confirmed = 1;
                DestroyWindow(hwnd);
                return 0;
            }
        }
        return 0;
    }
    case WM_MOUSEWHEEL: {
        if (!st) return 0;
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        st->scroll_y -= delta / 3;
        if (st->scroll_y < 0) st->scroll_y = 0;
        int max_scroll = st->grid_rows
            * (CSP_THUMB_H + CSP_THUMB_PAD + CSP_THUMB_LABEL);
        if (st->scroll_y > max_scroll) st->scroll_y = max_scroll;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }
    case WM_KEYDOWN:
        if (!st) return 0;
        switch (wp) {
        case VK_ESCAPE: DestroyWindow(hwnd); break;
        case VK_RETURN:
            if (st->selected >= 0) {
                st->result->symbol_idx = st->selected;
                st->result->angle      = st->angle;
                st->confirmed = 1;
                DestroyWindow(hwnd);
            }
            break;
        case VK_LEFT:
            if (st->selected > 0)
                { st->selected--; InvalidateRect(hwnd, NULL, FALSE); }
            break;
        case VK_RIGHT:
            if (st->selected < st->sym_count - 1)
                { st->selected++; InvalidateRect(hwnd, NULL, FALSE); }
            break;
        case VK_UP:
            if (st->selected >= CSP_COLS)
                { st->selected -= CSP_COLS; InvalidateRect(hwnd, NULL, FALSE); }
            break;
        case VK_DOWN:
            if (st->selected + CSP_COLS < st->sym_count)
                { st->selected += CSP_COLS; InvalidateRect(hwnd, NULL, FALSE); }
            break;
        case 'R':
            st->angle += M_PI / 2.0;
            if (st->angle >= 2 * M_PI) st->angle -= 2 * M_PI;
            InvalidateRect(hwnd, NULL, FALSE);
            break;
        }
        return 0;

    case WM_DESTROY:
        /* PAS de PostQuitMessage ici — boucle modale via IsWindow() */
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ── Point d'entrée public ───────────────────────────────────────────── */
int csp_pick(HWND parent, const CspSymbol *symbols, CspResult *result)
{
    int count = 0;
    while (symbols[count].name) count++;
    if (count == 0) return 0;

    CspState st   = {0};
    st.symbols    = symbols;
    st.sym_count  = count;
    st.result     = result;
    st.selected   = (result && result->symbol_idx >= 0 && result->symbol_idx < count)
                    ? result->symbol_idx : 0;
    st.hovered    = -1;
    st.angle      = (result) ? result->angle : 0.0;
    st.grid_rows  = (count + CSP_COLS - 1) / CSP_COLS;

    /* Scroller pour que le symbole sélectionné soit visible */
    if (st.selected > 0) {
        int row = st.selected / CSP_COLS;
        st.scroll_y = row * (CSP_THUMB_H + CSP_THUMB_PAD + CSP_THUMB_LABEL);
    }

    st.rsvg_cache = (RsvgHandle**)calloc(count, sizeof(RsvgHandle*));
    st.rc_thumbs  = (RECT*)       calloc(count, sizeof(RECT));

    /* Zones de hit initialisées à zéro — safe avant le premier rendu */
    SetRectEmpty(&st.rc_apply);
    SetRectEmpty(&st.rc_cancel);
    SetRectEmpty(&st.rc_rot_left);
    SetRectEmpty(&st.rc_rot_right);

    HINSTANCE hi = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);

    static int reg = 0;
    if (!reg) {
        reg = 1;
        WNDCLASSEX wc = {0};
        wc.cbSize        = sizeof wc;
        wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
        wc.lpfnWndProc   = csp__dlg_proc;
        wc.hInstance     = hi;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "CSP_Dialog";
        RegisterClassEx(&wc);
    }

    RECT pr; GetWindowRect(parent, &pr);
    int x = pr.left + (pr.right  - pr.left  - CSP_W) / 2;
    int y = pr.top  + (pr.bottom - pr.top   - CSP_H) / 2;

    HWND hwnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "CSP_Dialog", "S\xc3\xa9lection de symbole",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        x, y, CSP_W, CSP_H,
        parent, NULL, hi, NULL);

    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)&st);

    EnableWindow(parent, FALSE);
    ShowWindow(hwnd, SW_SHOW);

    /* Dark mode titlebar */
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof dark);

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

    /* Libérer uniquement les handles chargés par le picker lui-même
     * (pas les handles pré-compilés fournis via CspSymbol.rsvg) */
    for (int i = 0; i < count; i++)
        if (st.rsvg_cache[i] && !st.symbols[i].rsvg)
            g_object_unref(st.rsvg_cache[i]);
    free(st.rsvg_cache);
    free(st.rc_thumbs);

    if (st.confirmed) *result = *st.result;
    return st.confirmed;
}

#endif /* CAIRO_SYMBOL_PICKER_IMPLEMENTATION */
#endif /* CAIRO_SYMBOL_PICKER_H */
