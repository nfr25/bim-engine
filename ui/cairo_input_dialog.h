/*
 * cairo_input_dialog.h  –  Dialogue de saisie texte dark Cairo/Win32
 * ──────────────────────────────────────────────────────────────────
 *
 *  UTILISATION
 *  ───────────
 *    #define CAIRO_INPUT_DIALOG_IMPLEMENTATION
 *    #include "cairo_input_dialog.h"
 *
 *    char name[64] = "";
 *    if (cl_input_dialog(hwnd, "Nouveau layer", "Nom :", name, sizeof name)) {
 *        // name contient le texte saisi
 *    }
 *
 *  CONTRÔLES CLAVIER
 *  ──────────────────
 *    Entrée   → valider
 *    Echap    → annuler
 *
 *  DÉPENDANCES
 *  ────────────
 *    cairo, cairo-win32, gdi32, dwmapi
 *    Compilateur : GCC / MinGW-w64 (MSYS2)
 * ──────────────────────────────────────────────────────────────────
 */

#ifndef CAIRO_INPUT_DIALOG_H
#define CAIRO_INPUT_DIALOG_H

#include <windows.h>
#include <windowsx.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <dwmapi.h>
#include <string.h>
#include <stdlib.h>

/* ─── API publique ──────────────────────────────────────────────── */

/* Ouvre un dialogue de saisie modal dark.
 * Retourne 1 si validé (Entrée), 0 si annulé (Echap ou croix).
 * name_out est rempli uniquement si retour == 1. */
int cl_input_dialog(HWND parent, const char *title,
                    const char *prompt,
                    char *name_out, int max_len);

/* ═══════════════════════════════════════════════════════════════════
   SECTION IMPLÉMENTATION
   ═══════════════════════════════════════════════════════════════════ */
#ifdef CAIRO_INPUT_DIALOG_IMPLEMENTATION

/* ── Couleurs cohérentes avec cairo_layout.h ─────────────────────── */
static inline void cid__col_bg    (cairo_t *cr) { cairo_set_source_rgb (cr, 0.13, 0.14, 0.18); }
static inline void cid__col_bg2   (cairo_t *cr) { cairo_set_source_rgb (cr, 0.10, 0.11, 0.15); }
static inline void cid__col_border(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.22, 0.24, 0.32, a); }
static inline void cid__col_text  (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.90, 0.92, 0.95, a); }
static inline void cid__col_textd (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.42, 0.44, 0.50, a); }
static inline void cid__col_accent(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.30, 0.70, 1.00, a); }
static inline void cid__col_btn   (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.25, 0.45, 0.90, a); }

static void cid__rounded_rect(cairo_t *cr, double x, double y,
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

/* ── État interne ────────────────────────────────────────────────── */
typedef struct {
    const char *title;
    const char *prompt;
    char       *name_out;
    int         max_len;
    int         result;       /* 1=OK, 0=annulé */
    HWND        hwnd_edit;
    WNDPROC     edit_orig_proc;
    int         btn_ok_hovered;
    int         btn_cancel_hovered;
} CidState;

/* ── Rendu fond Cairo ────────────────────────────────────────────── */
static void cid__draw(CidState *st, HWND hwnd, HDC hdc, int w, int h)
{
    cairo_surface_t *surf = cairo_win32_surface_create(hdc);
    cairo_t         *cr   = cairo_create(surf);

    /* Fond principal */
    cid__col_bg(cr);
    cairo_paint(cr);

    /* Bordure extérieure */
    cid__rounded_rect(cr, 0.5, 0.5, w-1, h-1, 6);
    cid__col_border(cr, 0.8);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    /* Prompt "Nom :" — au-dessus du cadre */
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);
    cid__col_textd(cr, 1.0);
    cairo_move_to(cr, 16, 16);
    cairo_show_text(cr, st->prompt);

    /* Cadre champ edit */
    int ex = 16, ey = 24, ew = w - 32, eh = 26;
    cid__rounded_rect(cr, ex, ey, ew, eh, 4);
    cid__col_bg2(cr);
    cairo_fill_preserve(cr);
    cid__col_accent(cr, 0.70);
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    /* Séparateur avant boutons */
    cid__col_border(cr, 0.6);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, h - 46);
    cairo_line_to(cr, w, h - 46);
    cairo_stroke(cr);

    /* Bouton Annuler */
    int bh = 28, bw = 90, by = h - 38, margin = 12;
    int cancel_x = w - bw*2 - margin*2;
    int ok_x     = w - bw   - margin;

    cid__rounded_rect(cr, cancel_x, by, bw, bh, 4);
    if (st->btn_cancel_hovered) {
        cid__col_border(cr, 0.6);
        cairo_fill_preserve(cr);
        cid__col_border(cr, 0.9);
    } else {
        cid__col_bg2(cr);
        cairo_fill_preserve(cr);
        cid__col_border(cr, 0.6);
    }
    cairo_set_line_width(cr, 1.0);
    cairo_stroke(cr);

    cairo_set_font_size(cr, 11.5);
    cairo_text_extents_t te;
    cairo_text_extents(cr, "Annuler", &te);
    cid__col_textd(cr, 1.0);
    cairo_move_to(cr, cancel_x + (bw - te.width)/2.0 - te.x_bearing,
                      by + (bh + te.height)/2.0 - te.y_bearing - te.height);
    cairo_show_text(cr, "Annuler");

    /* Bouton OK */
    cid__rounded_rect(cr, ok_x, by, bw, bh, 4);
    if (st->btn_ok_hovered)
        cid__col_btn(cr, 1.0);
    else
        cid__col_btn(cr, 0.80);
    cairo_fill(cr);

    cairo_text_extents(cr, "OK", &te);
    cid__col_text(cr, 1.0);
    cairo_move_to(cr, ok_x + (bw - te.width)/2.0 - te.x_bearing,
                      by + (bh + te.height)/2.0 - te.y_bearing - te.height);
    cairo_show_text(cr, "OK");

    /* Hint clavier */
    /*
    cairo_set_font_size(cr, 9.5);
    cid__col_textd(cr, 0.5);
    cairo_move_to(cr, 16, h - 52);
    cairo_show_text(cr, "Entree = valider   Echap = annuler");
    */
    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

/* ── Subclass de l'edit — capture Entrée et Echap ────────────────── */
static LRESULT CALLBACK cid__edit_proc(HWND hwnd, UINT msg,
                                        WPARAM wp, LPARAM lp)
{
    CidState *st = (CidState*)GetWindowLongPtr(
                        GetParent(hwnd), GWLP_USERDATA);
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) {
            SendMessage(GetParent(hwnd), WM_COMMAND, IDOK,     0);
            return 0;
        }
        if (wp == VK_ESCAPE) {
            SendMessage(GetParent(hwnd), WM_COMMAND, IDCANCEL, 0);
            return 0;
        }
    }
    /* Redessiner le cadre Cairo au changement de texte */
    if (msg == WM_CHAR)
        InvalidateRect(GetParent(hwnd), NULL, FALSE);

    return st ? CallWindowProc(st->edit_orig_proc, hwnd, msg, wp, lp)
              : DefWindowProc(hwnd, msg, wp, lp);
}

/* ── WndProc dialogue ────────────────────────────────────────────── */
static LRESULT CALLBACK cid__dlg_proc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp)
{
    CidState *st = (CidState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {

    case WM_CREATE: {
        CREATESTRUCT *cs = (CREATESTRUCT*)lp;
        st = (CidState*)cs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)st);

        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right;

        /* Champ edit natif Windows — fond noir, texte blanc */
        st->hwnd_edit = CreateWindowEx(
            0, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            18, 26, w - 36, 22,
            hwnd, (HMENU)10, cs->hInstance, NULL);

        /* Couleurs edit via WM_CTLCOLOREDIT */
        HFONT hf = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
        SendMessage(st->hwnd_edit, WM_SETFONT, (WPARAM)hf, TRUE);

        /* Subclasser l'edit pour capturer Entrée/Echap */
        st->edit_orig_proc = (WNDPROC)SetWindowLongPtr(
            st->hwnd_edit, GWLP_WNDPROC, (LONG_PTR)cid__edit_proc);

        SetFocus(st->hwnd_edit);
        return 0;
    }

    case WM_CTLCOLOREDIT: {
        /* Fond et texte de l'edit natif en dark */
        HDC hdc_edit = (HDC)wp;
        SetBkColor  (hdc_edit, RGB(26, 28, 38));   /* bg2 */
        SetTextColor(hdc_edit, RGB(230, 235, 242)); /* text */
        static HBRUSH hbr = NULL;
        if (!hbr) hbr = CreateSolidBrush(RGB(26, 28, 38));
        return (LRESULT)hbr;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, w, h);
        HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
        cid__draw(st, hwnd, mem, w, h);
        BitBlt(hdc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        int bh = 28, bw = 90, by = h - 38, margin = 12;
        int cancel_x = w - bw*2 - margin*2;
        int ok_x     = w - bw   - margin;
        int prev_ok     = st->btn_ok_hovered;
        int prev_cancel = st->btn_cancel_hovered;
        st->btn_ok_hovered     = (mx >= ok_x     && mx < ok_x+bw     && my >= by && my < by+bh);
        st->btn_cancel_hovered = (mx >= cancel_x && mx < cancel_x+bw && my >= by && my < by+bh);
        if (st->btn_ok_hovered != prev_ok || st->btn_cancel_hovered != prev_cancel)
            InvalidateRect(hwnd, NULL, FALSE);
        TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        st->btn_ok_hovered = st->btn_cancel_hovered = 0;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        int w = rc.right, h = rc.bottom;
        int bh = 28, bw = 90, by = h - 38, margin = 12;
        int cancel_x = w - bw*2 - margin*2;
        int ok_x     = w - bw   - margin;
        if (mx >= ok_x && mx < ok_x+bw && my >= by && my < by+bh)
            SendMessage(hwnd, WM_COMMAND, IDOK, 0);
        if (mx >= cancel_x && mx < cancel_x+bw && my >= by && my < by+bh)
            SendMessage(hwnd, WM_COMMAND, IDCANCEL, 0);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wp) == IDOK) {
            GetWindowText(st->hwnd_edit, st->name_out, st->max_len);
            if (strlen(st->name_out) > 0) {
                st->result = 1;
                DestroyWindow(hwnd);
            } else {
                /* Champ vide — on signale visuellement */
                InvalidateRect(hwnd, NULL, FALSE);
                SetFocus(st->hwnd_edit);
            }
        }
        if (LOWORD(wp) == IDCANCEL) {
            st->result = 0;
            DestroyWindow(hwnd);
        }
        return 0;

    case WM_DESTROY:
        /* PAS de PostQuitMessage — fenêtre modale */
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ── Fonction publique ───────────────────────────────────────────── */
int cl_input_dialog(HWND parent, const char *title,
                    const char *prompt,
                    char *name_out, int max_len)
{
    CidState st = {0};
    st.title   = title;
    st.prompt  = prompt;
    st.name_out = name_out;
    st.max_len  = max_len;
    st.result   = 0;

    HINSTANCE hi = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);

    /* Enregistrer la classe une seule fois */
    static int reg = 0;
    if (!reg) {
        reg = 1;
        WNDCLASSEX wc = {0};
        wc.cbSize        = sizeof wc;
        wc.style         = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc   = cid__dlg_proc;
        wc.hInstance     = hi;
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
        wc.lpszClassName = "CID_Dialog";
        wc.cbWndExtra    = sizeof(LONG_PTR);
        RegisterClassEx(&wc);
    }

    /* Centrer sur le parent */
    RECT pr; GetWindowRect(parent, &pr);
    int dw = 340, dh = 148;
    int dx = pr.left + (pr.right  - pr.left - dw) / 2;
    int dy = pr.top  + (pr.bottom - pr.top  - dh) / 2;

    HWND hwnd = CreateWindowEx(
        WS_EX_DLGMODALFRAME,
        "CID_Dialog", title,
        WS_POPUP | WS_CAPTION,
        dx, dy, dw, dh,
        parent, NULL, hi, &st);

    /* Dark mode titlebar */
    ShowWindow(hwnd, SW_SHOW);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20, &dark, sizeof dark);
    UpdateWindow(hwnd);

    /* Boucle modale */
    MSG m;
    while (IsWindow(hwnd) && GetMessage(&m, NULL, 0, 0)) {
        if (!IsDialogMessage(hwnd, &m)) {
            TranslateMessage(&m);
            DispatchMessage(&m);
        }
    }

    return st.result;
}

#endif /* CAIRO_INPUT_DIALOG_IMPLEMENTATION */
#endif /* CAIRO_INPUT_DIALOG_H */
