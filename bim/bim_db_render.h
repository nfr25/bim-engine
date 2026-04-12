///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CV_DB_RENDER_H
#define CV_DB_RENDER_H
#ifdef __cplusplus
extern "C" {
#endif
/// rendering ////////////////////////////////////////////////////////////////////////////////////////////

void bim_db_render_blobs(BimDB *db, double vMinX, double vMaxX, double vMinY, double vMaxY, 
                         BimBlobRenderCallback render_cb, void *user_data) {
    
    // 1. On parcourt le catalogue des calques

    const char *meta_sql = "SELECT l.name, l.geom_type, "
    "s.stroke_color, "
    "s.stroke_width, "
    "s.fill_color, "
    "s.line_type FROM "
    "bim_layers l JOIN bim_styles s ON l.style_id = s.id WHERE geom_type > 0 AND l.is_visible = 1 ORDER BY l.id;";
    sqlite3_stmt *st_meta;

    if (sqlite3_prepare_v2(db->handle, meta_sql, -1, &st_meta, NULL) != SQLITE_OK) 
    { 
      fprintf(stderr, "Erreur SQL : %s\n", sqlite3_errmsg(db->handle));  
      return;
    }
    while (sqlite3_step(st_meta) == SQLITE_ROW) {
        BimStyle style;
        const char *table_name = (const char*)sqlite3_column_text(st_meta, 0);
        int geom_type          = sqlite3_column_int(st_meta, 1);
        style.stroke_color     = sqlite3_column_int(st_meta, 2);
        style.stroke_width     = sqlite3_column_int(st_meta, 3);
        style.fill_color       = sqlite3_column_int(st_meta, 4);
        style.line_type        = sqlite3_column_int(st_meta, 5);
        // 2. Requête dynamique pointant sur l'INDEX DÉDIÉ (idx_table_name)
        char data_sql[1024];
        snprintf(data_sql, sizeof(data_sql), 
            "SELECT t.geom FROM %s t "
            "WHERE t.id IN ("
            "  SELECT id FROM idx_%s " // <-- Ici on utilise l'index spécifique
            "  WHERE minX <= %f AND maxX >= %f "
            "  AND minY <= %f AND maxY >= %f"
            ");", 
            table_name, table_name, vMaxX, vMinX, vMaxY, vMinY);
 
        sqlite3_stmt *st_data;
        
        if (sqlite3_prepare_v2(db->handle, data_sql, -1, &st_data, NULL) == SQLITE_OK) {
            while (sqlite3_step(st_data) == SQLITE_ROW) {
                const void *blob = sqlite3_column_blob(st_data, 0);
                int size = sqlite3_column_bytes(st_data, 0);
                
                // 3. ENVOI AU CALLBACK Cairo
                render_cb(user_data, blob, size, geom_type, &style);
            }
            sqlite3_finalize(st_data);
            render_cb(user_data, NULL, 0, geom_type, &style);
        } else {
            fprintf(stderr, "Erreur SQL data: %s\n", sqlite3_errmsg(db->handle));
        }
    }
    sqlite3_finalize(st_meta);
}

static void bim_db_render_viewport(BimDB *db, 
                                   float vMinX, float vMaxX, 
                                   float vMinY, float vMaxY,
                                   bool highlight,
                                   BimObjRenderCallBack draw, 
                                   void *user_data) {
    BimStyle bs;
    sqlite3_stmt* stmt = highlight ? db->stmt_highlight : db->stmt_paint;
    if (!stmt) return;

    sqlite3_reset(stmt);
    // Bind des paramètres (Assure-toi que l'ordre correspond à ton SQL)
    sqlite3_bind_double(stmt, 1, vMaxX); // si.minX <= MaxX
    sqlite3_bind_double(stmt, 2, vMinX); // si.maxX >= MinX
    sqlite3_bind_double(stmt, 3, vMaxY); // si.minY <= MaxY
    sqlite3_bind_double(stmt, 4, vMinY); // si.maxY >= MinY

    sqlite3_int64 current_id = -1;
    BimObject obj;
    memset(&obj, 0, sizeof(BimObject)); // Reset complet de l'objet de transfert

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        sqlite3_int64 id = sqlite3_column_int64(stmt, 0);

        // --- CAS 1 : NOUVEL OBJET DÉTECTÉ ---
        if (id != current_id) {
            // Dessiner l'objet précédent s'il existait
            if (current_id != -1 && obj.poly.count > 0) {
                draw(1, &bs, highlight, &obj, user_data);
            }

            // Initialisation du nouvel objet
            current_id = id;

            obj.type = sqlite3_column_int(stmt, 1);
            obj.sx  = sqlite3_column_double(stmt, 2); // Ancrage pour NODE
            obj.sy  = sqlite3_column_double(stmt, 3);
            obj.poly.count = 0;
            
            // Attributs spécifiques aux NODES
            obj.symbol_id = sqlite3_column_int(stmt, 5); 
            obj.rotation  = sqlite3_column_double(stmt, 6);
            obj.scale     = sqlite3_column_double(stmt, 7);

            // Attributs de STYLE (COALESCE en SQL)
            bs.stroke_color = (unsigned int)sqlite3_column_int64(stmt, 8);
            bs.stroke_width = sqlite3_column_double(stmt, 9);
            bs.line_type    = sqlite3_column_int(stmt, 10);
        }

        // --- CAS 2 : ACCUMULATION DES POINTS (LIGNES / SURFACES / RINGS) ---
        if (obj.poly.count < MAX_POLY_PTS) {
            int idx = obj.poly.count;
            obj.poly.vertices[idx].x = (float)sqlite3_column_double(stmt, 2);
            obj.poly.vertices[idx].y = (float)sqlite3_column_double(stmt, 3);
            
            // Gestion vitale du part_id pour les polygones à trous (rings)
            obj.poly.part_ids[idx] = sqlite3_column_int(stmt, 4); 
            
            obj.poly.count++;
        }
    }

    // --- ÉTAPE FINALE : DESSINER LE DERNIER OBJET ---
    if (current_id != -1 && obj.poly.count > 0) {
        draw(1, &bs, highlight, &obj, user_data);
    }
}
#ifdef __cplusplus
}
#endif
#endif