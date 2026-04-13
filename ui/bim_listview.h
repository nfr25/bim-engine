/*
 * bim_listview.h  –  Virtual listview Cairo/Win32 connectée à SQLite3
 * ──────────────────────────────────────────────────────────────────
 *
 *  STRUCTURE
 *  ─────────
 *   ┌─────┬──────────────────┬────────────┬────────────────────────┐
 *   │  id │ nom              │ x          │ y                      │  ← header
 *   ├─────┼──────────────────┼────────────┼────────────────────────┤
 *   │   1 │ Noeud_A          │   123.456  │   789.012              │
 *   │   2 │ Vanne_01         │   234.567  │   890.123              │  ← rows
 *   │ ... │ ...              │ ...        │ ...                    │
 *   └─────┴──────────────────┴────────────┴────────────────────────┘  ← scrollbar native
 *
 *  CARACTÉRISTIQUES
 *  ─────────────────
 *   - Virtuelle : seules les lignes visibles sont dessinées
 *   - La query SQLite est stockée dans le contrôle et ré-exécutable
 *   - Colonnes redimensionnables par drag du séparateur de header
 *   - Scrollbar verticale Win32 native
 *   - Colonnes détectées automatiquement depuis la query
 *   - Sélection de ligne (simple)
 *   - Double-clic → callback avec les données de la ligne
 *   - Tri par colonne au clic header (ASC/DESC, ORDER BY injecté)
 *   - Largeurs de colonnes : session uniquement (pas de persistance)
 *
 *  UTILISATION MINIMALE
 *  ─────────────────────
 *    #define BIM_LISTVIEW_IMPLEMENTATION
 *    #include "bim_listview.h"
 *
 *  CRÉATION
 *    BimListView *lv = blv_create(hwnd_parent, x, y, w, h, db_handle);
 *    blv_set_query(lv, "SELECT id, nom, x, y FROM bim_nodes WHERE layer=1");
 *    blv_set_cb(lv, on_row_dbl, on_sel, userdata);
 *
 *  REFRESH
 *    blv_refresh(lv);          // ré-exécute la query
 *    blv_set_query(lv, sql);   // nouvelle query + refresh auto
 *
 *  INTÉGRATION WndProc
 *    case WM_SIZE:    blv_resize(lv, x, y, new_w, new_h); break;
 *    case WM_DESTROY: blv_destroy(lv);                    break;
 *
 *  CALLBACKS
 *    typedef void (*BlvRowCb)(int row, int col_count,
 *                             const char **col_names,
 *                             const char **values, void *ud);
 *    typedef void (*BlvSelCb)(int row, void *ud);
 *
 * ──────────────────────────────────────────────────────────────────
 *  Dépendances : cairo, cairo-win32, sqlite3, ui_backend.h
 *  Compilateur : GCC / MinGW-w64 (MSYS2)
 * ──────────────────────────────────────────────────────────────────
 */

#ifndef BIM_LISTVIEW_H
#define BIM_LISTVIEW_H

#include "ui_backend.h"
#include <windows.h>
#include <windowsx.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <sqlite3.h>
#include <uxtheme.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

/* ── Dimensions ─────────────────────────────────────────────────────── */
#define BLV_HEADER_H      24    /* hauteur du header                    */
#define BLV_ROW_H         20    /* hauteur d'une ligne                  */
#define BLV_COL_MIN_W     30    /* largeur min d'une colonne            */
#define BLV_COL_DEFAULT_W 120   /* largeur par défaut                   */
#define BLV_MAX_COLS      32    /* colonnes max                         */
#define BLV_MAX_ROWS      200000 /* rows max (virtual, pas en mémoire)  */
#define BLV_CELL_PAD      6     /* padding horizontal cellule           */
#define BLV_SEP_GRAB      4     /* zone de grab du séparateur (±px)    */
#define BLV_LABEL_MAX     128   /* longueur max d'une valeur affichée   */
#define BLV_QUERY_MAX     4096  /* longueur max de la query             */
#define BLV_SORT_SQL_MAX  (BLV_QUERY_MAX + 128)

/* ── Couleurs ────────────────────────────────────────────────────────── */
#define BLV_COL_BG          0.11, 0.11, 0.12   /* fond rows            */
#define BLV_COL_HDR_BG      0.16, 0.16, 0.18   /* fond header          */
#define BLV_COL_HDR_HOVER   0.20, 0.20, 0.23   /* header col survolée  */
#define BLV_COL_HDR_SORT    0.22, 0.33, 0.55   /* header col triée     */
#define BLV_COL_ROW_ALT     0.13, 0.13, 0.14   /* lignes alternées     */
#define BLV_COL_SEL         0.20, 0.35, 0.65   /* ligne sélectionnée   */
#define BLV_COL_SEL_TXT     0.95, 0.95, 1.00   /* texte sélectionné    */
#define BLV_COL_BORDER      0.22, 0.22, 0.25   /* séparateurs          */
#define BLV_COL_TEXT        0.80, 0.80, 0.85   /* texte normal         */
#define BLV_COL_TEXT_DIM    0.45, 0.45, 0.50   /* texte NULL/vide      */
#define BLV_COL_HDR_TEXT    0.88, 0.88, 0.92   /* texte header         */
#define BLV_COL_SORT_ARROW  0.50, 0.75, 1.00   /* flèche de tri        */
#define BLV_COL_SEP_HOVER   0.40, 0.60, 1.00   /* séparateur en drag   */

/* ── Tri ─────────────────────────────────────────────────────────────── */
#define BLV_SORT_NONE  0
#define BLV_SORT_ASC   1
#define BLV_SORT_DESC  2

/* ── Types ───────────────────────────────────────────────────────────── */
typedef struct BimListView_ BimListView;

/* callback double-clic : row = index 0-based, valeurs de la ligne */
typedef void (*BlvRowCb)(int row, int col_count,
                         const char **col_names,
                         const char **values,
                         void *userdata);
/* callback sélection */
typedef void (*BlvSelCb)(int row, void *userdata);

/* ── API publique ────────────────────────────────────────────────────── */
BimListView *blv_create (HWND parent, int x, int y, int w, int h,
                         sqlite3 *db);
void         blv_destroy(BimListView *lv);
void         blv_resize (BimListView *lv, int x, int y, int w, int h);

void  blv_set_query  (BimListView *lv, const char *sql);
void  blv_refresh    (BimListView *lv);          /* ré-exécute la query */
void  blv_set_cb     (BimListView *lv, BlvRowCb row_dbl,
                      BlvSelCb sel, void *ud);

int   blv_row_count  (BimListView *lv);
int   blv_selected   (BimListView *lv);          /* -1 si aucun         */
HWND  blv_get_hwnd   (BimListView *lv);

/* ══════════════════════════════════════════════════════════════════════
   IMPLÉMENTATION
   ══════════════════════════════════════════════════════════════════════ */
#ifdef BIM_LISTVIEW_IMPLEMENTATION

/* ── Colonne ─────────────────────────────────────────────────────────── */
typedef struct {
    char  name[BLV_LABEL_MAX];   /* nom de colonne (depuis SQLite)      */
    int   w;                      /* largeur en pixels                   */
    int   sort;                   /* BLV_SORT_NONE / ASC / DESC          */
    int   hover;                  /* survol header                       */
} BlvCol;

/* ── Cache de données : fenêtre glissante ───────────────────────────── */
/*
 * On ne stocke PAS toutes les lignes en mémoire.
 * On garde un cache de BLV_CACHE_ROWS lignes autour de la vue.
 * Quand le scroll sort du cache, on ré-exécute la query avec LIMIT/OFFSET.
 */
#define BLV_CACHE_ROWS  512     /* taille de la fenêtre cache            */
#define BLV_CACHE_AHEAD 128     /* refetch si on approche du bord        */

typedef struct {
    int     offset;             /* première ligne du cache (0-based)     */
    int     count;              /* nombre de lignes dans le cache        */
    /* valeurs : tableau [count][col_count] de char*                     */
    /* stocké comme bloc contigu : char ***cells                         */
    char  **cells;              /* cells[row * col_count + col]          */
    int     col_count;
} BlvCache;

/* ── Structure principale ────────────────────────────────────────────── */
struct BimListView_ {
    HWND        hwnd;
    HWND        parent;
    sqlite3    *db;

    /* query */
    char        query[BLV_QUERY_MAX];       /* query de base             */
    char        query_sorted[BLV_SORT_SQL_MAX]; /* query + ORDER BY      */

    /* colonnes */
    BlvCol      cols[BLV_MAX_COLS];
    int         col_count;

    /* données */
    int         row_count;                  /* total rows (COUNT query)  */
    BlvCache    cache;

    /* scroll */
    int         scroll_top;                 /* première ligne visible    */
    int         visible_rows;              /* nombre de lignes visibles  */

    /* sélection */
    int         selected;                   /* -1 = aucun               */

    /* tri */
    int         sort_col;                   /* -1 = pas de tri           */
    int         sort_dir;                   /* BLV_SORT_ASC / DESC       */

    /* drag séparateur */
    int         drag_col;                   /* -1 = pas de drag          */
    int         drag_start_x;
    int         drag_start_w;

    /* hover */
    int         hover_col_sep;             /* index col dont on survole le sep */
    int         hover_row;                 /* -1 = aucun                */

    /* géométrie */
    int         x, y, w, h;
    int         content_w;                 /* somme des largeurs cols   */

    /* callbacks */
    BlvRowCb    cb_dbl;
    BlvSelCb    cb_sel;
    void       *cb_ud;

    /* tracking */
    int         mouse_tracked;
};

/* ── Forward declarations ────────────────────────────────────────────── */
static LRESULT CALLBACK blv__wnd_proc(HWND, UINT, WPARAM, LPARAM);
static void blv__draw          (BimListView *lv, cairo_t *cr, int w, int h);
static void blv__draw_header   (BimListView *lv, cairo_t *cr, int w);
static void blv__draw_rows     (BimListView *lv, cairo_t *cr, int w, int h);
static void blv__draw_cell     (cairo_t *cr, const char *text, int selected,
                                 double x, double y, double w, double h,
                                 int is_null);
static void blv__execute_query (BimListView *lv);
static void blv__count_rows    (BimListView *lv);
static void blv__fetch_cache   (BimListView *lv, int from_row);
static void blv__free_cache    (BimListView *lv);
static void blv__update_scroll (BimListView *lv);
static int  blv__col_at_x      (BimListView *lv, int mx, int *is_sep);
static int  blv__row_at_y      (BimListView *lv, int my);
static void blv__build_sort_query(BimListView *lv);
static const char *blv__cache_cell(BimListView *lv, int row, int col);

/* ── Enregistrement de classe ────────────────────────────────────────── */
static void blv__register_class(void)
{
    static int done = 0;
    if (done) return;
    done = 1;
    ui_register_class("BIM_ListView", blv__wnd_proc, CS_DBLCLKS,
                  LoadCursor(NULL, IDC_ARROW));
}

/* ── Création ────────────────────────────────────────────────────────── */
BimListView *blv_create(HWND parent, int x, int y, int w, int h, sqlite3 *db)
{
    blv__register_class();

    BimListView *lv = (BimListView*)calloc(1, sizeof *lv);
    if (!lv) return NULL;

    lv->parent    = parent;
    lv->db        = db;
    lv->x = x; lv->y = y; lv->w = w; lv->h = h;
    lv->selected  = -1;
    lv->sort_col  = -1;
    lv->sort_dir  = BLV_SORT_NONE;
    lv->drag_col  = -1;
    lv->hover_col_sep = -1;
    lv->hover_row = -1;
    lv->cache.cells = NULL;

    lv->hwnd = ui_subwnd_create("BIM_ListView", parent, x, y, w, h, lv);
    if (!lv->hwnd) { free(lv); return NULL; }

    /* scrollbar verticale native */
    SetScrollRange(lv->hwnd, SB_VERT, 0, 0, FALSE);
    ShowScrollBar(lv->hwnd, SB_VERT, TRUE);
    /* Scrollbar dark mode */
    SetWindowTheme(lv->hwnd, L"DarkMode_Explorer", NULL);

    return lv;
}

/* ── Destruction ─────────────────────────────────────────────────────── */
void blv_destroy(BimListView *lv)
{
    if (!lv) return;
    blv__free_cache(lv);
    if (lv->hwnd) DestroyWindow(lv->hwnd);
    free(lv);
}

/* ── Resize ──────────────────────────────────────────────────────────── */
void blv_resize(BimListView *lv, int x, int y, int w, int h)
{
    if (!lv) return;
    lv->x = x; lv->y = y; lv->w = w; lv->h = h;
    MoveWindow(lv->hwnd, x, y, w, h, TRUE);
    lv->visible_rows = (h - BLV_HEADER_H) / BLV_ROW_H;
    blv__update_scroll(lv);
    ui_redraw(lv->hwnd);
}

/* ── Set query ───────────────────────────────────────────────────────── */
void blv_set_query(BimListView *lv, const char *sql)
{
    if (!lv || !sql) return;
    strncpy(lv->query, sql, BLV_QUERY_MAX - 1);
    lv->query[BLV_QUERY_MAX - 1] = '\0';
    lv->sort_col = -1;
    lv->sort_dir = BLV_SORT_NONE;
    blv_refresh(lv);
}

/* ── Refresh ─────────────────────────────────────────────────────────── */
void blv_refresh(BimListView *lv)
{
    if (!lv || !lv->db || lv->query[0] == '\0') return;
    blv__execute_query(lv);
    blv__count_rows(lv);
    lv->scroll_top = 0;
    lv->selected   = -1;
    lv->visible_rows = (lv->h - BLV_HEADER_H) / BLV_ROW_H;
    blv__fetch_cache(lv, 0);
    blv__update_scroll(lv);
    ui_redraw(lv->hwnd);
}

/* ── Callbacks ───────────────────────────────────────────────────────── */
void blv_set_cb(BimListView *lv, BlvRowCb row_dbl, BlvSelCb sel, void *ud)
{
    if (!lv) return;
    lv->cb_dbl = row_dbl;
    lv->cb_sel = sel;
    lv->cb_ud  = ud;
}

/* ── Accesseurs ──────────────────────────────────────────────────────── */
int  blv_row_count(BimListView *lv) { return lv ? lv->row_count : 0; }
int  blv_selected (BimListView *lv) { return lv ? lv->selected  : -1; }
HWND blv_get_hwnd (BimListView *lv) { return lv ? lv->hwnd      : NULL; }

/* ═══════════════════════════════════════════════════════════════════════
   FONCTIONS INTERNES — SQLite
   ═══════════════════════════════════════════════════════════════════════ */

/* ── Construction de la query triée ─────────────────────────────────── */
static void blv__build_sort_query(BimListView *lv)
{
    if (lv->sort_col < 0 || lv->sort_col >= lv->col_count ||
        lv->sort_dir == BLV_SORT_NONE) {
        strncpy(lv->query_sorted, lv->query, BLV_SORT_SQL_MAX - 1);
        lv->query_sorted[BLV_SORT_SQL_MAX - 1] = '\0';
        return;
    }
    /* Envelopper dans un SELECT pour injecter proprement ORDER BY.
     * Ex: SELECT * FROM (SELECT ...) ORDER BY "col" ASC             */
    snprintf(lv->query_sorted, BLV_SORT_SQL_MAX,
             "SELECT * FROM (%s) ORDER BY \"%s\" %s",
             lv->query,
             lv->cols[lv->sort_col].name,
             lv->sort_dir == BLV_SORT_ASC ? "ASC" : "DESC");
}

/* ── Exécution initiale (détecte les colonnes) ───────────────────────── */
static void blv__execute_query(BimListView *lv)
{
    blv__build_sort_query(lv);

    /* query limitée à 1 row pour détecter les colonnes */
    char probe[BLV_SORT_SQL_MAX + 32];
    snprintf(probe, sizeof probe, "SELECT * FROM (%s) LIMIT 1",
             lv->query_sorted);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(lv->db, probe, -1, &stmt, NULL) != SQLITE_OK)
        return;

    sqlite3_step(stmt);

    int nc = sqlite3_column_count(stmt);
    if (nc > BLV_MAX_COLS) nc = BLV_MAX_COLS;
    lv->col_count = nc;

    lv->content_w = 0;
    for (int i = 0; i < nc; i++) {
        const char *nm = sqlite3_column_name(stmt, i);
        strncpy(lv->cols[i].name, nm ? nm : "", BLV_LABEL_MAX - 1);
        lv->cols[i].name[BLV_LABEL_MAX - 1] = '\0';
        /* conserver la largeur si elle avait déjà été ajustée */
        if (lv->cols[i].w < BLV_COL_MIN_W)
            lv->cols[i].w = BLV_COL_DEFAULT_W;
        lv->content_w += lv->cols[i].w;
    }

    sqlite3_finalize(stmt);
}

/* ── Comptage total des rows ─────────────────────────────────────────── */
static void blv__count_rows(BimListView *lv)
{
    char count_sql[BLV_SORT_SQL_MAX + 32];
    snprintf(count_sql, sizeof count_sql,
             "SELECT COUNT(*) FROM (%s)", lv->query_sorted);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(lv->db, count_sql, -1, &stmt, NULL) != SQLITE_OK) {
        lv->row_count = 0;
        return;
    }
    lv->row_count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW)
        lv->row_count = sqlite3_column_int(stmt, 0);
    sqlite3_finalize(stmt);
}

/* ── Libération du cache ─────────────────────────────────────────────── */
static void blv__free_cache(BimListView *lv)
{
    if (!lv->cache.cells) return;
    int total = lv->cache.count * lv->cache.col_count;
    for (int i = 0; i < total; i++)
        free(lv->cache.cells[i]);
    free(lv->cache.cells);
    lv->cache.cells    = NULL;
    lv->cache.count    = 0;
    lv->cache.col_count = 0;
}

/* ── Fetch d'une fenêtre de cache ────────────────────────────────────── */
static void blv__fetch_cache(BimListView *lv, int from_row)
{
    if (!lv->db || lv->col_count == 0) return;
    if (from_row < 0) from_row = 0;

    blv__free_cache(lv);

    int limit = BLV_CACHE_ROWS;
    if (from_row + limit > lv->row_count)
        limit = lv->row_count - from_row;
    if (limit <= 0) {
        lv->cache.offset    = from_row;
        lv->cache.count     = 0;
        lv->cache.col_count = lv->col_count;
        return;
    }

    char fetch_sql[BLV_SORT_SQL_MAX + 64];
    snprintf(fetch_sql, sizeof fetch_sql,
             "SELECT * FROM (%s) LIMIT %d OFFSET %d",
             lv->query_sorted, limit, from_row);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(lv->db, fetch_sql, -1, &stmt, NULL) != SQLITE_OK)
        return;

    /* allouer le tableau de cellules */
    int total = limit * lv->col_count;
    lv->cache.cells = (char**)calloc(total, sizeof(char*));
    if (!lv->cache.cells) { sqlite3_finalize(stmt); return; }

    int row = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW && row < limit) {
        for (int c = 0; c < lv->col_count; c++) {
            int idx = row * lv->col_count + c;
            const char *val = (const char*)sqlite3_column_text(stmt, c);
            if (val) {
                lv->cache.cells[idx] = (char*)malloc(strlen(val) + 1);
                if (lv->cache.cells[idx])
                    strcpy(lv->cache.cells[idx], val);
            } else {
                lv->cache.cells[idx] = NULL; /* NULL SQLite */
            }
        }
        row++;
    }

    lv->cache.offset    = from_row;
    lv->cache.count     = row;
    lv->cache.col_count = lv->col_count;
    sqlite3_finalize(stmt);
}

/* ── Accès à une cellule du cache ────────────────────────────────────── */
static const char *blv__cache_cell(BimListView *lv, int row, int col)
{
    int local = row - lv->cache.offset;
    if (local < 0 || local >= lv->cache.count) return NULL;
    if (col < 0 || col >= lv->col_count)       return NULL;
    return lv->cache.cells[local * lv->col_count + col];
}

/* ── Mise à jour scrollbar ───────────────────────────────────────────── */
static void blv__update_scroll(BimListView *lv)
{
    SCROLLINFO si = {sizeof si, SIF_ALL, 0, 0, 0, 0, 0};
    si.nMin   = 0;
    si.nMax   = lv->row_count > 0 ? lv->row_count - 1 : 0;
    si.nPage  = lv->visible_rows > 0 ? lv->visible_rows : 1;
    si.nPos   = lv->scroll_top;
    SetScrollInfo(lv->hwnd, SB_VERT, &si, TRUE);
}

/* ── Hit tests ───────────────────────────────────────────────────────── */

/* Retourne l'index de la colonne à mx.
 * is_sep non-NULL : positionné à 1 si on est sur un séparateur de colonne */
static int blv__col_at_x(BimListView *lv, int mx, int *is_sep)
{
    if (is_sep) *is_sep = 0;
    int x = 0;
    for (int i = 0; i < lv->col_count; i++) {
        x += lv->cols[i].w;
        if (is_sep && abs(mx - x) <= BLV_SEP_GRAB) {
            *is_sep = 1;
            return i;
        }
        if (mx < x) return i;
    }
    return lv->col_count - 1;
}

/* Retourne l'index de la ligne visuelle à my (relative au contenu, sans header) */
static int blv__row_at_y(BimListView *lv, int my)
{
    if (my < BLV_HEADER_H) return -1;
    int row = lv->scroll_top + (my - BLV_HEADER_H) / BLV_ROW_H;
    if (row >= lv->row_count) return -1;
    return row;
}

/* ═══════════════════════════════════════════════════════════════════════
   DESSIN CAIRO
   ═══════════════════════════════════════════════════════════════════════ */

/* ── Flèche de tri dans le header ────────────────────────────────────── */
static void blv__draw_sort_arrow(cairo_t *cr, double cx, double cy, int dir)
{
    cairo_set_source_rgb(cr, BLV_COL_SORT_ARROW);
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    double sz = 4.0;
    if (dir == BLV_SORT_ASC) {
        cairo_move_to(cr, cx,      cy - sz);
        cairo_line_to(cr, cx + sz, cy + sz);
        cairo_line_to(cr, cx - sz, cy + sz);
        cairo_close_path(cr);
    } else {
        cairo_move_to(cr, cx,      cy + sz);
        cairo_line_to(cr, cx + sz, cy - sz);
        cairo_line_to(cr, cx - sz, cy - sz);
        cairo_close_path(cr);
    }
    cairo_fill(cr);
}

/* ── Header ──────────────────────────────────────────────────────────── */
static void blv__draw_header(BimListView *lv, cairo_t *cr, int w)
{
    cairo_select_font_face(cr, "Segoe UI",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 11.0);

    cairo_font_extents_t fext;
    cairo_font_extents(cr, &fext);

    int x = 0;
    for (int i = 0; i < lv->col_count; i++) {
        int cw = lv->cols[i].w;
        int sorted = (i == lv->sort_col);

        /* fond */
        if (sorted)
            cairo_set_source_rgb(cr, BLV_COL_HDR_SORT);
        else if (lv->cols[i].hover)
            cairo_set_source_rgb(cr, BLV_COL_HDR_HOVER);
        else
            cairo_set_source_rgb(cr, BLV_COL_HDR_BG);

        cairo_rectangle(cr, x, 0, cw, BLV_HEADER_H);
        cairo_fill(cr);

        /* texte */
        cairo_set_source_rgb(cr, BLV_COL_HDR_TEXT);
        double ty = (BLV_HEADER_H - fext.height) / 2.0 + fext.ascent;

        /* zone texte : laisser de la place pour la flèche si tri */
        double text_w = (double)cw - 2 * BLV_CELL_PAD;
        if (sorted) text_w -= 14.0;

        /* troncature */
        char display[BLV_LABEL_MAX + 4];
        strncpy(display, lv->cols[i].name, BLV_LABEL_MAX);
        display[BLV_LABEL_MAX] = '\0';
        cairo_text_extents_t ext;
        cairo_text_extents(cr, display, &ext);
        if (ext.width > text_w && text_w > 16.0) {
            int len = (int)strlen(display);
            while (len > 1) {
                display[--len] = '\0';
                char tmp[BLV_LABEL_MAX + 4];
                snprintf(tmp, sizeof tmp, "%s…", display);
                cairo_text_extents(cr, tmp, &ext);
                if (ext.width <= text_w) {
                    strncpy(display, tmp, sizeof display - 1);
                    break;
                }
            }
        }
        cairo_move_to(cr, x + BLV_CELL_PAD, ty);
        cairo_show_text(cr, display);

        /* flèche de tri */
        if (sorted && lv->sort_dir != BLV_SORT_NONE) {
            double ax = x + cw - BLV_CELL_PAD - 6.0;
            double ay = BLV_HEADER_H / 2.0;
            blv__draw_sort_arrow(cr, ax, ay, lv->sort_dir);
        }

        /* séparateur de colonne */
        cairo_set_source_rgba(cr, BLV_COL_BORDER, 1.0);
        if (lv->hover_col_sep == i)
            cairo_set_source_rgb(cr, BLV_COL_SEP_HOVER);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, x + cw - 0.5, 3);
        cairo_line_to(cr, x + cw - 0.5, BLV_HEADER_H - 3);
        cairo_stroke(cr);

        x += cw;
    }

    /* ligne basse du header */
    cairo_set_source_rgb(cr, BLV_COL_BORDER);
    cairo_set_line_width(cr, 1.0);
    cairo_move_to(cr, 0,   BLV_HEADER_H - 0.5);
    cairo_line_to(cr, (double)w, BLV_HEADER_H - 0.5);
    cairo_stroke(cr);
}

/* ── Cellule ─────────────────────────────────────────────────────────── */
static void blv__draw_cell(cairo_t *cr, const char *text, int selected,
                            double cx, double cy, double cw, double ch,
                            int is_null)
{
    cairo_save(cr);
    cairo_rectangle(cr, cx, cy, cw, ch);
    cairo_clip(cr);

    if (is_null || !text) {
        cairo_set_source_rgb(cr, BLV_COL_TEXT_DIM);
        cairo_select_font_face(cr, "Segoe UI",
                               CAIRO_FONT_SLANT_ITALIC,
                               CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0);
        cairo_font_extents_t fext;
        cairo_font_extents(cr, &fext);
        cairo_move_to(cr, cx + BLV_CELL_PAD,
                      cy + (ch - fext.height) / 2.0 + fext.ascent);
        cairo_show_text(cr, "NULL");
    } else {
        if (selected)
            cairo_set_source_rgb(cr, BLV_COL_SEL_TXT);
        else
            cairo_set_source_rgb(cr, BLV_COL_TEXT);

        cairo_select_font_face(cr, "Segoe UI",
                               CAIRO_FONT_SLANT_NORMAL,
                               CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0);
        cairo_font_extents_t fext;
        cairo_font_extents(cr, &fext);
        cairo_move_to(cr, cx + BLV_CELL_PAD,
                      cy + (ch - fext.height) / 2.0 + fext.ascent);
        cairo_show_text(cr, text);
    }
    cairo_restore(cr);
}

/* ── Lignes ──────────────────────────────────────────────────────────── */
static void blv__draw_rows(BimListView *lv, cairo_t *cr, int w, int h)
{
    int row_area_h = h - BLV_HEADER_H;
    if (row_area_h <= 0) return;

    cairo_save(cr);
    cairo_rectangle(cr, 0, BLV_HEADER_H, (double)w, (double)row_area_h);
    cairo_clip(cr);

    cairo_select_font_face(cr, "Segoe UI",
                           CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_NORMAL);
    cairo_set_font_size(cr, 11.0);

    for (int vi = 0; vi < lv->visible_rows + 1; vi++) {
        int row = lv->scroll_top + vi;
        if (row >= lv->row_count) break;

        double ry = BLV_HEADER_H + vi * BLV_ROW_H;
        int selected = (row == lv->selected);
        int hovered  = (row == lv->hover_row && !selected);

        /* fond de ligne */
        if (selected)
            cairo_set_source_rgb(cr, BLV_COL_SEL);
        else if (hovered)
            cairo_set_source_rgba(cr, 0.18, 0.18, 0.22, 1.0);
        else if (row % 2 == 1)
            cairo_set_source_rgb(cr, BLV_COL_ROW_ALT);
        else
            cairo_set_source_rgb(cr, BLV_COL_BG);

        cairo_rectangle(cr, 0, ry, (double)w, (double)BLV_ROW_H);
        cairo_fill(cr);

        /* cellules */
        int cx = 0;
        for (int c = 0; c < lv->col_count; c++) {
            const char *val = blv__cache_cell(lv, row, c);
            blv__draw_cell(cr, val, selected,
                           (double)cx, ry,
                           (double)lv->cols[c].w, (double)BLV_ROW_H,
                           val == NULL);

            /* séparateur vertical */
            cairo_set_source_rgba(cr, BLV_COL_BORDER, 0.6);
            cairo_set_line_width(cr, 1.0);
            cairo_move_to(cr, cx + lv->cols[c].w - 0.5, ry + 2);
            cairo_line_to(cr, cx + lv->cols[c].w - 0.5, ry + BLV_ROW_H - 2);
            cairo_stroke(cr);

            cx += lv->cols[c].w;
        }
    }

    cairo_restore(cr);
}

/* ── Dessin principal ────────────────────────────────────────────────── */
static void blv__draw(BimListView *lv, cairo_t *cr, int w, int h)
{
    /* fond général */
    cairo_set_source_rgb(cr, BLV_COL_BG);
    cairo_paint(cr);

    if (lv->col_count == 0) {
        /* pas de query — message */
        cairo_set_source_rgb(cr, BLV_COL_TEXT_DIM);
        cairo_select_font_face(cr, "Segoe UI",
                               CAIRO_FONT_SLANT_ITALIC,
                               CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12.0);
        cairo_move_to(cr, 12, h / 2.0);
        cairo_show_text(cr, "Aucune query");
        return;
    }

    blv__draw_rows(lv, cr, w, h);
    blv__draw_header(lv, cr, w);

    /* message "0 résultats" */
    if (lv->row_count == 0) {
        cairo_set_source_rgb(cr, BLV_COL_TEXT_DIM);
        cairo_select_font_face(cr, "Segoe UI",
                               CAIRO_FONT_SLANT_ITALIC,
                               CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 12.0);
        cairo_move_to(cr, 12, BLV_HEADER_H + 28);
        cairo_show_text(cr, "0 résultats");
    }
}

/* ═══════════════════════════════════════════════════════════════════════
   WndProc
   ═══════════════════════════════════════════════════════════════════════ */
static LRESULT CALLBACK blv__wnd_proc(HWND hwnd, UINT msg,
                                       WPARAM wp, LPARAM lp)
{
    BimListView *lv = ui_get_data(BimListView, hwnd);
    if (!lv) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {

    /* ── Peinture ──────────────────────────────────────────────────── */
    case WM_PAINT: {
        UI_PAINT_BEGIN(hwnd, ctx);
            blv__draw(lv, ctx.cr, ctx.w, ctx.h);
        UI_PAINT_END(hwnd, ctx);
        return 0;
    }

    /* ── Scroll vertical ───────────────────────────────────────────── */
    case WM_VSCROLL: {
        SCROLLINFO si = {sizeof si, SIF_ALL};
        GetScrollInfo(hwnd, SB_VERT, &si);
        int old_top = lv->scroll_top;

        switch (LOWORD(wp)) {
        case SB_LINEUP:       lv->scroll_top--;            break;
        case SB_LINEDOWN:     lv->scroll_top++;            break;
        case SB_PAGEUP:       lv->scroll_top -= lv->visible_rows; break;
        case SB_PAGEDOWN:     lv->scroll_top += lv->visible_rows; break;
        case SB_THUMBTRACK:
        case SB_THUMBPOSITION: lv->scroll_top = si.nTrackPos; break;
        case SB_TOP:          lv->scroll_top = 0;         break;
        case SB_BOTTOM:       lv->scroll_top = lv->row_count; break;
        }

        int max_top = lv->row_count - lv->visible_rows;
        if (max_top < 0) max_top = 0;
        if (lv->scroll_top < 0)       lv->scroll_top = 0;
        if (lv->scroll_top > max_top) lv->scroll_top = max_top;

        if (lv->scroll_top != old_top) {
            /* refetch si on sort du cache */
            int local = lv->scroll_top - lv->cache.offset;
            if (local < 0 || local + lv->visible_rows > lv->cache.count) {
                int fetch_from = lv->scroll_top - BLV_CACHE_AHEAD;
                if (fetch_from < 0) fetch_from = 0;
                blv__fetch_cache(lv, fetch_from);
            }
            blv__update_scroll(lv);
            ui_redraw(hwnd);
        }
        return 0;
    }

    /* ── Molette souris ────────────────────────────────────────────── */
    case WM_MOUSEWHEEL: {
        int delta = GET_WHEEL_DELTA_WPARAM(wp);
        int lines = delta > 0 ? -3 : 3;
        lv->scroll_top += lines;
        int max_top = lv->row_count - lv->visible_rows;
        if (max_top < 0) max_top = 0;
        if (lv->scroll_top < 0)       lv->scroll_top = 0;
        if (lv->scroll_top > max_top) lv->scroll_top = max_top;
        /* refetch si besoin */
        int local = lv->scroll_top - lv->cache.offset;
        if (local < 0 || local + lv->visible_rows > lv->cache.count) {
            int ff = lv->scroll_top - BLV_CACHE_AHEAD;
            if (ff < 0) ff = 0;
            blv__fetch_cache(lv, ff);
        }
        blv__update_scroll(lv);
        ui_redraw(hwnd);
        return 0;
    }

    /* ── Mouvement souris ──────────────────────────────────────────── */
    case WM_MOUSEMOVE: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);

        if (!lv->mouse_tracked) {
            TRACKMOUSEEVENT tme = {sizeof tme, TME_LEAVE, hwnd, 0};
            TrackMouseEvent(&tme);
            lv->mouse_tracked = 1;
        }

        int need_redraw = 0;

        /* drag séparateur de colonne */
        if (lv->drag_col >= 0 && (GetKeyState(VK_LBUTTON) & 0x8000)) {
            int delta = mx - lv->drag_start_x;
            int new_w = lv->drag_start_w + delta;
            if (new_w < BLV_COL_MIN_W) new_w = BLV_COL_MIN_W;
            lv->cols[lv->drag_col].w = new_w;
            lv->content_w = 0;
            for (int i = 0; i < lv->col_count; i++)
                lv->content_w += lv->cols[i].w;
            need_redraw = 1;
            SetCursor(LoadCursor(NULL, IDC_SIZEWE));
            break;
        }

        /* hover header */
        if (my < BLV_HEADER_H) {
            int is_sep = 0;
            int col = blv__col_at_x(lv, mx, &is_sep);
            int sep = is_sep ? col : -1;
            if (sep != lv->hover_col_sep) {
                lv->hover_col_sep = sep;
                need_redraw = 1;
                SetCursor(is_sep ? LoadCursor(NULL, IDC_SIZEWE)
                                 : LoadCursor(NULL, IDC_ARROW));
            }
            /* hover colonne */
            for (int i = 0; i < lv->col_count; i++) {
                int h_col = (!is_sep && i == col);
                if (lv->cols[i].hover != h_col) {
                    lv->cols[i].hover = h_col;
                    need_redraw = 1;
                }
            }
            if (lv->hover_row != -1) {
                lv->hover_row = -1;
                need_redraw = 1;
            }
        } else {
            /* hover row */
            int row = blv__row_at_y(lv, my);
            if (row != lv->hover_row) {
                lv->hover_row = row;
                need_redraw = 1;
            }
            if (lv->hover_col_sep != -1) {
                lv->hover_col_sep = -1;
                need_redraw = 1;
            }
            for (int i = 0; i < lv->col_count; i++) {
                if (lv->cols[i].hover) { lv->cols[i].hover = 0; need_redraw = 1; }
            }
            SetCursor(LoadCursor(NULL, IDC_ARROW));
        }

        if (need_redraw) ui_redraw(hwnd);
        return 0;
    }

    case WM_MOUSELEAVE: {
        lv->mouse_tracked    = 0;
        lv->hover_row        = -1;
        lv->hover_col_sep    = -1;
        for (int i = 0; i < lv->col_count; i++) lv->cols[i].hover = 0;
        ui_redraw(hwnd);
        return 0;
    }

    /* ── Clic gauche ───────────────────────────────────────────────── */
    case WM_LBUTTONDOWN: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        SetCapture(hwnd);

        /* clic header */
        if (my < BLV_HEADER_H) {
            int is_sep = 0;
            int col = blv__col_at_x(lv, mx, &is_sep);
            if (is_sep) {
                /* début drag redimensionnement */
                lv->drag_col     = col;
                lv->drag_start_x = mx;
                lv->drag_start_w = lv->cols[col].w;
            } else {
                /* tri par colonne */
                if (lv->sort_col == col) {
                    lv->sort_dir = (lv->sort_dir == BLV_SORT_ASC)
                                   ? BLV_SORT_DESC : BLV_SORT_ASC;
                } else {
                    lv->sort_col = col;
                    lv->sort_dir = BLV_SORT_ASC;
                }
                /* reset sort si double toggle */
                blv__build_sort_query(lv);
                blv__count_rows(lv);
                lv->scroll_top = 0;
                lv->selected   = -1;
                blv__fetch_cache(lv, 0);
                blv__update_scroll(lv);
                ui_redraw(hwnd);
            }
            return 0;
        }

        /* clic row */
        int row = blv__row_at_y(lv, my);
        if (row >= 0) {
            lv->selected = row;
            if (lv->cb_sel) lv->cb_sel(row, lv->cb_ud);
            ui_redraw(hwnd);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (lv->drag_col >= 0) {
            lv->drag_col = -1;
            ReleaseCapture();
        } else {
            ReleaseCapture();
        }
        return 0;
    }

    /* ── Double-clic ───────────────────────────────────────────────── */
    case WM_LBUTTONDBLCLK: {
        int mx = GET_X_LPARAM(lp);
        int my = GET_Y_LPARAM(lp);
        if (my < BLV_HEADER_H) return 0;

        int row = blv__row_at_y(lv, my);
        if (row < 0 || !lv->cb_dbl) return 0;

        /* s'assurer que la ligne est dans le cache */
        int local = row - lv->cache.offset;
        if (local < 0 || local >= lv->cache.count) {
            blv__fetch_cache(lv, row);
            local = row - lv->cache.offset;
        }
        if (local < 0 || local >= lv->cache.count) return 0;

        /* construire tableau de pointeurs pour le callback */
        const char **names  = (const char**)malloc(lv->col_count * sizeof(char*));
        const char **values = (const char**)malloc(lv->col_count * sizeof(char*));
        if (names && values) {
            for (int c = 0; c < lv->col_count; c++) {
                names [c] = lv->cols[c].name;
                values[c] = lv->cache.cells[local * lv->col_count + c];
            }
            lv->cb_dbl(row, lv->col_count, names, values, lv->cb_ud);
        }
        free(names);
        free(values);
        return 0;
    }

    /* ── Resize ────────────────────────────────────────────────────── */
    case WM_SIZE: {
        RECT rc; GetClientRect(hwnd, &rc);
        lv->w = rc.right; lv->h = rc.bottom;
        lv->visible_rows = (lv->h - BLV_HEADER_H) / BLV_ROW_H;
        blv__update_scroll(lv);
        ui_redraw(hwnd);
        return 0;
    }

    case WM_ERASEBKGND:
        return 1;
    }

    return DefWindowProc(hwnd, msg, wp, lp);
}

#endif /* BIM_LISTVIEW_IMPLEMENTATION */
#endif /* BIM_LISTVIEW_H */

/*
 * ════════════════════════════════════════════════════════════════════
 * GUIDE D'INTÉGRATION dans bim.c
 * ════════════════════════════════════════════════════════════════════
 *
 *  1. INCLUDES
 *     #define BIM_LISTVIEW_IMPLEMENTATION
 *     #include "ui/bim_listview.h"
 *
 *  2. CRÉATION d'un tab avec listview (ouverture d'une query)
 *
 *     static void open_query_tab(CairoTabPane *tp, sqlite3 *db,
 *                                const char *label, const char *sql)
 *     {
 *         RECT rc;
 *         tp_get_content_rect(tp, &rc);
 *         int cw = rc.right  - rc.left;
 *         int ch = rc.bottom - rc.top;
 *
 *         BimListView *lv = blv_create(tp->hwnd,
 *                                      rc.left, rc.top, cw, ch, db);
 *         blv_set_query(lv, sql);
 *         blv_set_cb(lv, on_row_dblclick, on_row_select, lv);
 *
 *         int idx = tp_tab_add(tp, label, 1);          // closable
 *         tp_tab_set_content(tp, idx, blv_get_hwnd(lv));
 *         tp_tab_select(tp, idx);
 *     }
 *
 *  3. CALLBACK double-clic (ex: centrer le canvas sur l'entité)
 *
 *     static void on_row_dblclick(int row, int nc,
 *                                 const char **names,
 *                                 const char **values, void *ud)
 *     {
 *         // chercher la colonne "id"
 *         for (int i = 0; i < nc; i++) {
 *             if (strcmp(names[i], "id") == 0 && values[i]) {
 *                 int id = atoi(values[i]);
 *                 canvas_focus_entity(g_canvas, id);
 *                 break;
 *             }
 *         }
 *     }
 *
 *  4. FERMETURE d'un tab (dans on_tab_event)
 *
 *     case TP_EV_CLOSE: {
 *         HWND hc = tp_tab_get_content(g_tabpane, tab_idx);
 *         if (hc) {
 *             BimListView *lv = (BimListView*)GetWindowLongPtr(
 *                                   hc, GWLP_USERDATA);
 *             if (lv) blv_destroy(lv);
 *             // blv_destroy détruit déjà le HWND
 *         }
 *         break;
 *     }
 *
 *  5. REFRESH après modification de la DB
 *
 *     // Après un INSERT/UPDATE/DELETE, rafraîchir tous les tabs actifs
 *     for (int i = 0; i < tp_tab_count(g_tabpane); i++) {
 *         HWND hc = tp_tab_get_content(g_tabpane, i);
 *         if (!hc) continue;
 *         BimListView *lv = (BimListView*)GetWindowLongPtr(
 *                               hc, GWLP_USERDATA);
 *         if (lv) blv_refresh(lv);
 *     }
 *
 * ════════════════════════════════════════════════════════════════════
 * PATTERN "SÉLECTION → TAB"
 * ════════════════════════════════════════════════════════════════════
 *
 * Quand l'utilisateur sélectionne des entités sur le canvas :
 *
 *     void on_canvas_selection(int *ids, int count, void *ud)
 *     {
 *         if (count == 0) return;
 *
 *         // Construire une query IN (id1, id2, ...)
 *         char sql[4096];
 *         int pos = snprintf(sql, sizeof sql,
 *             "SELECT e.id, l.nom AS layer, e.type "
 *             "FROM bim_entities e "
 *             "JOIN bim_layers l ON l.id = e.layer_id "
 *             "WHERE e.id IN (");
 *         for (int i = 0; i < count; i++) {
 *             pos += snprintf(sql + pos, sizeof sql - pos,
 *                             i ? ",%d" : "%d", ids[i]);
 *         }
 *         strncat(sql, ")", sizeof sql - pos - 1);
 *
 *         char label[64];
 *         snprintf(label, sizeof label, "Sélection (%d)", count);
 *         open_query_tab(g_tabpane, db, label, sql);
 *     }
 *
 * ════════════════════════════════════════════════════════════════════
 */
