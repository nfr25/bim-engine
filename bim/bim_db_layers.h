///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef DB_LAYERS_H
#define DB_LAYERS_H
#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//layers
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
BimLayerInfo* bim_db_fetch_layers(BimDB *db, int *out_count) {
    *out_count = 0;
    const char *sql = "SELECT id, name, is_visible, geom_type, style_id FROM bim_layers ORDER BY id;";
    sqlite3_stmt *stmt;
    
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Erreur SQL (Fetch Layers): %s\n", sqlite3_errmsg(db->handle));
        return NULL;
    }

    // On alloue un bloc raisonnable au début
    int capacity = 16;
    BimLayerInfo *list = malloc(sizeof(BimLayerInfo) * capacity);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (*out_count >= capacity) {
            capacity *= 2;
            list = realloc(list, sizeof(BimLayerInfo) * capacity);
        }

        BimLayerInfo *layer = &list[*out_count];
        layer->id = sqlite3_column_int(stmt, 0);
        
        const char *raw_name = (const char*)sqlite3_column_text(stmt, 1);
        strncpy(layer->name, raw_name ? raw_name : "layer_unknown", 63);
        layer->name[63]  = '\0'; // Sécurité string
        layer->visible   = sqlite3_column_int(stmt, 2);
        layer->geom_type = sqlite3_column_int(stmt, 3);
        layer->style_id  = sqlite3_column_int(stmt, 4);
        (*out_count)++;
    }

    sqlite3_finalize(stmt);
    return list;
}

int bim_db_get_layers_count(BimDB *db) {
    const char *sql = "SELECT COUNT(*) FROM bim_layers;";
    sqlite3_stmt *stmt;
    int count = 0;

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            count = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return count;
}

int bim_db_get_layer_visible(BimDB *db, int id) {
    const char *sql = "SELECT is_visible FROM bim_layers WHERE id = ?;";
    sqlite3_stmt *stmt;
    int visible = 0;

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            visible = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return visible;
}

int bim_db_get_layer_style_id(BimDB *db, int id) {
    const char *sql = "SELECT style_id FROM bim_layers WHERE id = ?;";
    sqlite3_stmt *stmt;
    int style_id = 0;

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            style_id = sqlite3_column_int(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }
    return style_id;
}

void bim_db_set_layer_visible(BimDB *db, int id, int visible) {
    const char *sql = "UPDATE bim_layers SET is_visible = ? WHERE id = ?;";
    sqlite3_stmt *stmt;
    printf("id %d, visible %d\n",id, visible);
    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur Prepare: %s\n", sqlite3_errmsg(db->handle));
        return;
    }
    sqlite3_bind_int(stmt, 1, visible);
    sqlite3_bind_int(stmt, 2, id);
    if (sqlite3_step(stmt) != SQLITE_DONE) {
        fprintf(stderr, "Erreur Step: %s\n", sqlite3_errmsg(db->handle));
    } else {
        printf("Layer %d visibilité -> %d\n", id, visible);
    }        
    sqlite3_finalize(stmt);

}

void bim_db_get_layer_name(BimDB *db, int id, char *out_name, int max_len) {
    const char *sql = "SELECT name FROM bim_layers WHERE id = ?;";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *name = (const char*)sqlite3_column_text(stmt, 0);
            strncpy(out_name, name ? name : "", max_len - 1);
            out_name[max_len - 1] = '\0';
        }
        sqlite3_finalize(stmt);
    }
}

int bim_db_add_layer(BimDB *db, const char *name) {
    char sql[1024];
    // 1. create a new_style
    sqlite3_exec(db->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    snprintf(sql, sizeof(sql), 
        "INSERT INTO bim_styles (name) VALUES ('%s');", 
        name);
    
    if (sqlite3_exec(db->handle, sql, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_exec(db->handle, "ROLLBACK;", NULL, NULL, NULL);
        return -1;
    }
    int new_id = (int)sqlite3_last_insert_rowid(db->handle);
    // 2. Enregistrement dans layer
    snprintf(sql, sizeof(sql), 
        "INSERT INTO bim_layers (name, z_order, is_visible, is_locked, style_id) VALUES ('%s', 0, 1, 0, %d);", 
        name, new_id);
    
    if (sqlite3_exec(db->handle, sql, NULL, NULL, NULL) != SQLITE_OK) {
        sqlite3_exec(db->handle, "ROLLBACK;", NULL, NULL, NULL);
        return -1;
    }
    new_id = (int)sqlite3_last_insert_rowid(db->handle);
    sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
    return new_id; 
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//style
static int bim_db_get_style(BimDB *db, int id, BimStyle *out_style) {
    const char *sql = "SELECT stroke_color, stroke_width, fill_color, line_type FROM bim_styles WHERE id = ?;";
    sqlite3_stmt *stmt;
    int found = 0;

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_style->stroke_color = sqlite3_column_int(stmt, 0);
            out_style->stroke_width = sqlite3_column_int(stmt, 1);
            out_style->fill_color   = sqlite3_column_int(stmt, 2);
            out_style->line_type    = sqlite3_column_int(stmt, 3);
            found = 1;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

static int bim_db_get_layer_style(BimDB *db, int id, BimStyle *out_style) {
    const char *sql = "SELECT s.stroke_color, s.stroke_width, s.fill_color, s.line_type FROM bim_styles s, bim_layers l WHERE l.style_id = s.id and l.id = ?;";
    sqlite3_stmt *stmt;
    int found = 0;

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            out_style->stroke_color = sqlite3_column_int(stmt, 0);
            out_style->stroke_width = sqlite3_column_int(stmt, 1);
            out_style->fill_color   = sqlite3_column_int(stmt, 2);
            out_style->line_type    = sqlite3_column_int(stmt, 3);
            found = 1;
        }
        sqlite3_finalize(stmt);
    }
    return found;
}


void bim_db_set_style(BimDB *db, int id, BimStyle *style) {static 
    const char *sql = "INSERT OR REPLACE INTO bim_styles (id, name, stroke_color, stroke_width, fill_color, line_type) "
                      "VALUES (?, 'style_name', ?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_int(stmt, 2, style->stroke_color);
        sqlite3_bind_int(stmt, 3, style->stroke_width);
        sqlite3_bind_int(stmt, 4, style->fill_color);
        sqlite3_bind_int(stmt, 5, style->line_type);
        
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

void bim_db_set_layer_style(BimDB *db, int id, BimStyle *style) { 
    const char *sql = "INSERT OR REPLACE INTO bim_styles (id, name,  stroke_color, stroke_width, fill_color, line_type) "
                      "VALUES ((SELECT style_id FROM bim_layers WHERE id = ?), "
                      "(SELECT name FROM bim_layers WHERE id = ?), "
                      " ?, ?, ?, ?);";
    sqlite3_stmt *stmt;

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, id);
        sqlite3_bind_int(stmt, 2, id);
        sqlite3_bind_int(stmt, 3, style->stroke_color);
        sqlite3_bind_int(stmt, 4, style->stroke_width);
        sqlite3_bind_int(stmt, 5, style->fill_color);
        sqlite3_bind_int(stmt, 6, style->line_type);
        
        int rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE)
            printf("sqlite3_step error: %s\n", sqlite3_errmsg(db->handle));
        sqlite3_finalize(stmt);
    }
}

#ifdef __cplusplus
}
#endif
#endif
