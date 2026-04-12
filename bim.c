///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*
 * 
 *
 * Compilation :
 * dll cairo
 set PATH=C:\msys64\mingw64\bin;%PATH% 
 --recupérer toutes le dll utilisées.
 ldd canvas.exe | grep mingw64 | awk '{print $3}' | xargs -I{} cp {} .
 */
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <dwmapi.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdbool.h>
#include <ctype.h>
#include <libgen.h>
#include <sqlite3.h>
#include <librsvg/rsvg.h>
#define _USE_MATH_DEFINES
#include <math.h>
//cairo UI
#include "ui_backend.h"
#define CAIRO_TABPANE_IMPLEMENTATION
#include "cairo_tabpane.h"
#define BIM_LISTVIEW_IMPLEMENTATION
#include "ui/bim_listview.h"
#define CAIRO_MENU_IMPLEMENTATION
#include "cairo_menu.h"
#define CAIRO_TRANSCRIPT_IMPLEMENTATION
#include "cairo_transcript.h"
#define CAIRO_LAYOUT_IMPLEMENTATION
#include "cairo_layout.h"
#define CAIRO_STYLE_EDITOR_IMPLEMENTATION
#include "cairo_style_editor.h"
#define CAIRO_SYMBOL_PICKER_IMPLEMENTATION
#include "cairo_symbol_picker.h"
#define CAIRO_INPUT_DIALOG_IMPLEMENTATION
#include "cairo_input_dialog.h"
#include "cairo_icones.h"

#include "shapefil.h"
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
//bim itself
#include "bim_db_types.h"
#include "bim_db_extent.h"
#include "bim_db_schema.h"
#include "bim_db_svg.h"
#include "bim_db_layers.h"
#include "bim_db_entity.h"
#define BIM_DB_EDIT_IMPLEMENTATION
#include "bim_db_edit.h"
#include "bim_db_shape.h"
#include "bim_db_pick.h"
#include "bim_db_render.h"
#include "bim_db.h"
#include "bim_canvas.h"
 

#define IDI_APPICON 101
/* ── IDs boutons toolbar ─────────────────────────────────────────── */
#define TB_NOUVEAU   1
#define TB_OUVRIR    2
#define TB_ENREG     3
#define TB_ZOOM_P    4
#define TB_ZOOM_M    5
#define TB_GRILLE    6
#define TB_SNAP      7
#define TB_COMBO     8
#define TB_ADD       9
#define TB_DEL      10
#define TB_STYLE    11
#define TB_POLYLINE 12
#define TB_POLYGON  13
#define TB_NODE     14
#define TB_ZOOM_F   15
#define TB_ZOOM_R   16
#define TB_ZOOM     18
#define TB_TRANSCRIPT 17

/* ── IDs menu contextuel ─────────────────────────────────────────── */
#define MN_AJOUTER   10
#define MN_SUPPRIMER 11
#define MN_PROPRIETE 12
#define MN_GRILLE    20
#define MN_SNAP      21
#define MN_ZOOM_P    30
#define MN_ZOOM_M    31
#define MN_ZOOM_0    32
#define MN_ZOOM_F    33
/* ── État application ────────────────────────────────────────────── */
static CairoLayout *g_layout = NULL;
CairoMenu    *g_menu   = NULL;
static char   g_status0[64] = "Prêt";
static char   g_status2[64] = "X: —   Y: —";
static Style  g_style;
HWND g_canvas;
HWND parent = NULL;
/* ── Callback menu ───────────────────────────────────────────────── */
CspSymbol *build_picker_symbols(SymbolCacheEntry *cache) {
    int count = hmlen(cache);
    CspSymbol *out = calloc(count + 1, sizeof(CspSymbol));
    for (int i = 0; i < count; i++) {
        out[i].name       = cache[i].name;
        out[i].rsvg       = cache[i].handle;
        out[i].ports_mask = 0; //cache[i].ports_mask;
        out[i].width      = cache[i].w;
        out[i].height     = cache[i].h;
        out[i].key        = cache[i].key;
        out[i].port_max   = 1;
    }
    out[count].name = NULL;  /* sentinel */
    return out;
}

void svg_symbol(GuiCanvas *cv) {
  CspResult res = {-1, 0.0};
  static CspSymbol *syms = NULL;
  int i;
  if (syms == NULL)  // only create the table one time, never destroyed.
     syms = build_picker_symbols(cv->db.symbol_cache); 
  for (i = 0; i< hmlen(cv->db.symbol_cache); i++)
  { 
      //printf(" %d : [%d]\n",i,syms[i].key);  
      if (syms[i].key == cv->current_symbol)
      {
            res.symbol_idx = i;
            break;
      }      
  }          
  //printf("Before : current_symbol : %d res.key %d idx %d\n", cv->current_symbol, syms[i].key, i);            
  if (csp_pick(cv->hwnd, syms, &res))
  {
    cv->current_symbol =  syms[res.symbol_idx].key;
    //printf("After : current_symbol : %d\n", cv->current_symbol);
  }  
  //free(syms);  
}

int rgb_to_hex(double r, double g, double b) {
    int ri = (int)(r * 255.0 + 0.5);
    int gi = (int)(g * 255.0 + 0.5);
    int bi = (int)(b * 255.0 + 0.5);
    return (ri << 16) | (gi << 8) | bi;
}

void hex_to_rgb(int hex_color, double *r, double *g, double *b) {
    *r = ((hex_color >> 16) & 0xFF) / 255.0;
    *g = ((hex_color >>  8) & 0xFF) / 255.0;
    *b = ( hex_color        & 0xFF) / 255.0;
}

void style_edit(GuiCanvas *cv) {
    Style s;
    BimStyle bs;
    bim_db_get_layer_style(&(cv->db),cv->current_layer,&bs);
    s.stroke_width = bs.stroke_width;
    hex_to_rgb(bs.stroke_color, &s.stroke_r, &s.stroke_g, &s.stroke_b);
    hex_to_rgb(bs.fill_color,  &s.fill_r, &s.fill_g, &s.fill_b);
    s.stroke_style= bs.line_type;
    if (cse_edit(cv->hwnd, &s)) {
        //printf("saving layer style %d\n", cv->current_layer);
        bs.stroke_width = s.stroke_width;
        bs.stroke_color = rgb_to_hex(s.stroke_r, s.stroke_g, s.stroke_b);
        bs.fill_color   = rgb_to_hex(s.fill_r, s.fill_g, s.fill_b);
        bs.line_type    = s.stroke_style;
        bim_db_set_layer_style(&(cv->db), cv->current_layer, &bs);
        cv->cache_valid = false;
        canvas_refresh(cv);
        }  
}

static void on_menu(int id, void *ud)
{
    HWND canvas = (HWND) ud;
    GuiCanvas *cv = gui_get_canvas_data(canvas);

    switch (id) {
    case MN_GRILLE:
        cv->grid = !cv->grid;
        cm_set_checked(g_menu, MN_GRILLE, cv->grid);
        cl_toolbar_set_pressed(g_layout, TB_GRILLE, cv->grid);
        snprintf(g_status0, sizeof g_status0,
                 "Grille %s", cv->grid ? "activée" : "désactivée");
        cv->cache_valid = false;
        canvas_refresh(cv);
        break;
    case MN_SNAP:
        cv->snap = !cv->snap;
        cm_set_checked(g_menu, MN_SNAP, cv->snap);
        cl_toolbar_set_pressed(g_layout, TB_SNAP, cv->snap);
        snprintf(g_status0, sizeof g_status0,
                 "Magnétisme %s", cv->snap ? "activé" : "désactivé");
        break;
    case MN_ZOOM_P: 
         canvas_zoom_plus(cv);
         break;
    case MN_ZOOM_M: 
         canvas_zoom_minus(cv);
         break;
    case MN_ZOOM_F: 
         cv->refit = true;
         canvas_refresh(cv);
         break;         
    }
    cl_status_set(g_layout, 0, g_status0);
    cl_status_set(g_layout, 2, g_status2);
    InvalidateRect(canvas, NULL, FALSE);
}

/* ── Callback toolbar ────────────────────────────────────────────── */
static void on_toolbar(int id, void *ud)
{
    HWND canvas = (HWND) ud;
    GuiCanvas *cv = gui_get_canvas_data(canvas);
    if (id >= CL_ID_EYE_BASE) {  
        int layer = id - CL_ID_EYE_BASE;
        int vis   = cl_combo_get_visible(g_layout, TB_COMBO, layer);
        bim_db_set_layer_visible(&(cv->db), layer, vis );
        cv->cache_valid = false;
        canvas_refresh(cv);
        return;           
    }    
    /* Réutilise la logique menu pour les actions communes */
    switch (id) {
    case TB_COMBO :   
    int idx = cl_combo_get_current(g_layout, TB_COMBO);
    canvas_set_current_layer(cv, idx , (char*)cl_combo_get_name(g_layout, TB_COMBO, idx));
    int geom_type = cl_combo_get_geom_type(g_layout, TB_COMBO, idx); 
    //printf("geom type : %d\n", geom_type);
    cl_toolbar_set_enabled(g_layout, TB_POLYLINE, geom_type == 0);
    cl_toolbar_set_enabled(g_layout, TB_POLYGON,  geom_type == 0);
    cl_toolbar_set_enabled(g_layout, TB_NODE,     geom_type == 0);
    InvalidateRect(g_layout->hwnd_toolbar, NULL, FALSE);
    break; 
    case TB_GRILLE:   on_menu(MN_GRILLE, ud); break;
    case TB_SNAP:     on_menu(MN_SNAP,   ud); break;
    case TB_ZOOM_P:   on_menu(MN_ZOOM_P, ud); break;
    case TB_ZOOM:   
        {
        int mode = cl_toolbar_get_mode(g_layout, TB_ZOOM);
        switch (mode) {
        case 0: canvas_zoom_extent(cv);           break;
        case 1: canvas_set_mode(cv, ZOOM_WINDOW, false); break;
        case 2: canvas_zoom_selection(cv);        break;
         }
        } 
    break;            
    case TB_ZOOM_M:   on_menu(MN_ZOOM_M, ud); break;
    case TB_POLYLINE : canvas_set_mode(cv, OBJ_POLYLINE, true); break;
    case TB_POLYGON  : canvas_set_mode(cv, OBJ_POLYGON, true);  break;
    case TB_NODE     : canvas_set_mode(cv, OBJ_NODE, true);     break;
    case CL_ID_SPLIT_BASE + TB_NODE :  svg_symbol(cv);    break;
    case TB_STYLE    : style_edit(cv); break;
    case TB_ADD :
        {
            char name[64] = "";
            if (cl_input_dialog(cv->hwnd, "Nouveau layer", "Nom :", name, sizeof name)) {
                bim_db_add_layer(&(cv->db), name);
                cv->call_back(EV_LAYERS, NULL, cv);
            }    
        } 
        break;     
    case TB_TRANSCRIPT:
        cl_toggle_transcript(g_layout);
        break;    
    }
}

/* ── Construction menu contextuel ───────────────────────────────── */
static void * build_menu(void *ud)
{
    g_menu = cm_create(g_canvas, on_menu, ud);

    cm_add_item (g_menu,  MN_ZOOM_P, "Zoom avant",    "Ctrl++", CM_FLAG_NONE);
    cm_add_item (g_menu,  MN_ZOOM_M, "Zoom arrière",  "Ctrl+-", CM_FLAG_NONE);
    cm_add_item (g_menu,  MN_ZOOM_0, "Réinitialiser", "Ctrl+0", CM_FLAG_NONE);
    cm_add_sep  (g_menu);
    cm_add_check(g_menu,  MN_GRILLE, "Grille",        NULL, CM_FLAG_NONE, 0);
    cm_add_check(g_menu,  MN_SNAP,   "Magnétisme",    NULL, CM_FLAG_NONE, 1);
    return g_menu;
}

/* ── WndProc parent ──────────────────────────────────────────────── */
static LRESULT CALLBACK ParentProc(HWND hwnd, UINT msg,
                                    WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_SIZE:
        cl_resize(g_layout);
        return 0;
    case WM_CTLCOLOREDIT:
        return ct_on_ctlcolor(g_layout->transcript, (HDC)wp, (HWND)lp);        
    case WM_DESTROY:
        cm_destroy(g_menu);
        cl_destroy(g_layout);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ── Info du canvas ─────────────────────────────────────
int call_back(int event, char * text, void * user) 
{   GuiCanvas *cv = (GuiCanvas *)user;
    switch(event)
    {
        case EV_MOUSEMOVE :
            cl_status_set(g_layout, 2, text);        
            break;
        case EV_MODE :  
            cl_status_set(g_layout, 1, text);    
            break;  
        case EV_LAYERS :
            {
            int count = 0;    
            cl_combo_clear(g_layout, TB_COMBO);
            BimLayerInfo* info = bim_db_fetch_layers(&(cv->db),&count);
            for (int i = 0;i < count; i++) {
                cl_combo_add_layer  (g_layout, TB_COMBO, info[i].name, info[i].visible);
                cl_combo_set_layer_data(g_layout, TB_COMBO, i, 0, info[i].geom_type, info[i].style_id);
            }    
            free(info);
            InvalidateRect(g_layout->hwnd_toolbar, NULL, FALSE);
            }
            break; 
        case EV_CUR_LAYER :
                cl_status_set(g_layout, 0, text);   
            break;                
    }
}

typedef struct {
    int header_printed;
    int row_count;
} TranscriptCtx;

int bim_transcript_formatter(void *data, int argc, char **argv, char **azColName) {
    if (data == NULL)
    {
       ct_add(g_layout->transcript, CT_ERR, argv[0]);  
       return 0;
    }
    TranscriptCtx *ctx = (TranscriptCtx *)data;
    char buf[255]  ="\0";
    char buf2[1024]="\0";

    // 1. Imprimer l'en-tête (noms des colonnes) seulement à la première ligne
    if (!ctx->header_printed) {
        for (int i = 0; i < argc; i++) {
            sprintf(buf, "%-15s", azColName[i]); // Largeur fixe de 15 caractères
            strcat(buf2, "|");
            strcat(buf2, buf);   
        }
        
        ct_add(g_layout->transcript, CT_OK, buf2);
        ct_add(g_layout->transcript, CT_SEP, "");            // ── ligne fine ──
        buf2[0] = '\0';
        ctx->header_printed = 1;
    }

    // 2. Imprimer les données de la ligne
    for (int i = 0; i < argc; i++) {
        sprintf(buf,"%-15s", argv[i] ? argv[i] : "NULL");
        strcat(buf2, "|");
        strcat(buf2, buf);   
    }
    ct_add(g_layout->transcript, CT_OK, buf2);

    ctx->row_count++;

    return 0; // 0 pour continuer vers la ligne suivante
}

void on_command(const char *text, void *userdata) {
   TranscriptCtx tc = {0,0};
   GuiCanvas *cv = gui_get_canvas_data(g_canvas);
   ct_add(g_layout->transcript, CT_CMD, text);
   bim_db_raw_select(&(cv->db), text, bim_transcript_formatter,&tc);
}

static void on_tab_close(int idx, HWND content, void *ud)
{
    (void)ud;
    if (!content) return;
    BimListView *lv = ui_get_data(BimListView, content);
    if (lv) blv_destroy(lv);   /* blv_destroy détruit le HWND */
}

/* ── WinMain ─────────────────────────────────────────────────────── */
int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR lp, int ns)
{
    (void)hp; (void)lp;

    /* Enregistrement classes */
    WNDCLASSEX wc = {0};
    wc.cbSize        = sizeof wc;
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.hInstance     = hi;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);

    wc.lpfnWndProc   = ParentProc;
    wc.lpszClassName = "CL_Parent";
    RegisterClassEx(&wc);
    /* Fenêtre parent */
    parent = CreateWindowEx(0, "CL_Parent",
        "Topological Demo",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 1024,
        NULL, NULL, hi, NULL);
    HICON hIcon = LoadIcon(hi, MAKEINTRESOURCE(IDI_APPICON));
    SendMessage(parent, WM_SETICON, ICON_BIG,   (LPARAM)hIcon);
    SendMessage(parent, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
    BOOL dark = TRUE;
    DwmSetWindowAttribute(parent, 20, &dark, sizeof dark);

    /* Canvas enfant  */
    g_canvas = CreateCanvasWindow(hp, parent, call_back, NULL); 
    CairoTranscript *ct = ct_create(parent, 0, 0, 1, 1); // position provisoire
    ct_set_cb(ct, on_command, NULL);      
    /* Layout + toolbar + statusbar */
    g_layout = cl_create(parent, g_canvas);
    cl_attach_tabpane(g_layout, ct, 220, on_tab_close, NULL);
    //cl_attach_transcript(g_layout, ct, 220);  // 220px initial

    cl_toolbar_add_sep(g_layout);
    cl_toolbar_add_icon(g_layout, TB_ZOOM_P,  icon_zoom_plus,  "Zoom plus" ,     CL_BTN_NORMAL);
    cl_toolbar_add_icon_multimode(g_layout, TB_ZOOM,  icon_zoom, "Zoom",        3);
    cl_toolbar_add_icon(g_layout, TB_ZOOM_M,  icon_zoom_moins, "Zoom moins",    CL_BTN_NORMAL);
    cl_toolbar_add_sep(g_layout);
    //cl_toolbar_add_btn(g_layout, TB_GRILLE,  "Gril",  CL_BTN_TOGGLE);
    //cl_toolbar_add_btn(g_layout, TB_SNAP,    "Snap",  CL_BTN_TOGGLE);
    cl_toolbar_add_icon(g_layout, TB_GRILLE,  icon_grid,  "Grid" ,     CL_BTN_TOGGLE);
    cl_toolbar_add_icon(g_layout, TB_SNAP,    icon_snap, "Snap",       CL_BTN_TOGGLE);


    cl_toolbar_set_pressed(g_layout, TB_SNAP, 0);  /* snap actif par défaut */
    cl_toolbar_add_sep (g_layout);
    cl_toolbar_add_icon(g_layout, TB_POLYLINE, icon_polyline, "Polyligne", CL_BTN_NORMAL);
    cl_toolbar_add_icon(g_layout, TB_POLYGON,  icon_polygon,  "Surface",   CL_BTN_NORMAL);
     //cl_toolbar_add_btn (layout, ID_PLACE, "⬡ Node", CL_BTN_SPLIT);
    cl_toolbar_add_icon(g_layout, TB_NODE,     icon_node,  "Node",         CL_BTN_SPLIT);
    cl_toolbar_add_sep (g_layout);
 
    // Combo layer
    cl_toolbar_add_combo(g_layout, TB_COMBO,  160);
    cl_toolbar_add_icon(g_layout, TB_ADD, icon_add_layer, "Add Layer", CL_BTN_NORMAL);
    cl_toolbar_add_icon(g_layout, TB_DEL, icon_del_layer, "Del Layer", CL_BTN_NORMAL);
    cl_toolbar_add_icon(g_layout, TB_STYLE, icon_edit_style, "Edit Style", CL_BTN_NORMAL);
    cl_toolbar_add_icon(g_layout, TB_TRANSCRIPT,  icon_grid,  "Transcript" ,     CL_BTN_TOGGLE);     
   
    cl_set_toolbar_cb(g_layout, on_toolbar, g_canvas);

    cl_status_add_pane(g_layout, CL_PANE_FIXED,  160);  /* message          */
    cl_status_add_pane(g_layout, CL_PANE_SPRING,   0);  /* espace libre     */
    cl_status_add_pane(g_layout, CL_PANE_FIXED,  200);  /* coordonnées      */
    cl_status_set(g_layout, 0, "Prêt");
    cl_status_set(g_layout, 2, "X: —   Y: —");

    /* Menu contextuel */
   
    GuiCanvas *cv  = gui_get_canvas_data(g_canvas);
    cv->call_back(EV_LAYERS, NULL, cv);  
    cv->cairo_menu =  build_menu(g_canvas);
    cl_resize(g_layout);
    ShowWindow(parent, ns);
    UpdateWindow(parent);

    MSG m;
    while (GetMessage(&m, NULL, 0, 0)) {
        TranslateMessage(&m);
        DispatchMessage(&m);
    }
    return (int)m.wParam;
}
