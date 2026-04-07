///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CV_DB_H
#define CV_DB_H
#ifdef __cplusplus
extern "C" {
#endif

int bim_db_raw_select(BimDB *db, const char *sql, BimSQLCallback callback, void *data) {
    char *zErrMsg = 0;
    int rc = sqlite3_exec(db->handle, sql, callback, data, &zErrMsg);
    if (rc != SQLITE_OK) {
        callback(NULL,1,&zErrMsg, NULL);        
        sqlite3_free(zErrMsg);
        return 0;
    }
    return 1;
}

void bim_db_set_config(BimDB *db, const char *key, const char *value)
{
    const char *sql = "INSERT OR REPLACE INTO bim_config (key, value) VALUES (?, ?);";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key,   -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, value, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

const char *bim_db_get_config(BimDB *db, const char *key, const char *default_val)
{
    static char buf[256];
    const char *sql = "SELECT value FROM bim_config WHERE key = ?;";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, key, -1, SQLITE_STATIC);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *val = (const char*)sqlite3_column_text(stmt, 0);
            if (val) strncpy(buf, val, 255);
            buf[255] = '\0';
            sqlite3_finalize(stmt);
            return buf;
        }
        sqlite3_finalize(stmt);
    }
    return default_val;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//start here 
static GuiCanvas * bim_init(fptr_t call_back, void *user_data) {
    GuiCanvas* cv = (GuiCanvas*)malloc(sizeof(GuiCanvas));
    if (!cv) return NULL;

    // Mise à zéro sécurisée
    memset(cv, 0, sizeof(GuiCanvas));
    cv->w    = 0.0f;
    cv->h    = 0.0f;
    cv->zoom = 1.0f;
    cv->grid = false;
    cv->offset.x = 0.0f;
    cv->offset.y = 0.0f;
    cv->is_dragging = false;
    cv->last_mouse.x = 0;
    cv->last_mouse.y = 0;
    cv->is_interacting = false;
    cv->current_tool = SELECTION_POINT;
    cv->current_symbol = 1;
    cv->refit = true;
    cv->user_data = user_data;
    cv->call_back = call_back; 
    cv->current_layer = 0;
    cv->db.symbol_cache = NULL;
    cv->hdcCache = NULL;
    cv->cache_valid = false;
    cv->hbmCache = NULL;
    cv->edit_active = 0;
    cv->edit_handle_count =0;
    cv->dragging_handle = -1;  
    bim_db_open(&(cv->db), "project.bim"); 
    if (bim_db_get_library_count(&(cv->db)) <= 0)
        bim_import_svg_directory(&(cv->db), "./assets");
    bim_db_compile_svg_library(&(cv->db));    
    return cv;              
}

static void bim_close(GuiCanvas *cv) {
    if (cv) {
       bim_db_cleanup_svg_cache(&cv->db);
       bim_db_close(&(cv->db));        
       free(cv);
    }    
}

#ifdef __cplusplus
}
#endif
#endif