/*
 * cairo_transcript.h  –  Transcript pane Win32/Cairo, self-contained
 * ──────────────────────────────────────────────────────────────────
 *
 *  STRUCTURE
 *  ─────────
 *   ┌─────────────────────────────────────────────────────────────┐
 *   │  [14:23:01]  SELECT * FROM nodes WHERE layer_id = 1        │ ← CMD
 *   │  [ok]        3 résultats trouvés                           │ ← OK
 *   │  [14:23:15]  LOAD shapefile 'urb_parcelles.shp'            │ ← CMD
 *   │  [err]       Fichier non trouvé : urb_parcelles.shp        │ ← ERR
 *   │  [info]      Chargement layer 'Default' en cours...        │ ← INFO
 *   │                                             ║              │
 *   │                                             ║ scrollbar    │
 *   ├─────────────────────────────────────────────────────────────┤
 *   │  > _                                                        │ ← INPUT
 *   └─────────────────────────────────────────────────────────────┘
 *
 *  UTILISATION MINIMALE
 *  ─────────────────────
 *    #define CAIRO_TRANSCRIPT_IMPLEMENTATION
 *    #include "cairo_transcript.h"
 *
 *  CRÉATION
 *  ────────
 *    CairoTranscript *ct = ct_create(hwnd_parent, x, y, w, h);
 *    ct_set_cb(ct, on_command, userdata);
 *
 *  AJOUT DE LIGNES
 *  ───────────────
 *    ct_add(ct, CT_CMD,  "SELECT * FROM nodes");   // bleu clair
 *    ct_add(ct, CT_OK,   "3 résultats");            // vert
 *    ct_add(ct, CT_ERR,  "Fichier non trouvé");     // rouge
 *    ct_add(ct, CT_INFO, "Chargement en cours..."); // gris
 *    ct_add(ct, CT_WARN, "Index manquant");         // orange
 *
 *  CALLBACK COMMANDE
 *  ──────────────────
 *    void on_command(const char *text, void *userdata) {
 *        // appelé quand l'utilisateur appuie sur Entrée
 *        ct_add(ct, CT_CMD, text);
 *        // traiter la commande...
 *    }
 *
 *  INTÉGRATION WndProc
 *  ────────────────────
 *    case WM_SIZE:    ct_resize(ct, x, y, new_w, new_h); break;
 *    case WM_DESTROY: ct_destroy(ct);                    break;
 *
 *  DIVERS
 *  ──────
 *    ct_clear(ct);                    // vider le log
 *    ct_scroll_to_bottom(ct);         // forcer scroll en bas
 *    ct_set_input(ct, "texte");       // pré-remplir l'input
 *
 * ──────────────────────────────────────────────────────────────────
 *  Dépendances : cairo, cairo-win32, gdi32
 *  Compilateur : GCC / MinGW-w64 (MSYS2)
 * ──────────────────────────────────────────────────────────────────
 */

#ifndef CAIRO_TRANSCRIPT_H
#define CAIRO_TRANSCRIPT_H

#include <windows.h>
#include <windowsx.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/* ── Types de lignes ─────────────────────────────────────────────── */
typedef enum {
    CT_CMD  = 0,   /* commande saisie      → bleu clair  */
    CT_OK   = 1,   /* résultat ok          → vert        */
    CT_ERR  = 2,   /* erreur               → rouge       */
    CT_INFO = 3,   /* information          → gris clair  */
    CT_WARN = 4,   /* avertissement        → orange      */
    CT_SEP  = 5,   /* séparateur           → ligne fine  */
} CtLineType;

/* ── Opaque handle ───────────────────────────────────────────────── */
typedef struct CairoTranscript_ CairoTranscript;

/* ── Callback commande ───────────────────────────────────────────── */
typedef void (*CtCommandCb)(const char *text, void *userdata);

/* ── API publique ────────────────────────────────────────────────── */
CairoTranscript *ct_create        (HWND parent, int x, int y, int w, int h);
void             ct_destroy       (CairoTranscript *ct);
void             ct_resize        (CairoTranscript *ct, int x, int y, int w, int h);
void             ct_set_cb        (CairoTranscript *ct, CtCommandCb cb, void *ud);
void             ct_add           (CairoTranscript *ct, CtLineType type, const char *text);
void             ct_clear         (CairoTranscript *ct);
void             ct_scroll_to_bottom(CairoTranscript *ct);
void             ct_set_input     (CairoTranscript *ct, const char *text);

/* ═══════════════════════════════════════════════════════════════════
   SECTION IMPLÉMENTATION
   ═══════════════════════════════════════════════════════════════════ */
#ifdef CAIRO_TRANSCRIPT_IMPLEMENTATION

/* ── Constantes ──────────────────────────────────────────────────── */
#define CT_MAX_LINES      2048
#define CT_MAX_LINE_LEN    512
#define CT_LINE_H           20    /* hauteur px par ligne             */
#define CT_INPUT_LINES       2    /* nb lignes de l'edit input        */
#define CT_INPUT_H          (CT_INPUT_LINES * 18 + 14)
#define CT_SCROLL_W         12    /* largeur scrollbar                */
#define CT_SCROLL_MIN_H     24    /* hauteur minimum du thumb         */
#define CT_TIMESTAMP_W      72    /* largeur colonne timestamp        */
#define CT_TAG_W            44    /* largeur colonne tag [ok] etc.    */
#define CT_PADDING           6

/* ── Ligne de log ────────────────────────────────────────────────── */
typedef struct {
    CtLineType type;
    char       text     [CT_MAX_LINE_LEN];
    char       timestamp[12];              /* "HH:MM:SS" */
} CtLine;

/* ── Structure principale ────────────────────────────────────────── */
struct CairoTranscript_ {
    HWND   hwnd_log;      /* fenêtre zone log (Cairo)   */
    HWND   hwnd_input;    /* EDIT Win32 multi-lignes     */
    HWND   hwnd_parent;

    int    x, y, w, h;   /* position dans le parent     */

    CtLine  lines[CT_MAX_LINES];
    int     line_count;

    int     scroll_offset;   /* ligne du haut visible       */
    int     scroll_dragging;
    int     scroll_drag_y0;
    int     scroll_drag_off0;

    int     hover_line;      /* ligne survolée (-1 = aucune) */

    CtCommandCb cb;
    void       *cb_ud;

    WNDPROC edit_orig_proc;  /* subclassing de l'EDIT        */
};

/* ── Forward ─────────────────────────────────────────────────────── */
static LRESULT CALLBACK ct__log_proc  (HWND, UINT, WPARAM, LPARAM);
static LRESULT CALLBACK ct__edit_sub  (HWND, UINT, WPARAM, LPARAM);

/* ── Helpers couleurs ────────────────────────────────────────────── */
static inline void ct__col_bg    (cairo_t *cr) { cairo_set_source_rgb(cr, 0.06, 0.07, 0.09); }
static inline void ct__col_bg2   (cairo_t *cr) { cairo_set_source_rgb(cr, 0.09, 0.10, 0.13); }
static inline void ct__col_border(cairo_t *cr) { cairo_set_source_rgba(cr, 0.22, 0.24, 0.32, 0.8); }
static inline void ct__col_scroll(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.30, 0.35, 0.50, a); }
static inline void ct__col_hover (cairo_t *cr) { cairo_set_source_rgba(cr, 1, 1, 1, 0.04); }

/* Couleurs par type de ligne */
static void ct__col_type(cairo_t *cr, CtLineType t, double a)
{
    switch (t) {
    case CT_CMD:  cairo_set_source_rgba(cr, 0.40, 0.75, 1.00, a); break;
    case CT_OK:   cairo_set_source_rgba(cr, 0.35, 0.85, 0.50, a); break;
    case CT_ERR:  cairo_set_source_rgba(cr, 1.00, 0.35, 0.35, a); break;
    case CT_INFO: cairo_set_source_rgba(cr, 0.60, 0.65, 0.75, a); break;
    case CT_WARN: cairo_set_source_rgba(cr, 1.00, 0.65, 0.20, a); break;
    case CT_SEP:  cairo_set_source_rgba(cr, 0.25, 0.28, 0.38, a); break;
    }
}

/* Tags affichés dans la colonne gauche */
static const char *ct__tag(CtLineType t)
{
    switch (t) {
    case CT_CMD:  return "cmd";
    case CT_OK:   return "ok";
    case CT_ERR:  return "err";
    case CT_INFO: return "inf";
    case CT_WARN: return "wrn";
    case CT_SEP:  return "   ";
    default:      return "   ";
    }
}

/* ── Calcul nombre de lignes visibles ────────────────────────────── */
static int ct__visible_lines(CairoTranscript *ct)
{
    RECT rc; GetClientRect(ct->hwnd_log, &rc);
    int log_h = rc.bottom;
    return log_h / CT_LINE_H;
}

/* ── Dessin log ──────────────────────────────────────────────────── */
static void ct__draw_log(CairoTranscript *ct, HDC hdc, int w, int h)
{
    cairo_surface_t *surf = cairo_win32_surface_create(hdc);
    cairo_t         *cr   = cairo_create(surf);

    /* Fond */
    ct__col_bg(cr);
    cairo_paint(cr);

    /* Bordure haute */
    ct__col_border(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, 0.5); cairo_line_to(cr, w, 0.5);
    cairo_stroke(cr);

    int visible = h / CT_LINE_H;
    int log_w   = w - CT_SCROLL_W - 2;

    cairo_select_font_face(cr, "Consolas",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);

    for (int i = 0; i < visible; i++) {
        int idx = ct->scroll_offset + i;
        if (idx >= ct->line_count) break;
        CtLine *ln = &ct->lines[idx];

        double ly = i * CT_LINE_H;

        /* Fond hover */
        if (idx == ct->hover_line) {
            ct__col_hover(cr);
            cairo_rectangle(cr, 0, ly, log_w, CT_LINE_H);
            cairo_fill(cr);
        }

        /* ── Séparateur ── */
        if (ln->type == CT_SEP) {
            double my = ly + CT_LINE_H / 2.0;
            if (ln->text[0]) {
                /* Avec titre — ligne / texte / ligne */
                cairo_text_extents_t te;
                cairo_text_extents(cr, ln->text, &te);
                double tw  = te.width + 12;
                double lx1 = 8;
                double lx2 = (log_w - tw) / 2.0;
                double lx3 = lx2 + tw;
                double lx4 = log_w - 8;

                cairo_set_source_rgba(cr, 0.25, 0.28, 0.38, 0.7);
                cairo_set_line_width(cr, 1.0);
                cairo_move_to(cr, lx1, my); cairo_line_to(cr, lx2, my);
                cairo_move_to(cr, lx3, my); cairo_line_to(cr, lx4, my);
                cairo_stroke(cr);

                cairo_set_source_rgba(cr, 0.45, 0.50, 0.65, 0.9);
                cairo_move_to(cr, lx2 + 6, ly + CT_LINE_H * 0.72);
                cairo_show_text(cr, ln->text);
            } else {
                /* Ligne seule */
                cairo_set_source_rgba(cr, 0.20, 0.22, 0.32, 0.8);
                cairo_set_line_width(cr, 1.0);
                cairo_move_to(cr, 8,         my);
                cairo_line_to(cr, log_w - 8, my);
                cairo_stroke(cr);
            }
            continue;   /* pas de barre gauche ni de colonnes */
        }

        /* Barre colorée gauche */
        ct__col_type(cr, ln->type, 0.7);
        cairo_rectangle(cr, 0, ly + 2, 3, CT_LINE_H - 4);
        cairo_fill(cr);

        /* Timestamp */
        ct__col_border(cr);
        cairo_move_to(cr, 8, ly + CT_LINE_H * 0.72);
        cairo_show_text(cr, ln->timestamp);

        /* Séparateur */
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, CT_TIMESTAMP_W + 4, ly + 4);
        cairo_line_to(cr, CT_TIMESTAMP_W + 4, ly + CT_LINE_H - 4);
        cairo_stroke(cr);

        /* Tag [ok] [err] etc. */
        ct__col_type(cr, ln->type, 0.85);
        cairo_move_to(cr, CT_TIMESTAMP_W + 8, ly + CT_LINE_H * 0.72);
        cairo_show_text(cr, ct__tag(ln->type));

        /* Séparateur */
        ct__col_border(cr);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, CT_TIMESTAMP_W + CT_TAG_W + 4, ly + 4);
        cairo_line_to(cr, CT_TIMESTAMP_W + CT_TAG_W + 4, ly + CT_LINE_H - 4);
        cairo_stroke(cr);

        /* Texte message — clippé */
        cairo_save(cr);
        int tx = CT_TIMESTAMP_W + CT_TAG_W + 8;
        cairo_rectangle(cr, tx, ly, log_w - tx - CT_PADDING, CT_LINE_H);
        cairo_clip(cr);
        if (ln->type == CT_CMD)
            ct__col_type(cr, CT_CMD, 1.0);
        else
            cairo_set_source_rgba(cr, 0.88, 0.90, 0.92, 0.92);
        cairo_move_to(cr, tx, ly + CT_LINE_H * 0.72);
        cairo_show_text(cr, ln->text);
        cairo_restore(cr);

        /* Ligne séparatrice légère */
        if (i < visible - 1) {
            ct__col_border(cr);
            cairo_set_line_width(cr, 0.5);
            cairo_move_to(cr, 4, ly + CT_LINE_H - 0.5);
            cairo_line_to(cr, log_w - 4, ly + CT_LINE_H - 0.5);
            cairo_stroke(cr);
        }
    }

    /* ── Scrollbar ───────────────────────────────────────────────── */
    int sb_x = log_w + 2;
    int sb_h = h;

    /* Fond scrollbar */
    ct__col_bg2(cr);
    cairo_rectangle(cr, sb_x, 0, CT_SCROLL_W, sb_h);
    cairo_fill(cr);

    /* Séparateur gauche scrollbar */
    ct__col_border(cr);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, sb_x + 0.5, 0);
    cairo_line_to(cr, sb_x + 0.5, sb_h);
    cairo_stroke(cr);

    /* Thumb */
    if (ct->line_count > visible) {
        double ratio  = (double)visible / ct->line_count;
        int    thumb_h = (int)(sb_h * ratio);
        if (thumb_h < CT_SCROLL_MIN_H) thumb_h = CT_SCROLL_MIN_H;
        double thumb_y = (sb_h - thumb_h)
            * ((double)ct->scroll_offset / (ct->line_count - visible));

        double tx2 = sb_x + 2;
        double tw2 = CT_SCROLL_W - 4;
        double tr  = tw2 / 2.0;

        /* Fond thumb */
        ct__col_scroll(cr, ct->scroll_dragging ? 0.75 : 0.45);
        cairo_new_sub_path(cr);
        cairo_arc(cr, tx2+tr,             thumb_y+tr,            tr, M_PI,   3*M_PI/2);
        cairo_arc(cr, tx2+tw2-tr,         thumb_y+tr,            tr, 3*M_PI/2, 0);
        cairo_arc(cr, tx2+tw2-tr,         thumb_y+thumb_h-tr,    tr, 0,      M_PI/2);
        cairo_arc(cr, tx2+tr,             thumb_y+thumb_h-tr,    tr, M_PI/2, M_PI);
        cairo_close_path(cr);
        cairo_fill(cr);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

/* ── Scroll helpers ──────────────────────────────────────────────── */
static void ct__clamp_scroll(CairoTranscript *ct)
{
    int visible = ct__visible_lines(ct);
    int max_off = ct->line_count - visible;
    if (max_off < 0) max_off = 0;
    if (ct->scroll_offset < 0)       ct->scroll_offset = 0;
    if (ct->scroll_offset > max_off) ct->scroll_offset = max_off;
}

static int ct__scroll_thumb_y(CairoTranscript *ct, int log_h)
{
    int visible = ct__visible_lines(ct);
    if (ct->line_count <= visible) return 0;
    double ratio   = (double)visible / ct->line_count;
    int    thumb_h = (int)(log_h * ratio);
    if (thumb_h < CT_SCROLL_MIN_H) thumb_h = CT_SCROLL_MIN_H;
    return (int)((log_h - thumb_h)
        * ((double)ct->scroll_offset / (ct->line_count - visible)));
}

static int ct__scroll_thumb_h(CairoTranscript *ct, int log_h)
{
    int visible = ct__visible_lines(ct);
    if (ct->line_count <= visible) return log_h;
    double ratio   = (double)visible / ct->line_count;
    int    thumb_h = (int)(log_h * ratio);
    return thumb_h < CT_SCROLL_MIN_H ? CT_SCROLL_MIN_H : thumb_h;
}

static int ct__is_in_scroll(CairoTranscript *ct, int mx, int log_w)
{
    return mx >= log_w + 2;
}

/* ── WndProc log ─────────────────────────────────────────────────── */
static LRESULT CALLBACK ct__log_proc(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    CairoTranscript *ct = (CairoTranscript*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!ct) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc; GetClientRect(hwnd, &rc);
        HDC mem = CreateCompatibleDC(hdc);
        HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP old = (HBITMAP)SelectObject(mem, bmp);
        ct__draw_log(ct, mem, rc.right, rc.bottom);
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp); DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        ct->scroll_offset -= (delta / WHEEL_DELTA) * 3;
        ct__clamp_scroll(ct);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;
    }

    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        int log_w = rc.right - CT_SCROLL_W - 2;

        if (ct->scroll_dragging) {
            /* Déplacer le thumb */
            int log_h  = rc.bottom;
            int thumb_h = ct__scroll_thumb_h(ct, log_h);
            int travel  = log_h - thumb_h;
            if (travel > 0) {
                int dy  = my - ct->scroll_drag_y0;
                int visible = ct__visible_lines(ct);
                int max_off = ct->line_count - visible;
                ct->scroll_offset = ct->scroll_drag_off0
                    + (int)(dy * max_off / (double)travel);
                ct__clamp_scroll(ct);
                InvalidateRect(hwnd, NULL, FALSE);
            }
            return 0;
        }

        /* Hover ligne */
        int prev_hover = ct->hover_line;
        if (!ct__is_in_scroll(ct, mx, log_w)) {
            int line_idx = ct->scroll_offset + my / CT_LINE_H;
            ct->hover_line = (line_idx < ct->line_count) ? line_idx : -1;
        } else {
            ct->hover_line = -1;
        }
        if (ct->hover_line != prev_hover)
            InvalidateRect(hwnd, NULL, FALSE);

        TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        ct->hover_line = -1;
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        int log_w = rc.right - CT_SCROLL_W - 2;
        int log_h = rc.bottom;

        if (ct__is_in_scroll(ct, mx, log_w)) {
            /* Clic dans la scrollbar */
            int thumb_y = ct__scroll_thumb_y(ct, log_h);
            int thumb_h = ct__scroll_thumb_h(ct, log_h);
            if (my >= thumb_y && my < thumb_y + thumb_h) {
                ct->scroll_dragging  = 1;
                ct->scroll_drag_y0   = my;
                ct->scroll_drag_off0 = ct->scroll_offset;
                SetCapture(hwnd);
            } else {
                /* Clic hors thumb — sauter */
                int visible = ct__visible_lines(ct);
                if (my < thumb_y)
                    ct->scroll_offset -= visible;
                else
                    ct->scroll_offset += visible;
                ct__clamp_scroll(ct);
                InvalidateRect(hwnd, NULL, FALSE);
            }
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK: {
        /* Double-clic → copie dans l'input */
        int my = GET_Y_LPARAM(lp);
        RECT rc; GetClientRect(hwnd, &rc);
        int log_w = rc.right - CT_SCROLL_W - 2;
        int mx    = GET_X_LPARAM(lp);
        if (!ct__is_in_scroll(ct, mx, log_w)) {
            int line_idx = ct->scroll_offset + my / CT_LINE_H;
            if (line_idx >= 0 && line_idx < ct->line_count) {
                ct_set_input(ct, ct->lines[line_idx].text);
                SetFocus(ct->hwnd_input);
                /* Placer curseur en fin */
                int len = (int)strlen(ct->lines[line_idx].text);
                SendMessage(ct->hwnd_input, EM_SETSEL, len, len);
            }
        }
        return 0;
    }

    case WM_LBUTTONUP:
        if (ct->scroll_dragging) {
            ct->scroll_dragging = 0;
            ReleaseCapture();
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ── Subclass EDIT — intercepter Entrée et Echap ─────────────────── */
static LRESULT CALLBACK ct__edit_sub(HWND hwnd, UINT msg,
                                      WPARAM wp, LPARAM lp)
{
    CairoTranscript *ct = (CairoTranscript*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) {
            /* Entrée → callback */
            char buf[CT_MAX_LINE_LEN] = "";
            GetWindowText(hwnd, buf, CT_MAX_LINE_LEN - 1);
            /* Trim trailing newlines */
            int len = (int)strlen(buf);
            while (len > 0 && (buf[len-1] == '\r' || buf[len-1] == '\n'))
                buf[--len] = '\0';
            if (len > 0 && ct && ct->cb) {
                ct->cb(buf, ct->cb_ud);
                SetWindowText(hwnd, "");
            }
            return 0;
        }
        if (wp == VK_ESCAPE) {
            SetWindowText(hwnd, "");
            return 0;
        }
        if (wp == VK_UP) {
            /* Flèche haut → scroll log */
            ct->scroll_offset--;
            ct__clamp_scroll(ct);
            InvalidateRect(ct->hwnd_log, NULL, FALSE);
            return 0;
        }
        if (wp == VK_DOWN) {
            ct->scroll_offset++;
            ct__clamp_scroll(ct);
            InvalidateRect(ct->hwnd_log, NULL, FALSE);
            return 0;
        }
    }

    return CallWindowProc(ct->edit_orig_proc, hwnd, msg, wp, lp);
}

/* ── Enregistrement classe ───────────────────────────────────────── */
static void ct__register(HINSTANCE hi)
{
    static int done = 0;
    if (done) return; done = 1;

    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof wc;
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = ct__log_proc;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = "CT_Log";
    RegisterClassEx(&wc);
}

/* ── Couleur fond + texte pour l'EDIT via WM_CTLCOLOREDIT ────────── */
/* À ajouter dans le WndProc parent :
 *   case WM_CTLCOLOREDIT: return ct_on_ctlcolor(ct, (HDC)wp, (HWND)lp);
 */
static HBRUSH ct__edit_bg_brush = NULL;

LRESULT ct_on_ctlcolor(CairoTranscript *ct, HDC hdc, HWND hwnd_edit)
{
    if (hwnd_edit != ct->hwnd_input) return (LRESULT)NULL;
    SetTextColor(hdc, RGB(200, 210, 220));
    SetBkColor  (hdc, RGB(10,  13,  18));
    if (!ct__edit_bg_brush)
        ct__edit_bg_brush = CreateSolidBrush(RGB(10, 13, 18));
    return (LRESULT)ct__edit_bg_brush;
}

/* ══════════════════════════════════════════════════════════════════
   API PUBLIQUE — IMPLÉMENTATION
   ══════════════════════════════════════════════════════════════════ */

CairoTranscript *ct_create(HWND parent, int x, int y, int w, int h)
{
    CairoTranscript *ct = (CairoTranscript*)calloc(1, sizeof(CairoTranscript));
    ct->hwnd_parent  = parent;
    ct->x = x; ct->y = y; ct->w = w; ct->h = h;
    ct->hover_line   = -1;

    HINSTANCE hi = (HINSTANCE)GetWindowLongPtr(parent, GWLP_HINSTANCE);
    ct__register(hi);

    int log_h   = h - CT_INPUT_H - 1;
    int input_y = y + log_h + 1;

    /* Fenêtre log Cairo */
    ct->hwnd_log = CreateWindowEx(0, "CT_Log", NULL,
        WS_CHILD | WS_VISIBLE,
        x, y, w, log_h,
        parent, NULL, hi, NULL);
    SetWindowLongPtr(ct->hwnd_log, GWLP_USERDATA, (LONG_PTR)ct);

    /* EDIT multi-lignes */
    ct->hwnd_input = CreateWindowEx(
        WS_EX_CLIENTEDGE,
        "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
        x, input_y, w, CT_INPUT_H,
        parent, NULL, hi, NULL);

    /* Police monospace pour l'input */
    HFONT hf = CreateFont(
        14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN,
        "Consolas");
    SendMessage(ct->hwnd_input, WM_SETFONT, (WPARAM)hf, TRUE);

    /* Subclasser l'EDIT */
    SetWindowLongPtr(ct->hwnd_input, GWLP_USERDATA, (LONG_PTR)ct);
    ct->edit_orig_proc = (WNDPROC)SetWindowLongPtr(
        ct->hwnd_input, GWLP_WNDPROC, (LONG_PTR)ct__edit_sub);

    return ct;
}

void ct_destroy(CairoTranscript *ct)
{
    if (!ct) return;
    if (ct->hwnd_log)   DestroyWindow(ct->hwnd_log);
    if (ct->hwnd_input) DestroyWindow(ct->hwnd_input);
    free(ct);
}

void ct_resize(CairoTranscript *ct, int x, int y, int w, int h)
{
    if (!ct) return;
    ct->x = x; ct->y = y; ct->w = w; ct->h = h;

    int log_h   = h - CT_INPUT_H - 1;
    int input_y = y + log_h + 1;

    SetWindowPos(ct->hwnd_log,   NULL, x, y,        w, log_h,    SWP_NOZORDER);
    SetWindowPos(ct->hwnd_input, NULL, x, input_y,  w, CT_INPUT_H, SWP_NOZORDER);

    ct__clamp_scroll(ct);
    InvalidateRect(ct->hwnd_log, NULL, FALSE);
}

void ct_set_cb(CairoTranscript *ct, CtCommandCb cb, void *ud)
{
    if (!ct) return;
    ct->cb    = cb;
    ct->cb_ud = ud;
}

void ct_add(CairoTranscript *ct, CtLineType type, const char *text)
{
    if (!ct || !text) return;

    /* Si plein — supprimer la moitié des anciennes lignes */
    if (ct->line_count >= CT_MAX_LINES) {
        int keep = CT_MAX_LINES / 2;
        memmove(ct->lines, ct->lines + (CT_MAX_LINES - keep),
                keep * sizeof(CtLine));
        ct->line_count = keep;
    }

    CtLine *ln = &ct->lines[ct->line_count++];
    ln->type = type;
    strncpy(ln->text, text, CT_MAX_LINE_LEN - 1);
    ln->text[CT_MAX_LINE_LEN - 1] = '\0';

    /* Timestamp */
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    snprintf(ln->timestamp, sizeof ln->timestamp,
             "%02d:%02d:%02d", tm->tm_hour, tm->tm_min, tm->tm_sec);

    /* Auto-scroll vers le bas si on était déjà en bas */
    int visible = ct__visible_lines(ct);
    int max_off = ct->line_count - 1 - visible;
    if (max_off < 0) max_off = 0;
    if (ct->scroll_offset >= max_off - 1)
        ct->scroll_offset = max_off;

    ct__clamp_scroll(ct);
    InvalidateRect(ct->hwnd_log, NULL, FALSE);
}

void ct_clear(CairoTranscript *ct)
{
    if (!ct) return;
    ct->line_count    = 0;
    ct->scroll_offset = 0;
    ct->hover_line    = -1;
    InvalidateRect(ct->hwnd_log, NULL, FALSE);
}

void ct_scroll_to_bottom(CairoTranscript *ct)
{
    if (!ct) return;
    int visible = ct__visible_lines(ct);
    ct->scroll_offset = ct->line_count - visible;
    ct__clamp_scroll(ct);
    InvalidateRect(ct->hwnd_log, NULL, FALSE);
}

void ct_set_input(CairoTranscript *ct, const char *text)
{
    if (!ct || !text) return;
    SetWindowText(ct->hwnd_input, text);
}

#endif /* CAIRO_TRANSCRIPT_IMPLEMENTATION */
#endif /* CAIRO_TRANSCRIPT_H */
