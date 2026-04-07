///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CV_DB_SHAPE_H
#define CV_DB_SHAPE_H
#ifdef __cplusplus
extern "C" {
#endif

//chargement shape en topologique, trop lent au rendu dès que le fichier devient lourd.
void bim_db_import_shapefile(BimDB *db, const char *path) {
    SHPHandle hSHP = SHPOpen(path, "rb");
    if (!hSHP) return;

    int nEntities, nShapeType;
    double adfMin[4], adfMax[4];
    SHPGetInfo(hSHP, &nEntities, &nShapeType, adfMin, adfMax);

    sqlite3_exec(db->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    for (int i = 0; i < nEntities; i++) {
        SHPObject *psShape = SHPReadObject(hSHP, i);
        // 1. Créer le header (BBox fournie par Shapelib)
        sqlite3_int64 ent_id = bim_db_insert_entity_header(db, psShape->nSHPType, 
                                psShape->dfXMin, psShape->dfXMax, 
                                psShape->dfYMin, psShape->dfYMax);

        // 2. Parcourir les points
        int current_part = 0;
        for (int j = 0; j < psShape->nVertices; j++) {
            // Logique de Part
            if (current_part + 1 < psShape->nParts && psShape->panPartStart[current_part+1] == j) 
                current_part++;

            // Est-ce un noeud ? (Début ou fin de part)
            int is_node = 0;
            if (j == psShape->panPartStart[current_part] || 
                (current_part + 1 < psShape->nParts && j == psShape->panPartStart[current_part+1] - 1) ||
                (j == psShape->nVertices - 1)) {
                is_node = 1;
            }

            bim_db_link_vertex(db, ent_id, psShape->padfX[j], psShape->padfY[j], current_part, j, is_node);
        }
        SHPDestroyObject(psShape);
    }

    sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
    SHPClose(hSHP);
}

//nouvelle tentative en blob layer/////////////////////////////////////////////////////////////////////////////////////////////
static void bind_dbf_field(sqlite3_stmt *stmt, int sqlite_col_idx, DBFHandle hDBF, int shape_idx, int field_idx) {
    char field_name[12];
    DBFFieldType type = DBFGetFieldInfo(hDBF, field_idx, field_name, NULL, NULL);

    if (DBFIsAttributeNULL(hDBF, shape_idx, field_idx)) {
        sqlite3_bind_null(stmt, sqlite_col_idx);
        return;
    }

    switch (type) {
        case FTDouble:
            sqlite3_bind_double(stmt, sqlite_col_idx, DBFReadDoubleAttribute(hDBF, shape_idx, field_idx));
            break;
        case FTInteger:
            sqlite3_bind_int(stmt, sqlite_col_idx, DBFReadIntegerAttribute(hDBF, shape_idx, field_idx));
            break;
        case FTString:
        default:
            // On utilise SQLITE_TRANSIENT car le pointeur renvoyé par DBFReadString peut changer
            sqlite3_bind_text(stmt, sqlite_col_idx, DBFReadStringAttribute(hDBF, shape_idx, field_idx), -1, SQLITE_TRANSIENT);
            break;
    }
}

static sqlite3_stmt* build_insert_stmt(BimDB *db, const char *table_name, int nFields) {
    char sql[4096];
    // On commence avec "geom" (le BLOB) qui est toujours notre première colonne après l'ID
    snprintf(sql, sizeof(sql), "INSERT INTO %s (geom", table_name);
    
    // On ajoute les noms des colonnes (il faudra peut-être les relire du DBF ou passer la liste)
    // Mais pour faire simple et générique, on peut aussi lister uniquement les valeurs :
    // INSERT INTO table VALUES (NULL, ?, ?, ?, ...)
    
    char placeholders[1024] = "?"; // Le premier '?' est pour le BLOB
    for (int i = 0; i < nFields; i++) {
        strcat(placeholders, ", ?");
    }

    snprintf(sql, sizeof(sql), "INSERT INTO %s VALUES (NULL, %s);", table_name, placeholders);
    
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    return stmt;
}

static void extract_table_name(const char *path, char *dest) {
    // 1. On récupère le nom du fichier seul (ex: "/maps/mon-fond.shp" -> "mon-fond.shp")
    char *base = basename((char*)path);
    
    // 2. On copie en ignorant l'extension .shp
    strncpy(dest, base, 127);
    char *dot = strrchr(dest, '.');
    if (dot) *dot = '\0';

    // 3. On nettoie pour SQLite (caracteres alphanumériques et _ uniquement)
    for (int i = 0; dest[i]; i++) {
        if (!isalnum((unsigned char)dest[i])) {
            dest[i] = '_';
        } else {
            dest[i] = tolower((unsigned char)dest[i]); // Plus propre en minuscules
        }
    }
}

static int create_default_style(sqlite3 *db, const char* layer_name, int geom_type) {
    const char *sql = "INSERT INTO bim_styles (name, stroke_color, stroke_width, fill_color) VALUES (?, ?, ?, ?);";
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db, sql, -1, &st, NULL);

    // Un peu de couleur pour fêter ça !
    // Format 0xRRGGBB (ou ce que ton moteur préfère)
    int color = (geom_type == 5) ? 0x2ECC71 : 0x3498DB; // Vert pour poly, Bleu pour lignes
    
    sqlite3_bind_text(st, 1, layer_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(st, 2, color); // stroke
    sqlite3_bind_int(st, 3, 1);     // width
    sqlite3_bind_int(st, 4, 0xFFFFFF); // fill

    sqlite3_step(st);
    int style_id = (int)sqlite3_last_insert_rowid(db);
    sqlite3_finalize(st);
    return style_id;
}

int bim_db_import_blob_layer(BimDB *db, const char *shp_path,  int level_id) {
    char table_name[128];
    SHPHandle hSHP = SHPOpen(shp_path, "rb");
    if (!hSHP) return -1;
    extract_table_name(shp_path, table_name);
    int nEntities;
    int nShapeType;
    double adfMinBound[4], adfMaxBound[4];
    SHPGetInfo(hSHP, &nEntities, &nShapeType, adfMinBound, adfMaxBound);

    // 1. CRÉATION DE LA TABLE DE DONNÉES (BLOBs)
    char sql_data[512];
    snprintf(sql_data, sizeof(sql_data), 
             "CREATE TABLE IF NOT EXISTS %s (id INTEGER PRIMARY KEY, geom BLOB);", 
             table_name);
    sqlite3_exec(db->handle, sql_data, NULL, NULL, NULL);

    // 2. CRÉATION DE LA TABLE D'INDEX SPATIAL (R-TREE) DÉDIÉE
    char sql_idx_create[512];
    snprintf(sql_idx_create, sizeof(sql_idx_create), 
             "CREATE VIRTUAL TABLE IF NOT EXISTS idx_%s USING rtree(id, minX, maxX, minY, maxY);", 
             table_name);
    sqlite3_exec(db->handle, sql_idx_create, NULL, NULL, NULL);

    // 3. PRÉPARATION DES STATEMENTS
    sqlite3_stmt *st_data, *st_idx;
    char sql_ins_data[256], sql_ins_idx[256];
    
    snprintf(sql_ins_data, sizeof(sql_ins_data), "INSERT INTO %s (geom) VALUES (?);", table_name);
    snprintf(sql_ins_idx, sizeof(sql_ins_idx), "INSERT INTO idx_%s (id, minX, maxX, minY, maxY) VALUES (?, ?, ?, ?, ?);", table_name);

    sqlite3_prepare_v2(db->handle, sql_ins_data, -1, &st_data, NULL);
    sqlite3_prepare_v2(db->handle, sql_ins_idx, -1, &st_idx, NULL);

    // 4. TRANSACTION POUR LA PERFORMANCE
    sqlite3_exec(db->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    for (int i = 0; i < nEntities; i++) {
        SHPObject *psShape = SHPReadObject(hSHP, i);
        if (!psShape) continue;

        // --- A. Insertion du BLOB ---
        // On crée notre format custom : [int nVertices][double x1, y1, x2, y2...]
        int nVertices = psShape->nVertices;
        int blob_size = sizeof(int) + (nVertices * 2 * sizeof(double));
        unsigned char *buffer = malloc(blob_size);
        
        memcpy(buffer, &nVertices, sizeof(int));
        double *coords = (double*)(buffer + sizeof(int));
        for (int v = 0; v < nVertices; v++) {
            coords[v*2]   = psShape->padfX[v];
            coords[v*2+1] = psShape->padfY[v];
        }

        sqlite3_bind_blob(st_data, 1, buffer, blob_size, free);
        sqlite3_step(st_data);
        
        // Récupération de l'ID généré (rowid) pour le lier à l'index
        sqlite3_int64 last_id = sqlite3_last_insert_rowid(db->handle);
        sqlite3_reset(st_data);

        // --- B. Insertion dans l'Index Spatial ---
        sqlite3_bind_int64(st_idx, 1, last_id);
        sqlite3_bind_double(st_idx, 2, psShape->dfXMin);
        sqlite3_bind_double(st_idx, 3, psShape->dfXMax);
        sqlite3_bind_double(st_idx, 4, psShape->dfYMin);
        sqlite3_bind_double(st_idx, 5, psShape->dfYMax);

        sqlite3_step(st_idx);
        sqlite3_reset(st_idx);

        SHPDestroyObject(psShape);
    }

    sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);

    // 5. ENREGISTREMENT DANS LES MÉTADONNÉES
    // TODOil faut créer un style id ici
    // Ici on garde l'emprise globale pour le zoom par défaut
    int style_id = create_default_style(db->handle, table_name, nShapeType);    
    const char *meta_sql = "INSERT INTO bim_layers ("
    "name, geom_type, style_id" // <--- Pas de layer_id ici !
    ") VALUES (?, ?, ?);";      // <--- 3 points d'interrogation  
    sqlite3_stmt *st_meta;
    sqlite3_prepare_v2(db->handle, meta_sql, -1, &st_meta, NULL);
    sqlite3_bind_text(st_meta, 1, table_name, -1, SQLITE_STATIC);
    sqlite3_bind_int(st_meta, 2, nShapeType);
    sqlite3_bind_int(st_meta, 3, style_id); // style par défaut
     sqlite3_step(st_meta);

    // Ménage
    sqlite3_finalize(st_data);
    sqlite3_finalize(st_idx);
    sqlite3_finalize(st_meta);
    SHPClose(hSHP);
    
    return 0;
}
#ifdef __cplusplus
}
#endif
#endif