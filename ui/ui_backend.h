/*
 * ui_backend.h — Couche d'abstraction Win32/Cairo pour bim-engine/ui
 *
 * Objectif : concentrer tout le boilerplate Win32 ici.
 * Chaque custom control ne connaît plus que Cairo (cairo_t*) et
 * ses propres messages métier.
 *
 * Usage (pattern single-header) :
 *   #define UI_BACKEND_IMPLEMENTATION   (dans un seul .h ou bim.c)
 *   #include "ui_backend.h"
 *
 * ── Ce que fournit ce header ────────────────────────────────────
 *
 *  1. UiWndClass  — enregistrement d'une classe Win32 en une ligne
 *  2. UiSubWnd    — création d'une sous-fenêtre Cairo en une ligne
 *  3. UiDrawCtx   — wrapping cairo_win32_surface + cairo_t
 *  4. UI_PAINT_*  — macros WM_PAINT : double-buffer + Cairo en 2 lignes
 *  5. ui_redraw() — InvalidateRect wrapper
 *
 * ── Ce que NE fait PAS ce header ────────────────────────────────
 *  - Pas de WndProc générique : chaque contrôle garde le sien,
 *    mais n'a plus à toucher PAINTSTRUCT / CreateCompatibleDC / BitBlt.
 *  - Pas de message loop : bim.c garde son WinMain.
 *  - Pas de gestion d'événements : chaque contrôle gère WM_MOUSE*, etc.
 *
 * ── Portabilité ─────────────────────────────────────────────────
 *  Pour un futur portage Linux/X11, il suffira de fournir une
 *  autre implémentation de UiDrawCtx et des macros UI_PAINT_*.
 *  Les contrôles eux-mêmes n'auront pas à changer.
 */

#ifndef UI_BACKEND_H
#define UI_BACKEND_H

#include <stdbool.h>
#include <cairo.h>

/* ═══════════════════════════════════════════════════════════════════
   SECTION WIN32  (cachée derrière #ifdef _WIN32 pour portabilité future)
   ═══════════════════════════════════════════════════════════════════ */

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>   /* GET_X_LPARAM, GET_Y_LPARAM */
#include <cairo-win32.h>

/* ───────────────────────────────────────────────────────────────────
   1. UiWndClass — enregistrement d'une classe Win32
   ─────────────────────────────────────────────────────────────────── */

/*
 * Enregistre une classe Win32 avec les réglages standard dark-mode.
 * Appel idempotent : si la classe existe déjà, RegisterClassEx échoue
 * silencieusement (GetLastError() == ERROR_CLASS_ALREADY_EXISTS).
 *
 * Paramètres :
 *   class_name  — nom de la classe (ex: "BIM_Toolbar")
 *   wnd_proc    — WndProc du contrôle
 *   style_extra — flags CS_* supplémentaires (0 pour défaut)
 *                 ex: CS_DROPSHADOW pour les popups
 *   cursor      — curseur (NULL → IDC_ARROW)
 *
 * Retourne true si la classe est disponible (déjà enregistrée ou
 * enregistrée à l'instant).
 */
static inline bool ui_register_class(const char  *class_name,
                                      WNDPROC      wnd_proc,
                                      UINT         style_extra,
                                      HCURSOR      cursor)
{
    HINSTANCE hi = GetModuleHandle(NULL);

    WNDCLASSEX wc   = {0};
    wc.cbSize        = sizeof wc;
    wc.style         = CS_HREDRAW | CS_VREDRAW | style_extra;
    wc.lpfnWndProc   = wnd_proc;
    wc.hInstance     = hi;
    wc.hCursor       = cursor ? cursor : LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = class_name;

    if (!RegisterClassEx(&wc)) {
        /* Seul ERROR_CLASS_ALREADY_EXISTS est acceptable */
        return GetLastError() == ERROR_CLASS_ALREADY_EXISTS;
    }
    return true;
}

/* Variante : enregistrement avec flag "done" statique.
 * Utile pour grouper plusieurs classes dans une fonction.
 * Copier-coller ce bloc dans chaque xxx__register_classes() :
 *
 *   static void mymodule_register(void) {
 *       static int done = 0; if (done) return; done = 1;
 *       ui_register_class("MY_Wnd",     my_proc,     0,              NULL);
 *       ui_register_class("MY_Popup",   my_pop_proc, CS_DROPSHADOW,  NULL);
 *       ui_register_class("MY_Resize",  my_rsz_proc, 0, LoadCursor(NULL, IDC_SIZENS));
 *   }
 */

/* ───────────────────────────────────────────────────────────────────
   2. UiSubWnd — création d'une sous-fenêtre enfant
   ─────────────────────────────────────────────────────────────────── */

/*
 * Crée une sous-fenêtre WS_CHILD | WS_VISIBLE et attache userdata
 * via GWLP_USERDATA en une seule opération.
 *
 * Remplace le pattern :
 *   l->hwnd_foo = CreateWindowEx(0, "CL_Foo", NULL,
 *       WS_CHILD | WS_VISIBLE, x, y, w, h, parent, NULL, hi, NULL);
 *   SetWindowLongPtr(l->hwnd_foo, GWLP_USERDATA, (LONG_PTR)l);
 *
 * Par :
 *   l->hwnd_foo = ui_subwnd_create("CL_Foo", parent, x, y, w, h, l);
 */
static inline HWND ui_subwnd_create(const char *class_name,
                                     HWND        parent,
                                     int x, int y, int w, int h,
                                     void       *userdata)
{
    HINSTANCE hi = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    HWND hwnd = CreateWindowEx(
        0, class_name, NULL,
        WS_CHILD | WS_VISIBLE,
        x, y, w, h,
        parent, NULL, hi, NULL);
    if (hwnd && userdata)
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)userdata);
    return hwnd;
}

/*
 * Variante pour les fenêtres popup (menus, tooltips, dropdowns).
 * Utilise WS_POPUP | WS_CLIPSIBLINGS et des flags ex_style fournis.
 *
 * Remplace :
 *   hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_NOACTIVATE,
 *       "CM_Menu", NULL, WS_POPUP | WS_CLIPSIBLINGS,
 *       x, y, w, h, parent, NULL, hi, NULL);
 *   SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)m);
 *
 * Par :
 *   hwnd = ui_popup_create("CM_Menu", parent, x, y, w, h,
 *                           WS_EX_TOPMOST | WS_EX_NOACTIVATE, m);
 */
static inline HWND ui_popup_create(const char *class_name,
                                    HWND        parent,
                                    int x, int y, int w, int h,
                                    DWORD       ex_style,
                                    void       *userdata)
{
    HINSTANCE hi = GetModuleHandle(NULL);
    HWND hwnd = CreateWindowEx(
        ex_style, class_name, NULL,
        WS_POPUP | WS_CLIPSIBLINGS,
        x, y, w, h,
        parent, NULL, hi, NULL);
    if (hwnd && userdata)
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)userdata);
    return hwnd;
}

/* ───────────────────────────────────────────────────────────────────
   3. UiDrawCtx — surface Cairo sur HDC Win32
   ─────────────────────────────────────────────────────────────────── */

/*
 * Contexte de dessin Cairo lié à un HDC Win32.
 * Utilisé à la fois dans les draw functions et dans les macros WM_PAINT.
 *
 * Cycle de vie :
 *   UiDrawCtx ctx;
 *   ui_draw_begin(&ctx, hdc, w, h);
 *       // utiliser ctx.cr pour dessiner
 *   ui_draw_end(&ctx);
 *
 * Remplace dans chaque draw function :
 *   cairo_surface_t *surf = cairo_win32_surface_create(hdc);
 *   cairo_t         *cr   = cairo_create(surf);
 *   ...
 *   cairo_destroy(cr);
 *   cairo_surface_destroy(surf);
 */
typedef struct {
    cairo_surface_t *surface;
    cairo_t         *cr;
    int              w, h;
} UiDrawCtx;

static inline void ui_draw_begin(UiDrawCtx *ctx, HDC hdc, int w, int h)
{
    ctx->surface = cairo_win32_surface_create(hdc);
    ctx->cr      = cairo_create(ctx->surface);
    ctx->w       = w;
    ctx->h       = h;
}

static inline void ui_draw_end(UiDrawCtx *ctx)
{
    cairo_destroy(ctx->cr);
    cairo_surface_destroy(ctx->surface);
    ctx->cr      = NULL;
    ctx->surface = NULL;
}

/* ───────────────────────────────────────────────────────────────────
   4. Macros WM_PAINT — double-buffer + Cairo
   ─────────────────────────────────────────────────────────────────── */

/*
 * Pattern complet WM_PAINT avec double-buffer GDI + surface Cairo.
 *
 * Avant (6 lignes de plomberie par WndProc, répétées ~12 fois) :
 *
 *   case WM_PAINT: {
 *       PAINTSTRUCT ps;
 *       HDC hdc = BeginPaint(hwnd, &ps);
 *       RECT rc; GetClientRect(hwnd, &rc);
 *       int w = rc.right, h = rc.bottom;
 *       HDC mem = CreateCompatibleDC(hdc);
 *       HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
 *       HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
 *       my_draw_fn(data, mem, w, h);         // ← seule ligne unique
 *       BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
 *       SelectObject(mem, old);
 *       DeleteObject(bmp); DeleteDC(mem);
 *       EndPaint(hwnd, &ps);
 *       return 0;
 *   }
 *
 * Après (2 lignes) :
 *
 *   case WM_PAINT: {
 *       UI_PAINT_BEGIN(hwnd, ctx);
 *           my_draw_fn(data, ctx.cr, ctx.w, ctx.h);
 *       UI_PAINT_END(hwnd, ctx);
 *   }
 *
 * ────────────────────────────────────────────────────────────────
 * IMPORTANT : les draw functions doivent avoir la signature :
 *   void my_draw_fn(MyData *data, cairo_t *cr, int w, int h);
 *
 * Elles ne prennent plus de HDC — elles travaillent directement
 * avec le cairo_t fourni par la macro.
 * ────────────────────────────────────────────────────────────────
 *
 * UI_PAINT_BEGIN déclare :
 *   PAINTSTRUCT _ui_ps;
 *   HDC         _ui_hdc_screen;
 *   HDC         _ui_hdc_mem;
 *   HBITMAP     _ui_bmp, _ui_bmp_old;
 *   UiDrawCtx   <nom_fourni>;     ← accessible entre BEGIN et END
 */

#define UI_PAINT_BEGIN(hwnd, ctx)                                       \
    do {                                                                 \
        PAINTSTRUCT _ui_ps;                                              \
        HDC _ui_hdc_screen = BeginPaint((hwnd), &_ui_ps);               \
        RECT _ui_rc; GetClientRect((hwnd), &_ui_rc);                    \
        int _ui_w = _ui_rc.right, _ui_h = _ui_rc.bottom;               \
        HDC _ui_hdc_mem = CreateCompatibleDC(_ui_hdc_screen);           \
        HBITMAP _ui_bmp = CreateCompatibleBitmap(                       \
                            _ui_hdc_screen, _ui_w, _ui_h);              \
        HBITMAP _ui_bmp_old =                                           \
            (HBITMAP)SelectObject(_ui_hdc_mem, _ui_bmp);                \
        UiDrawCtx ctx;                                                   \
        ui_draw_begin(&(ctx), _ui_hdc_mem, _ui_w, _ui_h);

#define UI_PAINT_END(hwnd, ctx)                                         \
        ui_draw_end(&(ctx));                                             \
        BitBlt(_ui_hdc_screen, 0, 0, _ui_w, _ui_h,                     \
               _ui_hdc_mem, 0, 0, SRCCOPY);                             \
        SelectObject(_ui_hdc_mem, _ui_bmp_old);                         \
        DeleteObject(_ui_bmp);                                           \
        DeleteDC(_ui_hdc_mem);                                           \
        EndPaint((hwnd), &_ui_ps);                                       \
    } while(0)

/*
 * Variante légère sans double-buffer — utile pour les fenêtres
 * dont le contenu ne clignote pas (ex: canvas principal géré ailleurs).
 *
 *   case WM_PAINT: {
 *       UI_PAINT_DIRECT_BEGIN(hwnd, ctx);
 *           my_draw_fn(data, ctx.cr, ctx.w, ctx.h);
 *       UI_PAINT_DIRECT_END(hwnd, ctx);
 *   }
 */
#define UI_PAINT_DIRECT_BEGIN(hwnd, ctx)                                \
    do {                                                                 \
        PAINTSTRUCT _ui_ps;                                              \
        HDC _ui_hdc_screen = BeginPaint((hwnd), &_ui_ps);               \
        RECT _ui_rc; GetClientRect((hwnd), &_ui_rc);                    \
        UiDrawCtx ctx;                                                   \
        ui_draw_begin(&(ctx), _ui_hdc_screen,                           \
                      _ui_rc.right, _ui_rc.bottom);

#define UI_PAINT_DIRECT_END(hwnd, ctx)                                  \
        ui_draw_end(&(ctx));                                             \
        EndPaint((hwnd), &_ui_ps);                                       \
    } while(0)

/* ───────────────────────────────────────────────────────────────────
   5. Utilitaires
   ─────────────────────────────────────────────────────────────────── */

/* Invalide et redessine une fenêtre (sans effacer le fond). */
static inline void ui_redraw(HWND hwnd)
{
    InvalidateRect(hwnd, NULL, FALSE);
    /* UpdateWindow() optionnel : décommenter si redraw immédiat requis */
    /* UpdateWindow(hwnd); */
}

/*
 * Récupère le userdata d'une fenêtre (GWLP_USERDATA) casté au bon type.
 * Usage :
 *   MyData *d = ui_get_data(MyData, hwnd);
 */
#define ui_get_data(Type, hwnd) \
    ((Type*)GetWindowLongPtr((hwnd), GWLP_USERDATA))

/*
 * Active le dark mode DWM sur une fenêtre (Windows 10 1809+).
 * À appeler APRÈS ShowWindow() sur les popups WS_POPUP.
 *
 * Usage : ui_set_dark_mode(hwnd);
 */
static inline void ui_set_dark_mode(HWND hwnd)
{
    /* DWMWA_USE_IMMERSIVE_DARK_MODE = 20 */
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof dark);
}
/* dwmapi.h requis — à linker avec -ldwmapi */
#include <dwmapi.h>

/* ═══════════════════════════════════════════════════════════════════
   FIN SECTION WIN32
   ═══════════════════════════════════════════════════════════════════ */

#endif /* _WIN32 */
#endif /* UI_BACKEND_H */

/*
 * ════════════════════════════════════════════════════════════════════
 * GUIDE DE MIGRATION — comment adapter un custom control existant
 * ════════════════════════════════════════════════════════════════════
 *
 * ── Étape 1 : inclure ui_backend.h AVANT le header du contrôle ──
 *
 *   // dans bim.c (ou le header qui inclut tout) :
 *   #include "ui_backend.h"
 *   #define CAIRO_LAYOUT_IMPLEMENTATION
 *   #include "cairo_layout.h"
 *
 * ── Étape 2 : simplifier xxx__register_classes() ────────────────
 *
 *   AVANT :
 *     WNDCLASSEX wc = {0};
 *     wc.cbSize        = sizeof wc;
 *     wc.style         = CS_HREDRAW | CS_VREDRAW;
 *     wc.hInstance     = hi;
 *     wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
 *     wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
 *     wc.lpfnWndProc   = cl__toolbar_proc;
 *     wc.lpszClassName = "CL_Toolbar";
 *     RegisterClassEx(&wc);
 *     wc.lpfnWndProc   = cl__menubar_proc;
 *     wc.lpszClassName = "CL_Menubar";
 *     RegisterClassEx(&wc);
 *     // ... répété N fois
 *
 *   APRÈS :
 *     ui_register_class("CL_Toolbar",  cl__toolbar_proc,  0, NULL);
 *     ui_register_class("CL_Menubar",  cl__menubar_proc,  0, NULL);
 *     ui_register_class("CL_Status",   cl__status_proc,   0, NULL);
 *     ui_register_class("CL_Splitter", cl__splitter_proc, 0,
 *                        LoadCursor(NULL, IDC_SIZENS));
 *
 * ── Étape 3 : simplifier CreateWindowEx + SetWindowLongPtr ──────
 *
 *   AVANT :
 *     l->hwnd_toolbar = CreateWindowEx(0, "CL_Toolbar", NULL,
 *         WS_CHILD | WS_VISIBLE, 0, CL_MENUBAR_H, w, CL_TOOLBAR_H,
 *         parent, NULL, hi, NULL);
 *     SetWindowLongPtr(l->hwnd_toolbar, GWLP_USERDATA, (LONG_PTR)l);
 *
 *   APRÈS :
 *     l->hwnd_toolbar = ui_subwnd_create("CL_Toolbar", parent,
 *         0, CL_MENUBAR_H, w, CL_TOOLBAR_H, l);
 *
 * ── Étape 4 : simplifier WM_PAINT dans chaque WndProc ───────────
 *
 *   AVANT :
 *     case WM_PAINT: {
 *         PAINTSTRUCT ps;
 *         HDC hdc = BeginPaint(hwnd, &ps);
 *         RECT rc; GetClientRect(hwnd, &rc);
 *         int w = rc.right, h = rc.bottom;
 *         HDC mem = CreateCompatibleDC(hdc);
 *         HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
 *         HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
 *         cl__draw_toolbar(l, mem, w, h);
 *         BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
 *         SelectObject(mem, old);
 *         DeleteObject(bmp); DeleteDC(mem);
 *         EndPaint(hwnd, &ps);
 *         return 0;
 *     }
 *
 *   APRÈS :
 *     case WM_PAINT: {
 *         UI_PAINT_BEGIN(hwnd, ctx);
 *             cl__draw_toolbar(l, ctx.cr, ctx.w, ctx.h);
 *         UI_PAINT_END(hwnd, ctx);
 *         return 0;
 *     }
 *
 *   ATTENTION : la signature de cl__draw_toolbar doit changer :
 *     AVANT : static void cl__draw_toolbar(CairoLayout *l, HDC hdc, int w, int h)
 *     APRÈS : static void cl__draw_toolbar(CairoLayout *l, cairo_t *cr, int w, int h)
 *   Et dans la fonction :
 *     SUPPRIMER : cairo_surface_t *surf = cairo_win32_surface_create(hdc);
 *     SUPPRIMER : cairo_t *cr = cairo_create(surf);
 *     SUPPRIMER : cairo_destroy(cr); cairo_surface_destroy(surf);
 *     (cr est maintenant le paramètre reçu)
 *
 * ── Étape 5 : utiliser ui_get_data au lieu de GetWindowLongPtr ──
 *
 *   AVANT :
 *     CairoLayout *l = (CairoLayout*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
 *     if (!l) return DefWindowProc(hwnd, msg, wp, lp);
 *
 *   APRÈS :
 *     CairoLayout *l = ui_get_data(CairoLayout, hwnd);
 *     if (!l) return DefWindowProc(hwnd, msg, wp, lp);
 *
 * ════════════════════════════════════════════════════════════════════
 * PORTABILITÉ FUTURE (Linux/X11, macOS/Quartz)
 * ════════════════════════════════════════════════════════════════════
 *
 * Pour porter sur X11 : remplacer la section #ifdef _WIN32 par une
 * section #elif defined(__linux__) qui fournit :
 *   - ui_register_class() → XWindowClass ou GTK widget type
 *   - ui_subwnd_create()  → XCreateWindow ou gtk_widget_new
 *   - UiDrawCtx           → cairo_xlib_surface_create
 *   - UI_PAINT_BEGIN/END  → handler expose + XFlush
 *
 * Les contrôles (cairo_layout.h, cairo_menu.h, etc.) n'ont pas à
 * changer — ils ne voient que cairo_t*.
 */
