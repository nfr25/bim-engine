/*
 * cairo_menu.h  –  Popup menu Cairo, fenêtres WS_POPUP autonomes
 * ─────────────────────────────────────────────────────────────────
 *  UTILISATION MINIMALE
 *  ─────────────────────
 *    #define CAIRO_MENU_IMPLEMENTATION
 *    #include "cairo_menu.h"
 *
 *  DÉFINITION
 *  ──────────
 *    CairoMenu *m = cm_create(hwnd_parent, my_callback, userdata);
 *
 *    cm_add_item   (m, 1, "Nouveau",      "Ctrl+N", CM_FLAG_NONE);
 *    cm_add_item   (m, 2, "Ouvrir",       "Ctrl+O", CM_FLAG_NONE);
 *    cm_add_check  (m, 3, "Grille",       NULL,     CM_FLAG_NONE, 0);
 *    cm_add_sep    (m);
 *    int sub = cm_add_submenu(m, "Affichage");
 *    cm_sub_add_item (m, sub, 10, "Zoom +", "Ctrl++", CM_FLAG_NONE);
 *    cm_sub_add_item (m, sub, 11, "Zoom -", "Ctrl+-", CM_FLAG_DISABLED);
 *
 *  AFFICHAGE
 *  ─────────
 *    // Coordonnées écran (après ClientToScreen si besoin)
 *    cm_show(m, screen_x, screen_y);
 *
 *  INTÉGRATION Win32 (WndProc) — MINIMAL
 *  ──────────────────────────────────────
 *    case WM_RBUTTONDOWN: {
 *        POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
 *        ClientToScreen(hwnd, &pt);
 *        cm_show(m, pt.x, pt.y);
 *        break;
 *    }
 *    // C'est tout ! Le menu gère ses propres messages.
 *
 *  CALLBACK
 *  ────────
 *    void my_callback(int item_id, void *userdata) { ... }
 *
 *  DESTRUCTION
 *  ───────────
 *    cm_destroy(m);
 *
 *  COMPATIBILITÉ
 *  ─────────────
 *  Les fonctions cm_mouse_move / cm_mouse_down / cm_mouse_up / cm_render
 *  sont conservées comme no-ops pour ne pas casser le code existant.
 *
 * ─────────────────────────────────────────────────────────────────
 *  Dépendances : cairo, cairo-win32, gdi32
 *  Compilateur : GCC / MinGW-w64  (MSYS2)
 * ─────────────────────────────────────────────────────────────────
 */

#ifndef CAIRO_MENU_H
#define CAIRO_MENU_H

#include "ui_backend.h"

#include <windows.h>
#include <windowsx.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════
   SECTION PUBLIQUE
   ═══════════════════════════════════════════════════════════════════ */

/* ── Flags des items ─────────────────────────────────────────────── */
#define CM_FLAG_NONE       0x00
#define CM_FLAG_DISABLED   0x01

/* ── Opaque handle ───────────────────────────────────────────────── */
typedef struct CairoMenu_ CairoMenu;

/* ── Callback ────────────────────────────────────────────────────── */
typedef void (*CairoMenuCb)(int item_id, void *userdata);

/* ─── Cycle de vie ──────────────────────────────────────────────── */
CairoMenu *cm_create (HWND parent, CairoMenuCb cb, void *userdata);
void       cm_destroy(CairoMenu *m);

/* ─── Construction ──────────────────────────────────────────────── */
int  cm_add_item      (CairoMenu *m, int id, const char *label,
                       const char *shortcut, int flags);
int  cm_add_check     (CairoMenu *m, int id, const char *label,
                       const char *shortcut, int flags, int checked);
void cm_add_sep       (CairoMenu *m);
int  cm_add_submenu   (CairoMenu *m, const char *label);
int  cm_sub_add_item  (CairoMenu *m, int sub_handle,
                       int id, const char *label,
                       const char *shortcut, int flags);
int  cm_sub_add_check (CairoMenu *m, int sub_handle,
                       int id, const char *label,
                       const char *shortcut, int flags, int checked);
void cm_sub_add_sep   (CairoMenu *m, int sub_handle);

/* ─── Modification dynamique ────────────────────────────────────── */
void cm_set_checked(CairoMenu *m, int item_id, int checked);
void cm_set_enabled(CairoMenu *m, int item_id, int enabled);

/* ─── Affichage ─────────────────────────────────────────────────── */
void cm_show   (CairoMenu *m, int screen_x, int screen_y);
void cm_hide   (CairoMenu *m);
int  cm_visible(const CairoMenu *m);

/* ─── Compatibilité — no-ops ────────────────────────────────────── */
static inline int  cm_mouse_move(CairoMenu *m, int x, int y) { (void)m;(void)x;(void)y; return 0; }
static inline int  cm_mouse_down(CairoMenu *m, int x, int y) { (void)m;(void)x;(void)y; return 0; }
static inline int  cm_mouse_up  (CairoMenu *m, int x, int y) { (void)m;(void)x;(void)y; return 0; }
static inline void cm_render    (CairoMenu *m, HDC hdc)       { (void)m;(void)hdc;        }
int  cm_key(CairoMenu *m, int vk);

/* ═══════════════════════════════════════════════════════════════════
   SECTION IMPLÉMENTATION
   ═══════════════════════════════════════════════════════════════════ */
#ifdef CAIRO_MENU_IMPLEMENTATION

/* ── Constantes visuelles ───────────────────────────────────────── */
#define CM_ITEM_H        28
#define CM_SEP_H          9
#define CM_MENU_W       220
#define CM_SUB_W        200
#define CM_PADDING_X     14
#define CM_CORNER_R       6
#define CM_SHORTCUT_X   150

/* ── Couleurs ───────────────────────────────────────────────────── */
#define CM_COL_BG        0.13, 0.14, 0.18, 0.97
#define CM_COL_BG_SUB    0.11, 0.12, 0.16, 0.97
#define CM_COL_BORDER    0.30, 0.32, 0.40, 0.80
#define CM_COL_HOVER     0.25, 0.45, 0.90, 0.30
#define CM_COL_TEXT      0.92, 0.93, 0.95, 1.00
#define CM_COL_TEXT_DIS  0.45, 0.46, 0.50, 1.00
#define CM_COL_TEXT_SC   0.55, 0.60, 0.75, 1.00
#define CM_COL_SEP       0.28, 0.30, 0.38, 1.00
#define CM_COL_CHECK     0.30, 0.70, 1.00, 1.00
#define CM_COL_ARROW     0.65, 0.70, 0.85, 1.00

/* ── Types internes ─────────────────────────────────────────────── */
typedef enum { CM_KIND_ITEM, CM_KIND_CHECK, CM_KIND_SEP, CM_KIND_SUBMENU } CmKind;

typedef struct {
    CmKind kind;
    int    id;
    char   label   [64];
    char   shortcut[24];
    int    flags;
    int    checked;
    int    sub_handle;
} CmItem;

typedef struct {
    CmItem items[64];
    int    count;
    int    hovered;
    HWND   hwnd;       /* fenêtre popup du sous-menu */
} CmSub;

struct CairoMenu_ {
    CmItem items[128];
    int    count;
    CmSub  subs[16];
    int    sub_count;

    int    visible;
    int    hovered;
    int    open_sub;

    HWND   hwnd;        /* fenêtre popup principale  */
    HWND   hwnd_parent; /* fenêtre appelante          */

    CairoMenuCb cb;
    void       *userdata;
};

/* ── Forward ─────────────────────────────────────────────────────── */
static LRESULT CALLBACK cm__wnd_proc    (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK cm__sub_proc    (HWND, UINT, WPARAM, LPARAM);
static void             cm__close_sub   (CairoMenu *m);
static void             cm__close_all   (CairoMenu *m);

/* ── Helpers géométrie ──────────────────────────────────────────── */
static int cm__item_y(CairoMenu *m, int idx)
{
    int y = CM_CORNER_R;
    for (int i = 0; i < idx; i++)
        y += (m->items[i].kind == CM_KIND_SEP) ? CM_SEP_H : CM_ITEM_H;
    return y;
}

static int cm__menu_height(CairoMenu *m)
{
    int h = CM_CORNER_R * 2;
    for (int i = 0; i < m->count; i++)
        h += (m->items[i].kind == CM_KIND_SEP) ? CM_SEP_H : CM_ITEM_H;
    return h;
}

static int cm__sub_height(CmSub *s)
{
    int h = CM_CORNER_R * 2;
    for (int i = 0; i < s->count; i++)
        h += (s->items[i].kind == CM_KIND_SEP) ? CM_SEP_H : CM_ITEM_H;
    return h;
}

static int cm__item_at_y(CmItem *items, int count, int my)
{
    int y = CM_CORNER_R;
    for (int i = 0; i < count; i++) {
        int h = (items[i].kind == CM_KIND_SEP) ? CM_SEP_H : CM_ITEM_H;
        if (my >= y && my < y + h) {
            if (items[i].kind == CM_KIND_SEP)      return -1;
            if (items[i].flags & CM_FLAG_DISABLED) return -1;
            return i;
        }
        y += h;
    }
    return -1;
}

/* ── Dessin helpers ─────────────────────────────────────────────── */
static void cm__rounded_rect(cairo_t *cr, double x, double y,
                              double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -M_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r,  0,       M_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r,  M_PI/2,  M_PI);
    cairo_arc(cr, x+r,   y+r,   r,  M_PI,    3*M_PI/2);
    cairo_close_path(cr);
}

static void cm__draw_shadow(cairo_t *cr, double w, double h)
{
    for (int i = 4; i >= 1; i--) {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.12 * i);
        cm__rounded_rect(cr, i, i, w, h, CM_CORNER_R);
        cairo_fill(cr);
    }
}

static void cm__draw_check(cairo_t *cr, double cx, double cy)
{
    cairo_set_source_rgba(cr, CM_COL_CHECK);
    cairo_set_line_width(cr, 1.8);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, cx-4, cy);
    cairo_line_to(cr, cx-1, cy+3.5);
    cairo_line_to(cr, cx+4, cy-3.5);
    cairo_stroke(cr);
}

static void cm__draw_arrow(cairo_t *cr, double cx, double cy)
{
    cairo_set_source_rgba(cr, CM_COL_ARROW);
    cairo_set_line_width(cr, 1.6);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_move_to(cr, cx-3, cy-4);
    cairo_line_to(cr, cx+3, cy);
    cairo_line_to(cr, cx-3, cy+4);
    cairo_stroke(cr);
}

static void cm__draw_panel(cairo_t *cr, int w, int h, int is_sub)
{
    cm__draw_shadow(cr, w, h);

    cm__rounded_rect(cr, 0, 0, w, h, CM_CORNER_R);
    if (is_sub)
        cairo_set_source_rgba(cr, CM_COL_BG_SUB);
    else
        cairo_set_source_rgba(cr, CM_COL_BG);
    cairo_fill(cr);

    cm__rounded_rect(cr, 0.5, 0.5, w-1, h-1, CM_CORNER_R);
    cairo_set_source_rgba(cr, CM_COL_BORDER);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);
}

static void cm__draw_items(cairo_t *cr, CmItem *items, int count,
                            int hovered, double panel_w)
{
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);

    int iy = CM_CORNER_R;
    for (int i = 0; i < count; i++) {
        CmItem *it = &items[i];

        if (it->kind == CM_KIND_SEP) {
            double sy = iy + CM_SEP_H / 2.0;
            cairo_set_source_rgba(cr, CM_COL_SEP);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, CM_PADDING_X,           sy);
            cairo_line_to(cr, panel_w - CM_PADDING_X, sy);
            cairo_stroke(cr);
            iy += CM_SEP_H;
            continue;
        }

        /* Surbrillance */
        if (i == hovered) {
            cm__rounded_rect(cr, 3, iy+2, panel_w-6, CM_ITEM_H-4, 4);
            cairo_set_source_rgba(cr, CM_COL_HOVER);
            cairo_fill(cr);
        }

        int    disabled = (it->flags & CM_FLAG_DISABLED);
        double cy       = iy + CM_ITEM_H / 2.0;

        /* Checkmark */
        if (it->kind == CM_KIND_CHECK) {
            cairo_arc(cr, CM_PADDING_X + 6, cy, 6, 0, 2*M_PI);
            cairo_set_source_rgba(cr, 0.20, 0.22, 0.28, 1.0);
            cairo_fill(cr);
            cairo_arc(cr, CM_PADDING_X + 6, cy, 6, 0, 2*M_PI);
            cairo_set_source_rgba(cr, CM_COL_BORDER);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
            if (it->checked)
                cm__draw_check(cr, CM_PADDING_X + 6, cy);
        }

        /* Label */
        cairo_set_font_size(cr, 13.0);
        double label_x = CM_PADDING_X;
        if (it->kind == CM_KIND_CHECK) label_x += 18;

        if (disabled)
            cairo_set_source_rgba(cr, CM_COL_TEXT_DIS);
        else
            cairo_set_source_rgba(cr, CM_COL_TEXT);
        cairo_move_to(cr, label_x, cy + 4.5);
        cairo_show_text(cr, it->label);

        /* Raccourci */
        if (it->shortcut[0] && !disabled) {
            cairo_set_font_size(cr, 11.5);
            cairo_set_source_rgba(cr, CM_COL_TEXT_SC);
            cairo_move_to(cr, CM_SHORTCUT_X, cy + 4.0);
            cairo_show_text(cr, it->shortcut);
        }

        /* Flèche sous-menu */
        if (it->kind == CM_KIND_SUBMENU)
            cm__draw_arrow(cr, panel_w - 14, cy);

        iy += CM_ITEM_H;
    }
}

/* ── Enregistrement classes ─────────────────────────────────────── */
static void cm__register(HINSTANCE hi)
{
    static int done = 0;
    if (done) return; done = 1;

    ui_register_class("CM_Menu", cm__wnd_proc, CS_DROPSHADOW, NULL);
    ui_register_class("CM_Sub",  cm__sub_proc, CS_DROPSHADOW, NULL);
}

/* ── Fermeture sous-menu ────────────────────────────────────────── */
static void cm__close_sub(CairoMenu *m)
{
    if (m->open_sub >= 0) {
        CmSub *s = &m->subs[m->open_sub];
        if (s->hwnd) { DestroyWindow(s->hwnd); s->hwnd = NULL; }
        s->hovered  = -1;
        m->open_sub = -1;
    }
}

static void cm__close_all(CairoMenu *m)
{
    cm__close_sub(m);
    if (m->hwnd) { DestroyWindow(m->hwnd); m->hwnd = NULL; }
    m->visible  = 0;
    m->hovered  = -1;
}

/* ── Ouvrir sous-menu ───────────────────────────────────────────── */
static void cm__open_sub(CairoMenu *m, int item_idx)
{
    CmItem *it = &m->items[item_idx];
    if (it->kind != CM_KIND_SUBMENU) return;
    int sh = it->sub_handle;
    if (sh < 0 || sh >= m->sub_count) return;
    if (m->open_sub == sh) return;   /* déjà ouvert */

    cm__close_sub(m);
    m->open_sub = sh;

    CmSub *sub   = &m->subs[sh];
    sub->hovered = -1;
    int sub_h    = cm__sub_height(sub);

    /* Position : à droite de l'item parent, aligné verticalement */
    RECT wr; GetWindowRect(m->hwnd, &wr);
    int item_y   = cm__item_y(m, item_idx);
    int sx       = wr.left + CM_MENU_W + 4;
    int sy       = wr.top  + item_y;

    /* Clamp écran */
    int screen_w = GetSystemMetrics(SM_CXSCREEN);
    int screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (sx + CM_SUB_W > screen_w) sx = wr.left - CM_SUB_W - 4;
    if (sy + sub_h   > screen_h)  sy = screen_h - sub_h - 4;
    if (sy < 0) sy = 4;


    sub->hwnd = ui_popup_create("CM_Sub", m->hwnd_parent,
        sx, sy, CM_SUB_W, sub_h,
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, m);
    ShowWindow(sub->hwnd, SW_SHOWNOACTIVATE);
    SetProp(sub->hwnd, "cm_sub_idx", (HANDLE)(intptr_t)sh);
}

/* ══════════════════════════════════════════════════════════════════
   WNDPROC MENU PRINCIPAL
   ══════════════════════════════════════════════════════════════════ */
static LRESULT CALLBACK cm__wnd_proc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    CairoMenu *m = ui_get_data(CairoMenu, hwnd);
    if (!m) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            cairo_set_source_rgba(ctx.cr, 0, 0, 0, 0);
            cairo_paint(ctx.cr);
            cm__draw_panel(ctx.cr, ctx.w, ctx.h, 0);
            cm__draw_items(ctx.cr, m->items, m->count, m->hovered, CM_MENU_W);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int my = GET_Y_LPARAM(lp);
        int prev = m->hovered;
        m->hovered = cm__item_at_y(m->items, m->count, my);
        if (m->hovered != prev) {
            /* Ouvrir/fermer sous-menu */
            if (m->hovered >= 0 &&
                m->items[m->hovered].kind == CM_KIND_SUBMENU) {
                cm__open_sub(m, m->hovered);
            } else {
                cm__close_sub(m);
            }
            ui_redraw(hwnd);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        int my = GET_Y_LPARAM(lp);
        int idx = cm__item_at_y(m->items, m->count, my);
        if (idx >= 0) {
            CmItem *it = &m->items[idx];
            if (it->kind == CM_KIND_CHECK) {
                it->checked = !it->checked;
                if (m->cb) m->cb(it->id, m->userdata);
                cm__close_all(m);
            } else if (it->kind == CM_KIND_ITEM) {
                if (m->cb) m->cb(it->id, m->userdata);
                cm__close_all(m);
            }
            /* SUBMENU : géré par WM_MOUSEMOVE */
        }
        return 0;
    }

    case WM_KILLFOCUS:
        /* Fermer si on perd le focus ET que le sous-menu n'a pas le focus */
        if (m->open_sub >= 0 && m->subs[m->open_sub].hwnd) {
            if ((HWND)wp == m->subs[m->open_sub].hwnd) return 0;
        }
        cm__close_all(m);
        return 0;

    case WM_KEYDOWN:
        return cm_key(m, (int)wp) ? 0 : DefWindowProc(hwnd, msg, wp, lp);

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ══════════════════════════════════════════════════════════════════
   WNDPROC SOUS-MENU
   ══════════════════════════════════════════════════════════════════ */
static LRESULT CALLBACK cm__sub_proc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    CairoMenu *m  = (CairoMenu*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    int        sh = (int)(intptr_t)GetProp(hwnd, "cm_sub_idx");
    if (!m || sh < 0 || sh >= m->sub_count)
        return DefWindowProc(hwnd, msg, wp, lp);

    CmSub *sub = &m->subs[sh];

    switch (msg) {
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            cairo_set_source_rgba(ctx.cr, 0, 0, 0, 0);
            cairo_paint(ctx.cr);
            cm__draw_panel(ctx.cr, ctx.w, ctx.h, 1);
            cm__draw_items(ctx.cr, sub->items, sub->count, sub->hovered, CM_SUB_W);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int my = GET_Y_LPARAM(lp);
        int prev = sub->hovered;
        sub->hovered = cm__item_at_y(sub->items, sub->count, my);
        if (sub->hovered != prev)
            ui_redraw(hwnd);
        return 0;
    }

    case WM_LBUTTONUP: {
        int my = GET_Y_LPARAM(lp);
        int idx = cm__item_at_y(sub->items, sub->count, my);
        if (idx >= 0) {
            CmItem *it = &sub->items[idx];
            if (it->kind == CM_KIND_CHECK) it->checked = !it->checked;
            if (m->cb) m->cb(it->id, m->userdata);
            cm__close_all(m);
        }
        return 0;
    }

    case WM_KILLFOCUS:
        /* Fermer si focus ne va pas vers le menu principal */
        if ((HWND)wp != m->hwnd)
            cm__close_all(m);
        return 0;

    case WM_MOUSEACTIVATE:
        return MA_NOACTIVATE;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ══════════════════════════════════════════════════════════════════
   API PUBLIQUE — IMPLÉMENTATION
   ══════════════════════════════════════════════════════════════════ */

CairoMenu *cm_create(HWND parent, CairoMenuCb cb, void *userdata)
{
    CairoMenu *m = (CairoMenu*)calloc(1, sizeof(CairoMenu));
    m->cb          = cb;
    m->userdata    = userdata;
    m->hwnd_parent = parent;
    m->hovered     = -1;
    m->open_sub    = -1;

    HINSTANCE hi = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    cm__register(hi);

    /* Init hovered des sous-menus */
    for (int i = 0; i < 16; i++) m->subs[i].hovered = -1;

    return m;
}

void cm_destroy(CairoMenu *m)
{
    if (!m) return;
    cm__close_all(m);
    free(m);
}

/* ── Ajout items ────────────────────────────────────────────────── */
static CmItem *cm__new_item(CairoMenu *m)
{
    if (m->count >= 128) return NULL;
    CmItem *it = &m->items[m->count++];
    memset(it, 0, sizeof *it);
    it->sub_handle = -1;
    return it;
}

int cm_add_item(CairoMenu *m, int id, const char *label,
                const char *shortcut, int flags)
{
    CmItem *it = cm__new_item(m); if (!it) return -1;
    it->kind = CM_KIND_ITEM; it->id = id; it->flags = flags;
    strncpy(it->label,    label    ? label    : "", 63);
    strncpy(it->shortcut, shortcut ? shortcut : "", 23);
    return m->count - 1;
}

int cm_add_check(CairoMenu *m, int id, const char *label,
                 const char *shortcut, int flags, int checked)
{
    int idx = cm_add_item(m, id, label, shortcut, flags);
    if (idx < 0) return -1;
    m->items[idx].kind    = CM_KIND_CHECK;
    m->items[idx].checked = checked;
    return idx;
}

void cm_add_sep(CairoMenu *m)
{
    CmItem *it = cm__new_item(m); if (!it) return;
    it->kind = CM_KIND_SEP;
}

int cm_add_submenu(CairoMenu *m, const char *label)
{
    if (m->sub_count >= 16) return -1;
    int sh = m->sub_count++;
    m->subs[sh].count   = 0;
    m->subs[sh].hovered = -1;
    m->subs[sh].hwnd    = NULL;

    CmItem *it = cm__new_item(m); if (!it) return -1;
    it->kind       = CM_KIND_SUBMENU;
    it->sub_handle = sh;
    it->id         = -1;
    strncpy(it->label, label ? label : "", 63);
    return sh;
}

static CmItem *cm__sub_new_item(CmSub *s)
{
    if (s->count >= 64) return NULL;
    CmItem *it = &s->items[s->count++];
    memset(it, 0, sizeof *it);
    it->sub_handle = -1;
    return it;
}

int cm_sub_add_item(CairoMenu *m, int sh, int id, const char *label,
                    const char *shortcut, int flags)
{
    if (sh < 0 || sh >= m->sub_count) return -1;
    CmItem *it = cm__sub_new_item(&m->subs[sh]); if (!it) return -1;
    it->kind = CM_KIND_ITEM; it->id = id; it->flags = flags;
    strncpy(it->label,    label    ? label    : "", 63);
    strncpy(it->shortcut, shortcut ? shortcut : "", 23);
    return m->subs[sh].count - 1;
}

int cm_sub_add_check(CairoMenu *m, int sh, int id, const char *label,
                     const char *shortcut, int flags, int checked)
{
    int idx = cm_sub_add_item(m, sh, id, label, shortcut, flags);
    if (idx < 0) return -1;
    m->subs[sh].items[idx].kind    = CM_KIND_CHECK;
    m->subs[sh].items[idx].checked = checked;
    return idx;
}

void cm_sub_add_sep(CairoMenu *m, int sh)
{
    if (sh < 0 || sh >= m->sub_count) return;
    CmItem *it = cm__sub_new_item(&m->subs[sh]); if (!it) return;
    it->kind = CM_KIND_SEP;
}

/* ── Modification dynamique ─────────────────────────────────────── */
void cm_set_checked(CairoMenu *m, int item_id, int checked)
{
    for (int i = 0; i < m->count; i++)
        if (m->items[i].id == item_id) { m->items[i].checked = checked; return; }
    for (int s = 0; s < m->sub_count; s++)
        for (int i = 0; i < m->subs[s].count; i++)
            if (m->subs[s].items[i].id == item_id)
                m->subs[s].items[i].checked = checked;
}

void cm_set_enabled(CairoMenu *m, int item_id, int enabled)
{
    for (int i = 0; i < m->count; i++)
        if (m->items[i].id == item_id) {
            if (enabled) m->items[i].flags &= ~CM_FLAG_DISABLED;
            else         m->items[i].flags |=  CM_FLAG_DISABLED;
            return;
        }
    for (int s = 0; s < m->sub_count; s++)
        for (int i = 0; i < m->subs[s].count; i++)
            if (m->subs[s].items[i].id == item_id) {
                if (enabled) m->subs[s].items[i].flags &= ~CM_FLAG_DISABLED;
                else         m->subs[s].items[i].flags |=  CM_FLAG_DISABLED;
            }
}

/* ── Show / Hide ────────────────────────────────────────────────── */
void cm_show(CairoMenu *m, int screen_x, int screen_y)
{
    if (!m) return;
    cm__close_all(m);   /* fermer si déjà ouvert */

    int mh = cm__menu_height(m);
    int mw = CM_MENU_W;

    /* Clamp écran */
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    if (screen_x + mw > sw) screen_x = sw - mw - 4;
    if (screen_y + mh > sh) screen_y = sh - mh - 4;
    if (screen_x < 0) screen_x = 4;
    if (screen_y < 0) screen_y = 4;


    m->hwnd = ui_popup_create("CM_Menu", m->hwnd_parent,
        screen_x, screen_y, mw, mh,
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, m);
    ShowWindow(m->hwnd, SW_SHOWNOACTIVATE);
    m->visible  = 1;
    m->hovered  = -1;
    m->open_sub = -1;

    /* Capture souris pour détecter clic en dehors */
    //SetCapture(m->hwnd);
}

void cm_hide(CairoMenu *m)
{
    if (m) cm__close_all(m);
}

int cm_visible(const CairoMenu *m)
{
    return m ? m->visible : 0;
}

/* ── Clavier ────────────────────────────────────────────────────── */
int cm_key(CairoMenu *m, int vk)
{
    if (!m || !m->visible) return 0;

    /* Navigation sous-menu ouvert */
    if (m->open_sub >= 0) {
        CmSub *sub = &m->subs[m->open_sub];
        if (vk == VK_UP) {
            do {
                sub->hovered = (sub->hovered <= 0) ? sub->count-1 : sub->hovered-1;
            } while (sub->hovered > 0 &&
                     (sub->items[sub->hovered].kind == CM_KIND_SEP ||
                      sub->items[sub->hovered].flags & CM_FLAG_DISABLED));
            if (sub->hwnd) InvalidateRect(sub->hwnd, NULL, FALSE);
            return 1;
        }
        if (vk == VK_DOWN) {
            do {
                sub->hovered = (sub->hovered >= sub->count-1) ? 0 : sub->hovered+1;
            } while (sub->hovered < sub->count-1 &&
                     (sub->items[sub->hovered].kind == CM_KIND_SEP ||
                      sub->items[sub->hovered].flags & CM_FLAG_DISABLED));
            if (sub->hwnd) InvalidateRect(sub->hwnd, NULL, FALSE);
            return 1;
        }
        if (vk == VK_RETURN && sub->hovered >= 0) {
            CmItem *it = &sub->items[sub->hovered];
            if (it->kind == CM_KIND_CHECK) it->checked = !it->checked;
            if (m->cb) m->cb(it->id, m->userdata);
            cm__close_all(m);
            return 1;
        }
        if (vk == VK_LEFT || vk == VK_ESCAPE) {
            cm__close_sub(m);
            if (m->hwnd) InvalidateRect(m->hwnd, NULL, FALSE);
            return 1;
        }
        return 1;
    }

    /* Navigation menu principal */
    if (vk == VK_UP) {
        do {
            m->hovered = (m->hovered <= 0) ? m->count-1 : m->hovered-1;
        } while (m->hovered > 0 &&
                 (m->items[m->hovered].kind == CM_KIND_SEP ||
                  m->items[m->hovered].flags & CM_FLAG_DISABLED));
        if (m->hwnd) InvalidateRect(m->hwnd, NULL, FALSE);
        return 1;
    }
    if (vk == VK_DOWN) {
        do {
            m->hovered = (m->hovered >= m->count-1) ? 0 : m->hovered+1;
        } while (m->hovered < m->count-1 &&
                 (m->items[m->hovered].kind == CM_KIND_SEP ||
                  m->items[m->hovered].flags & CM_FLAG_DISABLED));
        if (m->hwnd) InvalidateRect(m->hwnd, NULL, FALSE);
        return 1;
    }
    if (vk == VK_RIGHT && m->hovered >= 0 &&
        m->items[m->hovered].kind == CM_KIND_SUBMENU) {
        cm__open_sub(m, m->hovered);
        if (m->open_sub >= 0) m->subs[m->open_sub].hovered = 0;
        return 1;
    }
    if (vk == VK_RETURN && m->hovered >= 0) {
        CmItem *it = &m->items[m->hovered];
        if (it->kind == CM_KIND_SUBMENU) {
            cm__open_sub(m, m->hovered);
            if (m->open_sub >= 0) m->subs[m->open_sub].hovered = 0;
        } else {
            if (it->kind == CM_KIND_CHECK) it->checked = !it->checked;
            if (m->cb) m->cb(it->id, m->userdata);
            cm__close_all(m);
        }
        return 1;
    }
    if (vk == VK_ESCAPE) { cm__close_all(m); return 1; }
    return 0;
}

#endif /* CAIRO_MENU_IMPLEMENTATION */
#endif /* CAIRO_MENU_H */
