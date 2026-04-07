///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CV_DB_TYPES_H
#define CV_DB_TYPES_H
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OBJ_NONE = 0,
    OBJ_LINE,
    OBJ_RECT,
    OBJ_CIRCLE,
    OBJ_ARC,
    OBJ_POLYLINE,
    OBJ_POLYGON,
    OBJ_NODE,
    ZOOM_WINDOW,
    SELECTION_POINT,
} ObjType;

typedef enum {
    ACT_ZOOM = 0,
    ACT_SELECT,
    ACT_CREATE
} Action;

#define MAX_POLY_PTS 4096
typedef struct {
    int x,y;
} ScreenPoint;

typedef struct {
  double x, y;
} BimPoint;

typedef struct {
    BimPoint min, max; // Bounding Box
} BimBbox;

typedef struct {
    BimPoint vertices[MAX_POLY_PTS];
    int  part_ids[MAX_POLY_PTS];
    int  count;
    bool is_closed;
} BimPoly;
///////////////////////////////////////////////////////////////////////////////////////////////
typedef struct {
    ObjType type;
    BimPoly poly;
    int symbol_id;
    double sx,sy;
    double scale;
    double rotation;    
} BimObject;

typedef struct {
   char *name;
   int  stroke_color;
   int  stroke_width;
   int  fill_color;
   int  line_type;
} BimStyle; 



///////////////////////////////////////////////////////////////////////////////////////////////
//comiled svg.
typedef struct {
    int key;              // symbol_id (la clé)
    RsvgHandle *handle;   // Le handle compilé
    double w, h;          // Dimensions pré-calculées
    char name[64];    
} SymbolCacheEntry;

///////////////////////////////////////////////////////////////////////////////////////////////
//layers
typedef struct {
    char name[64];
    int id;
    int visible; // 1 pour affiché, 0 pour masqué
    int geom_type;
    int style_id;
} BimLayerInfo;
///////////////////////////////////////////////////////////////////////////////////////////////
//sqlite interface
typedef struct {
    sqlite3 *handle;
    char *db_path;
    bool is_open;
    //stmt cache
    sqlite3_stmt 
    *stmt_dist_cache,
    *stmt_inside_cache, 
    *stmt_paint, 
    *stmt_highlight,
    *stmt_edit;
    SymbolCacheEntry *symbol_cache; 
} BimDB;

typedef void (*BimBlobRenderCallback)(void *user_data, const void *blob, int size, int type, BimStyle *style);
typedef int  (*BimObjRenderCallBack)(int layer, BimStyle *style, bool highlight, BimObject *obj, void* user_data);
typedef int  (*BimSQLCallback)(void *data, int argc, char **argv, char **azColName);
///////////////////////////////////////////////////////////////////////////////////////////////
typedef enum {
    EV_MOUSEMOVE = 0,
    EV_MODE,
    EV_LAYERS,
    EV_CUR_LAYER
} EVENTS;

typedef int (*fptr_t)(int, char *, void *);
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* ── Handle de vertex éditable ───────────────────────────────────── */

typedef struct {
    int    vertex_id;
    double x, y;          /* coordonnées monde                        */
    int    is_node;       /* 1 = nœud BIM, 0 = vertex intermédiaire  */
    int    entity_id;     /* entité parente                           */
    int    seq_order;     /* position dans la séquence                */
} BimEditHandle;

typedef struct {
    HWND     hwnd;
    void    *cairo_menu;
    float    w,h;           // taille du canvas
    // --- Matrice de Vue (Caméra) ---
    float    zoom;           // Échelle (1.0 = 100%)
    BimPoint offset;
    bool     grid;
    bool     snap;         //on grid
    // --- États Souris & Interaction ---
    bool  is_dragging;        // Vrai si on est en train de faire un PAN (clic molette/droit)
    ScreenPoint last_mouse;   // Dernière position curseur (pour le calcul du delta Pan)

    // --- Système d'Outils ---
    ObjType current_tool;  // L'outil de création sélectionné 
    bool is_interacting;  // Vrai si on est en cours de création d'un objet
    int   current_layer;
    int   current_symbol;    
    // Position "Monde" de la souris (après Snap)
    BimPoint mouseW, start;
    int      mx,my; //mouse pos in screen coord
    bool     picked; //a vertices is picked while dragging
    // L'objet "Fantôme" (celui qu'on dessine en ce moment)
    BimObject temp_obj;   
    bool      refit;     //recompute bounding box.
    BimDB     db;
    void      *user_data;
    fptr_t    call_back; //gui feedback call_back
    //caching
    bool    cache_valid;
    HDC     hdcCache;
    HBITMAP hbmCache;
    //edition
    int           edit_active;
    BimEditHandle edit_handles[MAX_POLY_PTS];
    int           edit_handle_count;
    int           dragging_handle;    /* index dans edit_handles, -1 = aucun */
} GuiCanvas;

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define CHECK_SQL(rc, db) \
    if ((rc) != SQLITE_OK && (rc) != SQLITE_DONE && (rc) != SQLITE_ROW) { \
        fprintf(stderr, "SQL Error at %s:%d : %s\n", __FILE__, __LINE__, sqlite3_errmsg(db->handle)); \
    }

#define SWAP(T, a, b) do { T _tmp = (a); (a) = (b); (b) = _tmp; } while(0)


#ifdef __cplusplus
}
#endif
#endif