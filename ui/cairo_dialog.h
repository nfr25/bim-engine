/*
 * cairo_dialog.h  —  Boite dialogue universelle Cairo/Win32
 * ──────────────────────────────────────────────────────────────────
 *
 *  UTILISATION
 *  ───────────
 *    #define CAIRO_DIALOG_IMPLEMENTATION
 *    #include "cairo_dialog.h"
 *
 *  DIALOGUE ÉDITION
 *  ────────────────
 *    CldField fields[] = {
 *        { "Nom",        "Default",  CLD_TEXT,       "" },
 *        { "Largeur",    "10",       CLD_INT,        "" },
 *        { "Opacité",    "0.75",     CLD_FLOAT,      "" },
 *        { "Couleur",    "Rouge",    CLD_MULTIVALUE, "Rouge|Vert|Bleu" },
 *        { "Visible",    "1",        CLD_BOOL,       "" },
 *    };
 *    if (cld_show(hwnd, "Propriétés", fields, 5, CLD_OK_CANCEL)) {
 *        // fields[0].value contient la nouvelle valeur
 *        // fields[1].value → atoi()
 *        // fields[2].value → atof()
 *        // fields[3].value → texte du choix sélectionné
 *        // fields[4].value → "1" ou "0"
 *    }
 *
 *  MSGBOX
 *  ──────
 *    if (cld_msgbox(hwnd, "Confirmer", "Supprimer cet objet ?", CLD_OK_CANCEL))
 *        // utilisateur a cliqué OK
 *
 * ──────────────────────────────────────────────────────────────────
 *  Dépendances : cairo, cairo-win32, gdi32, user32
 *  Compilateur : GCC / MinGW-w64 (MSYS2)
 * ──────────────────────────────────────────────────────────────────
 */

#ifndef CAIRO_DIALOG_H
#define CAIRO_DIALOG_H

#include <windows.h>
#include <windowsx.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* ── Types de champs ─────────────────────────────────────────────── */
#define CLD_TEXT        0   /* texte libre                           */
#define CLD_INT         1   /* entier — validation numérique         */
#define CLD_FLOAT       2   /* flottant — validation numérique       */
#define CLD_MULTIVALUE  3   /* combo dropdown "A|B|C"                */
#define CLD_BOOL        4   /* checkbox                              */

/* ── Flags dialogue ──────────────────────────────────────────────── */
#define CLD_OK_CANCEL   0   /* boutons OK + Annuler                  */
#define CLD_OK_ONLY     1   /* bouton OK uniquement                  */

/* ── Structure champ ─────────────────────────────────────────────── */
typedef struct {
    char label  [64];    /* libellé affiché à gauche                 */
    char value  [256];   /* valeur IN (initiale) et OUT (résultat)   */
    int  type;           /* CLD_TEXT / CLD_INT / CLD_FLOAT / ...     */
    char choices[512];   /* MULTIVALUE : "Rouge|Vert|Bleu"           */
} CldField;

/* ── API publique ────────────────────────────────────────────────── */

/* Dialogue édition — retourne 1=OK, 0=Annuler                       */
int cld_show  (HWND parent, const char *title,
               CldField *fields, int count, int flags);

/* Msgbox simple — retourne 1=OK, 0=Annuler                          */
int cld_msgbox(HWND parent, const char *title,
               const char *message, int flags);

/* ═══════════════════════════════════════════════════════════════════
   SECTION IMPLÉMENTATION
   ═══════════════════════════════════════════════════════════════════ */
#ifdef CAIRO_DIALOG_IMPLEMENTATION

/* ── Constantes visuelles ────────────────────────────────────────── */
#define CLD_W            380    /* largeur fixe de la boite           */
#define CLD_PADDING       16    /* marge intérieure                   */
#define CLD_LABEL_W      110    /* largeur colonne label              */
#define CLD_FIELD_H       28    /* hauteur d'un champ                 */
#define CLD_FIELD_GAP      8    /* espace entre champs                */
#define CLD_TITLE_H       36    /* hauteur zone titre                 */
#define CLD_MSG_H         48    /* hauteur zone message (msgbox)      */
#define CLD_BTN_H         36    /* hauteur zone boutons               */
#define CLD_BTN_W         90    /* largeur d'un bouton                */
#define CLD_CORNER_R       8    /* rayon coins fenêtre                */
#define CLD_MAX_FIELDS    32    /* champs max                         */
#define CLD_MAX_CHOICES   32    /* choix max par MULTIVALUE           */

/* ── Couleurs ────────────────────────────────────────────────────── */
static inline void cld__col_bg    (cairo_t *cr) { cairo_set_source_rgb (cr, 0.10, 0.11, 0.15); }
static inline void cld__col_bg2   (cairo_t *cr) { cairo_set_source_rgb (cr, 0.13, 0.14, 0.19); }
static inline void cld__col_border(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.28, 0.30, 0.42, a); }
static inline void cld__col_text  (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.92, 0.93, 0.95, a); }
static inline void cld__col_label (cairo_t *cr) { cairo_set_source_rgba(cr, 0.65, 0.68, 0.78, 1.0); }
static inline void cld__col_accent(cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.30, 0.60, 1.00, a); }
static inline void cld__col_error (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 1.00, 0.35, 0.35, a); }
static inline void cld__col_hover (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 1.00, 1.00, 1.00, a); }
static inline void cld__col_press (cairo_t *cr, double a) { cairo_set_source_rgba(cr, 0.30, 0.60, 1.00, a); }

static void cld__rounded_rect(cairo_t *cr, double x, double y,
                               double w, double h, double r)
{
    cairo_new_sub_path(cr);
    cairo_arc(cr, x+w-r, y+r,   r, -M_PI/2, 0);
    cairo_arc(cr, x+w-r, y+h-r, r,  0,       M_PI/2);
    cairo_arc(cr, x+r,   y+h-r, r,  M_PI/2,  M_PI);
    cairo_arc(cr, x+r,   y+r,   r,  M_PI,    3*M_PI/2);
    cairo_close_path(cr);
}

/* ── État interne ────────────────────────────────────────────────── */
typedef struct {
    char parts[CLD_MAX_CHOICES][64];
    int  count;
} CldChoices;

typedef struct {
    /* Référence vers les champs utilisateur */
    CldField   *fields;
    int         count;
    int         flags;
    const char *title;
    const char *message;   /* msgbox uniquement */

    /* Résultat */
    int         result;    /* 1=OK, 0=Annuler */

    /* HWND des contrôles Win32 */
    HWND        hwnd_edits  [CLD_MAX_FIELDS];   /* EDIT ou NULL       */
    HWND        hwnd_combos [CLD_MAX_FIELDS];   /* COMBO ou NULL      */
    HWND        hwnd_checks [CLD_MAX_FIELDS];   /* checkbox ou NULL   */
    HWND        hwnd_ok;
    HWND        hwnd_cancel;

    /* Validation */
    int         field_valid [CLD_MAX_FIELDS];   /* 1=ok, 0=erreur     */

    /* Hover boutons */
    int         ok_hovered;
    int         cancel_hovered;
    int         ok_pressed;
    int         cancel_pressed;

    /* Dimensions calculées */
    int         dlg_h;

    /* Choices parsées */
    CldChoices  choices[CLD_MAX_FIELDS];

} CldState;

/* ── Forward ─────────────────────────────────────────────────────── */
static LRESULT CALLBACK cld__proc(HWND, UINT, WPARAM, LPARAM);
static WNDPROC           cld__orig_edit_proc = NULL;
static LRESULT CALLBACK  cld__edit_sub(HWND, UINT, WPARAM, LPARAM);

/* ── Parser choix MULTIVALUE ─────────────────────────────────────── */
static void cld__parse_choices(const char *str, CldChoices *out)
{
    out->count = 0;
    if (!str || !str[0]) return;
    char buf[512];
    strncpy(buf, str, 511);
    char *tok = strtok(buf, "|");
    while (tok && out->count < CLD_MAX_CHOICES) {
        strncpy(out->parts[out->count++], tok, 63);
        tok = strtok(NULL, "|");
    }
}

/* ── Hauteur totale ──────────────────────────────────────────────── */
static int cld__calc_height(int field_count, int has_message)
{
    int h = CLD_TITLE_H + CLD_PADDING;
    if (has_message) h += CLD_MSG_H + CLD_FIELD_GAP;
    if (field_count > 0)
        h += field_count * (CLD_FIELD_H + CLD_FIELD_GAP);
    h += CLD_PADDING + CLD_BTN_H;
    return h;
}

/* ── Y du premier champ ──────────────────────────────────────────── */
static int cld__fields_y(int has_message)
{
    int y = CLD_TITLE_H + CLD_PADDING;
    if (has_message) y += CLD_MSG_H + CLD_FIELD_GAP;
    return y;
}

/* ── Validation d'un champ ───────────────────────────────────────── */
static int cld__validate(CldField *f, const char *val)
{
    if (f->type == CLD_INT) {
        char *end;
        strtol(val, &end, 10);
        return (*end == '\0' || *end == '\r' || *end == '\n');
    }
    if (f->type == CLD_FLOAT) {
        char *end;
        strtod(val, &end);
        return (*end == '\0' || *end == '\r' || *end == '\n');
    }
    return 1;
}

/* ── Dessin fond + titre ─────────────────────────────────────────── */
static void cld__draw(CldState *st, cairo_t *cr, int w, int h)
{

    /* Fond général */
    cld__col_bg(cr);
    cairo_paint(cr);

    /* Zone titre */
    cairo_pattern_t *pg = cairo_pattern_create_linear(0, 0, 0, CLD_TITLE_H);
    cairo_pattern_add_color_stop_rgb(pg, 0.0, 0.14, 0.16, 0.22);
    cairo_pattern_add_color_stop_rgb(pg, 1.0, 0.10, 0.11, 0.15);
    cairo_rectangle(cr, 0, 0, w, CLD_TITLE_H);
    cairo_set_source(cr, pg);
    cairo_fill(cr);
    cairo_pattern_destroy(pg);

    /* Ligne séparatrice titre */
    cld__col_border(cr, 0.7);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, CLD_TITLE_H - 0.5);
    cairo_line_to(cr, w, CLD_TITLE_H - 0.5);
    cairo_stroke(cr);

    /* Titre */
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 13.0);
    cld__col_text(cr, 1.0);
    cairo_move_to(cr, CLD_PADDING, CLD_TITLE_H * 0.68);
    cairo_show_text(cr, st->title ? st->title : "");

    /* Message (msgbox) */
    if (st->message) {
        cairo_select_font_face(cr, "Segoe UI",
            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12.5);
        cld__col_text(cr, 0.9);
        double my = CLD_TITLE_H + CLD_PADDING + CLD_MSG_H * 0.6;
        cairo_move_to(cr, CLD_PADDING, my);
        cairo_show_text(cr, st->message);
    }

    /* Labels des champs */
    int fy = cld__fields_y(st->message != NULL);
    cairo_select_font_face(cr, "Segoe UI",
        CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 12.0);

    for (int i = 0; i < st->count; i++) {
        CldField *f  = &st->fields[i];
        double    ly = fy + i * (CLD_FIELD_H + CLD_FIELD_GAP);
        double    cy2 = ly + CLD_FIELD_H / 2.0;

        /* Label */
        cld__col_label(cr);
        cairo_move_to(cr, CLD_PADDING, cy2 + 4.5);
        cairo_show_text(cr, f->label);

        /* Fond zone edit */
        int ex = CLD_PADDING + CLD_LABEL_W;
        int ew = w - ex - CLD_PADDING;

        if (f->type == CLD_BOOL) {
            /* Checkbox dessinée en Cairo */
            double bx = ex + 2;
            double by2 = ly + (CLD_FIELD_H - 16) / 2.0;

            cld__rounded_rect(cr, bx, by2, 16, 16, 3);
            cld__col_bg2(cr);
            cairo_fill_preserve(cr);
            if (!st->field_valid[i])
                cld__col_error(cr, 0.8);
            else
                cld__col_border(cr, 0.7);
            cairo_set_line_width(cr, 1.2);
            cairo_stroke(cr);

            /* Coche si checked */
            if (st->fields[i].value[0] == '1') {
                cld__col_accent(cr, 1.0);
                cairo_set_line_width(cr, 2.0);
                cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
                cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
                cairo_move_to(cr, bx+3,  by2+8);
                cairo_line_to(cr, bx+6,  by2+12);
                cairo_line_to(cr, bx+13, by2+4);
                cairo_stroke(cr);
            }
        } else {
            /* Fond champ edit/combo */
            cld__rounded_rect(cr, ex, ly + 3, ew, CLD_FIELD_H - 6, 4);
            cld__col_bg2(cr);
            cairo_fill_preserve(cr);
            if (!st->field_valid[i])
                cld__col_error(cr, 0.8);
            else
                cld__col_border(cr, 0.5);
            cairo_set_line_width(cr, 1.0);
            cairo_stroke(cr);
        }
    }

    /* Zone boutons */
    int by_zone = h - CLD_BTN_H;
    cld__col_border(cr, 0.4);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0, by_zone + 0.5);
    cairo_line_to(cr, w, by_zone + 0.5);
    cairo_stroke(cr);

    /* Bouton OK */
    {
        int bx = (st->flags == CLD_OK_ONLY)
                 ? (w - CLD_BTN_W) / 2
                 : w - CLD_PADDING - CLD_BTN_W * 2 - 8;
        int bby = by_zone + (CLD_BTN_H - 26) / 2;

        cld__rounded_rect(cr, bx, bby, CLD_BTN_W, 26, 5);
        if (st->ok_pressed)
            cld__col_press(cr, 0.85);
        else if (st->ok_hovered)
            cld__col_accent(cr, 0.55);
        else
            cld__col_accent(cr, 0.35);
        cairo_fill_preserve(cr);
        cld__col_accent(cr, 0.8);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        cairo_set_font_size(cr, 12.0);
        cairo_text_extents_t te;
        cairo_text_extents(cr, "OK", &te);
        cld__col_text(cr, 1.0);
        cairo_move_to(cr, bx + (CLD_BTN_W - te.width) / 2.0 - te.x_bearing,
                          bby + 17);
        cairo_show_text(cr, "OK");
    }

    /* Bouton Annuler */
    if (st->flags == CLD_OK_CANCEL) {
        int bx = w - CLD_PADDING - CLD_BTN_W;
        int bby = by_zone + (CLD_BTN_H - 26) / 2;

        cld__rounded_rect(cr, bx, bby, CLD_BTN_W, 26, 5);
        if (st->cancel_pressed)
            cld__col_press(cr, 0.6);
        else if (st->cancel_hovered)
            cld__col_hover(cr, 0.12);
        else
            cld__col_bg2(cr);
        cairo_fill_preserve(cr);
        cld__col_border(cr, 0.6);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        cairo_text_extents_t te;
        cairo_text_extents(cr, "Annuler", &te);
        cld__col_text(cr, 0.85);
        cairo_move_to(cr, bx + (CLD_BTN_W - te.width) / 2.0 - te.x_bearing,
                          bby + 17);
        cairo_show_text(cr, "Annuler");
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surf);
}

/* ── Subclass EDIT — Entrée = OK, Echap = Cancel ─────────────────── */
static LRESULT CALLBACK cld__edit_sub(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp)
{
    if (msg == WM_KEYDOWN) {
        if (wp == VK_RETURN) {
            SendMessage(GetParent(hwnd), WM_COMMAND,
                        MAKEWPARAM(IDOK, BN_CLICKED), 0);
            return 0;
        }
        if (wp == VK_ESCAPE) {
            SendMessage(GetParent(hwnd), WM_COMMAND,
                        MAKEWPARAM(IDCANCEL, BN_CLICKED), 0);
            return 0;
        }
        if (wp == VK_TAB) {
            /* Focus suivant */
            SetFocus(GetNextDlgTabItem(GetParent(hwnd), hwnd,
                     GetKeyState(VK_SHIFT) < 0));
            return 0;
        }
    }
    return CallWindowProc(cld__orig_edit_proc, hwnd, msg, wp, lp);
}

/* ── Positions boutons OK/Cancel ─────────────────────────────────── */
static void cld__btn_rects(CldState *st, int w, int h,
                            RECT *rc_ok, RECT *rc_cancel)
{
    int by_zone = h - CLD_BTN_H;
    int bby     = by_zone + (CLD_BTN_H - 26) / 2;

    if (st->flags == CLD_OK_ONLY) {
        int bx = (w - CLD_BTN_W) / 2;
        SetRect(rc_ok, bx, bby, bx + CLD_BTN_W, bby + 26);
    } else {
        int bx_ok  = w - CLD_PADDING - CLD_BTN_W * 2 - 8;
        int bx_can = w - CLD_PADDING - CLD_BTN_W;
        SetRect(rc_ok,     bx_ok,  bby, bx_ok  + CLD_BTN_W, bby + 26);
        SetRect(rc_cancel, bx_can, bby, bx_can + CLD_BTN_W, bby + 26);
    }
}

/* ── WndProc dialogue ────────────────────────────────────────────── */
static LRESULT CALLBACK cld__proc(HWND hwnd, UINT msg,
                                   WPARAM wp, LPARAM lp)
{
    CldState *st = (CldState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (msg) {
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            if (st) cld__draw(st, ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }

    case WM_MOUSEMOVE: {
        if (!st) break;
        RECT rc; GetClientRect(hwnd, &rc);
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        RECT ro = {0}, rcan = {0};
        cld__btn_rects(st, rc.right, rc.bottom, &ro, &rcan);
        int oh  = PtInRect(&ro,  (POINT){mx,my});
        int cah = PtInRect(&rcan,(POINT){mx,my});
        if (oh != st->ok_hovered || cah != st->cancel_hovered) {
            st->ok_hovered     = oh;
            st->cancel_hovered = cah;
            ui_redraw(hwnd);
        }
        TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        if (st) {
            st->ok_hovered = st->cancel_hovered = 0;
            ui_redraw(hwnd);
        }
        return 0;

    case WM_LBUTTONDOWN: {
        if (!st) break;
        RECT rc; GetClientRect(hwnd, &rc);
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        RECT ro = {0}, rcan = {0};
        cld__btn_rects(st, rc.right, rc.bottom, &ro, &rcan);
        if (PtInRect(&ro,  (POINT){mx,my})) { st->ok_pressed = 1;     InvalidateRect(hwnd,NULL,FALSE); }
        if (PtInRect(&rcan,(POINT){mx,my})) { st->cancel_pressed = 1; InvalidateRect(hwnd,NULL,FALSE); }

        /* Checkbox BOOL — clic direct */
        int fy = cld__fields_y(st->message != NULL);
        for (int i = 0; i < st->count; i++) {
            if (st->fields[i].type != CLD_BOOL) continue;
            int ly  = fy + i * (CLD_FIELD_H + CLD_FIELD_GAP);
            int ex  = CLD_PADDING + CLD_LABEL_W + 2;
            int by2 = ly + (CLD_FIELD_H - 16) / 2;
            RECT rcb = { ex, by2, ex+16, by2+16 };
            if (PtInRect(&rcb, (POINT){mx,my})) {
                st->fields[i].value[0] = (st->fields[i].value[0]=='1') ? '0' : '1';
                st->fields[i].value[1] = '\0';
                ui_redraw(hwnd);
            }
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (!st) break;
        RECT rc; GetClientRect(hwnd, &rc);
        int mx = GET_X_LPARAM(lp), my = GET_Y_LPARAM(lp);
        RECT ro = {0}, rcan = {0};
        cld__btn_rects(st, rc.right, rc.bottom, &ro, &rcan);

        if (st->ok_pressed && PtInRect(&ro, (POINT){mx,my}))
            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDOK,BN_CLICKED), 0);
        if (st->cancel_pressed && PtInRect(&rcan, (POINT){mx,my}))
            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDCANCEL,BN_CLICKED), 0);

        st->ok_pressed = st->cancel_pressed = 0;
        ui_redraw(hwnd);
        return 0;
    }

    case WM_COMMAND: {
        if (!st) break;
        int cmd = LOWORD(wp);

        if (cmd == IDOK) {
            /* Valider tous les champs */
            int all_ok = 1;
            for (int i = 0; i < st->count; i++) {
                CldField *f = &st->fields[i];
                if (f->type == CLD_BOOL) { st->field_valid[i] = 1; continue; }
                if (f->type == CLD_MULTIVALUE) {
                    /* Récupérer texte du combo */
                    if (st->hwnd_combos[i]) {
                        int sel = (int)SendMessage(st->hwnd_combos[i], CB_GETCURSEL, 0, 0);
                        if (sel >= 0 && sel < st->choices[i].count)
                            strncpy(f->value, st->choices[i].parts[sel], 255);
                    }
                    st->field_valid[i] = 1;
                    continue;
                }
                /* EDIT — récupérer la valeur */
                if (st->hwnd_edits[i]) {
                    char buf[256] = "";
                    GetWindowText(st->hwnd_edits[i], buf, 255);
                    st->field_valid[i] = cld__validate(f, buf);
                    if (st->field_valid[i])
                        strncpy(f->value, buf, 255);
                    else
                        all_ok = 0;
                }
            }
            if (!all_ok) {
                ui_redraw(hwnd);
                return 0;
            }
            st->result = 1;
            EndDialog(hwnd, 1);
            return 0;
        }

        if (cmd == IDCANCEL) {
            st->result = 0;
            EndDialog(hwnd, 0);
            return 0;
        }

        /* Notification EDIT — revalider en temps réel */
        if (HIWORD(wp) == EN_CHANGE) {
            for (int i = 0; i < st->count; i++) {
                if ((HWND)lp == st->hwnd_edits[i]) {
                    char buf[256] = "";
                    GetWindowText(st->hwnd_edits[i], buf, 255);
                    int was = st->field_valid[i];
                    st->field_valid[i] = cld__validate(&st->fields[i], buf);
                    if (st->field_valid[i] != was)
                        ui_redraw(hwnd);
                    break;
                }
            }
        }
        return 0;
    }

    case WM_KEYDOWN:
        if (wp == VK_ESCAPE) {
            if (st) st->result = 0;
            EndDialog(hwnd, 0);
            return 0;
        }
        if (wp == VK_RETURN) {
            SendMessage(hwnd, WM_COMMAND, MAKEWPARAM(IDOK,BN_CLICKED), 0);
            return 0;
        }
        break;

    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX: {
        HDC hdc = (HDC)wp;
        SetTextColor(hdc, RGB(220, 225, 235));
        SetBkColor  (hdc, RGB(18,  20,  28));
        static HBRUSH br = NULL;
        if (!br) br = CreateSolidBrush(RGB(18, 20, 28));
        return (LRESULT)br;
    }
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

/* ── Enregistrement classe ───────────────────────────────────────── */
static void cld__register(HINSTANCE hi)
{
    static int done = 0;
    if (done) return; done = 1;
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof wc;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    ui_register_class("CLD_Dialog", cld__proc, 0, NULL);
}

/* ── Création des contrôles Win32 ────────────────────────────────── */
static void cld__create_controls(HWND hwnd, CldState *st, int w)
{
    int fy = cld__fields_y(st->message != NULL);

    /* Police monospace pour les edits */
    HFONT hfont = CreateFont(
        13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    for (int i = 0; i < st->count; i++) {
        CldField *f  = &st->fields[i];
        int       ly = fy + i * (CLD_FIELD_H + CLD_FIELD_GAP);
        int       ex = CLD_PADDING + CLD_LABEL_W;
        int       ew = w - ex - CLD_PADDING;
        int       ey = ly + 4;
        int       eh = CLD_FIELD_H - 8;

        st->field_valid[i]  = 1;
        st->hwnd_edits[i]   = NULL;
        st->hwnd_combos[i]  = NULL;
        st->hwnd_checks[i]  = NULL;

        if (f->type == CLD_BOOL) continue;  /* géré en Cairo */

        if (f->type == CLD_MULTIVALUE) {
            /* Parser les choix */
            cld__parse_choices(f->choices, &st->choices[i]);

            st->hwnd_combos[i] = CreateWindowEx(0, "COMBOBOX", "",
                WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
                ex+2, ey, ew-4, eh + 120,
                hwnd, (HMENU)(intptr_t)(1000+i), hi, NULL);

            if (st->hwnd_combos[i]) {
                SendMessage(st->hwnd_combos[i], WM_SETFONT, (WPARAM)hfont, TRUE);
                int sel = 0;
                for (int j = 0; j < st->choices[i].count; j++) {
                    SendMessage(st->hwnd_combos[i], CB_ADDSTRING, 0,
                                (LPARAM)st->choices[i].parts[j]);
                    if (strcmp(st->choices[i].parts[j], f->value) == 0)
                        sel = j;
                }
                SendMessage(st->hwnd_combos[i], CB_SETCURSEL, sel, 0);
            }
        } else {
            /* EDIT texte/int/float */
            st->hwnd_edits[i] = CreateWindowEx(
                0, "EDIT", f->value,
                WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                ex+4, ey+1, ew-8, eh-2,
                hwnd, (HMENU)(intptr_t)(2000+i), hi, NULL);

            if (st->hwnd_edits[i]) {
                SendMessage(st->hwnd_edits[i], WM_SETFONT, (WPARAM)hfont, TRUE);
                /* Subclasser pour Entrée/Echap/Tab */
                if (!cld__orig_edit_proc)
                    cld__orig_edit_proc = (WNDPROC)GetWindowLongPtr(
                        st->hwnd_edits[i], GWLP_WNDPROC);
                SetWindowLongPtr(st->hwnd_edits[i], GWLP_WNDPROC,
                                 (LONG_PTR)cld__edit_sub);
            }
        }
    }
}

/* ══════════════════════════════════════════════════════════════════
   API PUBLIQUE — IMPLÉMENTATION
   ══════════════════════════════════════════════════════════════════ */

int cld_show(HWND parent, const char *title,
             CldField *fields, int count, int flags)
{
    cld__register(hi);

    /* Préparer l'état */
    CldState st = {0};
    st.fields  = fields;
    st.count   = count < CLD_MAX_FIELDS ? count : CLD_MAX_FIELDS;
    st.flags   = flags;
    st.title   = title;
    st.message = NULL;
    st.result  = 0;

    /* Centrer sur le parent */
    int dlg_h = cld__calc_height(st.count, 0);
    RECT pr; GetWindowRect(parent, &pr);
    int px = pr.left + (pr.right  - pr.left  - CLD_W)   / 2;
    int py = pr.top  + (pr.bottom - pr.top   - dlg_h) / 2;

    HWND hwnd = ui_popup_create("CLD_Dialog", parent,
        px, py, CLD_W, dlg_h,
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, &st);
    ShowWindow(hwnd, SW_SHOW);

    /* DWM dark mode */
    ui_set_dark_mode(hwnd);

    /* Créer les contrôles */
    RECT rc; GetClientRect(hwnd, &rc);
    cld__create_controls(hwnd, &st, rc.right);

    /* Focus sur le premier edit */
    for (int i = 0; i < st.count; i++) {
        if (st.hwnd_edits[i]) {
            SetFocus(st.hwnd_edits[i]);
            SendMessage(st.hwnd_edits[i], EM_SETSEL, 0, -1);
            break;
        }
        if (st.hwnd_combos[i]) { SetFocus(st.hwnd_combos[i]); break; }
    }

    /* Boucle modale */
    MSG msg;
    while (IsWindow(hwnd) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return st.result;
}

int cld_msgbox(HWND parent, const char *title,
               const char *message, int flags)
{
    cld__register(hi);

    CldState st = {0};
    st.fields  = NULL;
    st.count   = 0;
    st.flags   = flags;
    st.title   = title;
    st.message = message;
    st.result  = 0;

    int dlg_h = cld__calc_height(0, 1);
    RECT pr; GetWindowRect(parent, &pr);
    int px = pr.left + (pr.right  - pr.left  - CLD_W)   / 2;
    int py = pr.top  + (pr.bottom - pr.top   - dlg_h) / 2;

    HWND hwnd = ui_popup_create("CLD_Dialog", parent,
        px, py, CLD_W, dlg_h,
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, &st);
    ShowWindow(hwnd, SW_SHOW);

    ui_set_dark_mode(hwnd);

    MSG msg;
    while (IsWindow(hwnd) && GetMessage(&msg, NULL, 0, 0)) {
        if (!IsDialogMessage(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    return st.result;
}

#endif /* CAIRO_DIALOG_IMPLEMENTATION */
#endif /* CAIRO_DIALOG_H */
