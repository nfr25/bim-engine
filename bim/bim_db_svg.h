///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef DB_SVG_H
#define DB_SVG_H
#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//chargement des SVG db.
//load svg symbols and compile them
static void bim_db_cleanup_svg_cache(BimDB *db) {
    if (db->symbol_cache != NULL) {
        for (int i = 0; i < hmlen(db->symbol_cache); ++i) {
            if (db->symbol_cache[i].handle) {
                g_object_unref(db->symbol_cache[i].handle);
            }
        }
        hmfree(db->symbol_cache);
        db->symbol_cache = NULL; // Crucial pour stb_ds
    }
}

static void bim_db_compile_svg_library(BimDB *db) {
    const char *sql = "SELECT id, svg_data, name FROM bim_svg_library;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) return;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *svg_text = (const char*)sqlite3_column_text(stmt, 1);
        size_t len = strlen(svg_text);
        const char *name     = (const char *) sqlite3_column_text(stmt,2);
        
        // Compilation du handle via librsvg
        GError *error = NULL;
        RsvgHandle *h = rsvg_handle_new_from_data((const guint8*)svg_text, len, &error);
        
        if (h) {
            // Récupération des dimensions une fois pour toutes
            RsvgLength ow, oh;
            gboolean hw, hh;
            gboolean has_vb; // Pour capturer si une viewBox existe
            rsvg_handle_get_intrinsic_dimensions(h, &hw, &ow, &hh, &oh, &has_vb, NULL);
            // Insertion dans le dictionnaire stb_ds
            SymbolCacheEntry entry;
            strcpy(entry.name, name);
            entry.key       = id;
            entry.handle    = h;
            entry.w         = ow.length;
            entry.h         = oh.length;
            hmputs(db->symbol_cache, entry); // 'hmputs' ajoute ou remplace par clé
            //printf("symbol %d correctement chargé\n", id);
        } else {
            fprintf(stderr, "SVG Error ID %d: %s\n", id, error->message);
            g_clear_error(&error);
        }
    }
    sqlite3_finalize(stmt);
}

static int bim_db_get_library_count(BimDB* db) {
    sqlite3_stmt* stmt;
    int count = 0;
    
    if (sqlite3_prepare_v2(db->handle, "SELECT COUNT(*) FROM bim_svg_library;", -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count;
}


// Fonction utilitaire pour lire un fichier complet en mémoire
static char* read_file_to_string(const char* filename) {
    FILE* f = fopen(filename, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* string = malloc(fsize + 1);
    fread(string, fsize, 1, f);
    fclose(f);
    string[fsize] = 0;
    return string;
}

static void bim_import_svg_directory(BimDB* db, const char* directory_path) {
    WIN32_FIND_DATA findData;
    char searchPath[MAX_PATH];
    sprintf(searchPath, "%s\\*.svg", directory_path);

    HANDLE hFind = FindFirstFile(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    sqlite3_exec(db->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    do {
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char fullPath[MAX_PATH];
            sprintf(fullPath, "%s\\%s", directory_path, findData.cFileName);

            char* xml_content = read_file_to_string(fullPath);
            if (xml_content) {
                // On retire l'extension .svg pour le nom dans la base
                char name[MAX_PATH];
                strcpy(name, findData.cFileName);
                char* dot = strrchr(name, '.');
                if (dot) *dot = '\0';

                sqlite3_stmt* stmt;
                const char* sql = "INSERT OR IGNORE INTO bim_svg_library (name, svg_data) VALUES (?, ?);";
                
                if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(stmt, 2, xml_content, -1, SQLITE_TRANSIENT);
                    sqlite3_step(stmt);
                    sqlite3_finalize(stmt);
                }
                free(xml_content);
            }
        }
    } while (FindNextFile(hFind, &findData));

    sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
    FindClose(hFind);
}

static SymbolCacheEntry * bim_db_get_symbol(BimDB *db, int symbol_id){
   return hmgetp_null(db->symbol_cache, symbol_id);    
}

#ifdef __cplusplus
}
#endif
#endif