///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CV_DB_ENTITY_H
#define CV_DB_ENTITY_H
#ifdef __cplusplus
extern "C" {
#endif
/// insertion ////////////////////////////////////////////////////////////////////////////////////////////
static sqlite3_int64 bim_db_get_or_create_vertex(BimDB *db, double x, double y, int is_node) {
    sqlite3_stmt *stmt;
    sqlite3_int64 vtx_id = -1;
    double epsilon = 0.0001; // Tolérance de précision

    // 1. Chercher si un point existe déjà à ces coordonnées
    const char *sql_find = "SELECT id FROM bim_vertices WHERE x BETWEEN ? AND ? AND y BETWEEN ? AND ? LIMIT 1;";
    sqlite3_prepare_v2(db->handle, sql_find, -1, &stmt, NULL);
    sqlite3_bind_double(stmt, 1, x - epsilon);
    sqlite3_bind_double(stmt, 2, x + epsilon);
    sqlite3_bind_double(stmt, 3, y - epsilon);
    sqlite3_bind_double(stmt, 4, y + epsilon);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        vtx_id = sqlite3_column_int64(stmt, 0);
    }
    sqlite3_finalize(stmt);

    // 2. Si non trouvé, on le crée
    if (vtx_id == -1) {
        const char *sql_ins = "INSERT INTO bim_vertices (x, y, is_node) VALUES (?, ?, ?);";
        sqlite3_prepare_v2(db->handle, sql_ins, -1, &stmt, NULL);
        sqlite3_bind_double(stmt, 1, x);
        sqlite3_bind_double(stmt, 2, y);
        sqlite3_bind_int(stmt, 3, is_node);
        sqlite3_step(stmt);
        vtx_id = sqlite3_last_insert_rowid(db->handle);
        sqlite3_finalize(stmt);
    }
    else { //update use_count
        const char* sql_upd = "UPDATE bim_vertices SET use_count = use_count + 1 WHERE id = ?;";
        sqlite3_prepare_v2(db->handle, sql_upd, -1, &stmt, NULL);
        sqlite3_bind_int64(stmt, 1, vtx_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);        
    }
    
    return vtx_id;
}

static int bim_db_save_polyline(BimDB *db, BimPoint *pts, int pts_count, double  minX, double minY, double maxX, double maxY) {
    sqlite3_exec(db->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    // A. Création de l'entité parente
    sqlite3_stmt *st_ent;
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_entities (type) VALUES (?);", -1, &st_ent, NULL);
    sqlite3_bind_int(st_ent, 1, OBJ_POLYLINE);
    sqlite3_step(st_ent);
    sqlite3_int64 ent_id = sqlite3_last_insert_rowid(db->handle);
    sqlite3_finalize(st_ent);

    // B. Boucle sur les points
    sqlite3_stmt *st_jnc;
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_geometry_junction (entity_id, vertex_id, seq_order) VALUES (?, ?, ?);", -1, &st_jnc, NULL);

    for (int i = 0; i < pts_count; i++) {
        int is_node = (i == 0 || i == pts_count - 1) ? 1 : 0;
        
        // Utilisation de notre fonction de recherche/création
        sqlite3_int64 vtx_id = bim_db_get_or_create_vertex(db, pts[i].x, pts[i].y, is_node);

        sqlite3_bind_int64(st_jnc, 1, ent_id);
        sqlite3_bind_int64(st_jnc, 2, vtx_id);
        sqlite3_bind_int(st_jnc, 3, i);
        sqlite3_step(st_jnc);
        sqlite3_reset(st_jnc);
    }
    sqlite3_finalize(st_jnc);

    // C. Mise à jour de l'index spatial (obligatoire pour le rendu futur)
    sqlite3_stmt *st_idx;
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_spatial_index (id, minX, maxX, minY, maxY) VALUES (?, ?, ?, ?, ?);", -1, &st_idx, NULL);
    sqlite3_bind_int64( st_idx, 1, ent_id);
    sqlite3_bind_double(st_idx, 2, minX);
    sqlite3_bind_double(st_idx, 3, maxX);
    sqlite3_bind_double(st_idx, 4, minY);
    sqlite3_bind_double(st_idx, 5, maxY);
    sqlite3_step(st_idx);
    sqlite3_finalize(st_idx);

    return sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
}

//chargement de fichiers ////////////////////////////////////////////////////////////////////////////////////////////////////////
static void bim_db_force_ccw(BimPoint *pts, int count) {
    if (count < 3) return;

    double area = 0.0;
    for (int i = 0; i < count; i++) {
        // On boucle proprement : le dernier point revient au premier
        int next = (i + 1) % count;
        area += (double)(pts[i].x * pts[next].y) - (double)(pts[next].x * pts[i].y);
    }

    // Dans ce système de coordonnées (Y vers le haut ou bas), 
    // si area > 0 c'est un sens, si < 0 c'est l'autre.
    // Si area est négative, on inverse l'ordre pour forcer le CCW.
    if (area < 0) {
        int i = 0;
        int j = count - 1;
        while (i < j) {
            BimPoint temp = pts[i];
            pts[i] = pts[j];
            pts[j] = temp;
            i++;
            j--;
        }
    }
}


sqlite3_int64 bim_db_insert_entity_header(BimDB *db, int type, double minX, double maxX, double minY, double maxY) {
    sqlite3_stmt *st;
    
    // 1. Entité
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_entities (type, layer_id) VALUES (?, 0);", -1, &st, NULL);
    sqlite3_bind_int(st, 1, type);
    sqlite3_step(st);
    sqlite3_int64 ent_id = sqlite3_last_insert_rowid(db->handle);
    sqlite3_finalize(st);

    // 2. R-Tree
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_spatial_index (id, minX, maxX, minY, maxY) VALUES (?, ?, ?, ?, ?);", -1, &st, NULL);
    sqlite3_bind_int64(st, 1, ent_id);
    sqlite3_bind_double(st, 2, minX);
    sqlite3_bind_double(st, 3, maxX);
    sqlite3_bind_double(st, 4, minY);
    sqlite3_bind_double(st, 5, maxY);
    sqlite3_step(st);
    sqlite3_finalize(st);

    return ent_id;
}

// B. Lie un point à une entité (La table de jonction)
void bim_db_link_vertex(BimDB *db, sqlite3_int64 ent_id, double x, double y, int part_id, int seq, int is_node) {
    sqlite3_int64 vtx_id = bim_db_get_or_create_vertex(db, x, y, is_node);
    
    sqlite3_stmt *st;
    sqlite3_prepare_v2(db->handle, 
        "INSERT INTO bim_geometry_junction (entity_id, vertex_id, part_id, seq_order) VALUES (?, ?, ?, ?);", 
        -1, &st, NULL);
    
    sqlite3_bind_int64(st, 1, ent_id);
    sqlite3_bind_int64(st, 2, vtx_id);
    sqlite3_bind_int(st, 3, part_id);
    sqlite3_bind_int(st, 4, seq);
    
    sqlite3_step(st);
    sqlite3_finalize(st);
}

static int bim_db_save_node(BimDB *db, int layer_id, int symbol_id, double x, double y, double rotation) {
    sqlite3_exec(db->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    // 1. Création de l'entité de type NODE (type 3 par exemple, ou OBJ_NODE)
    sqlite3_stmt *st_ent;
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_entities (type, layer_id) VALUES (?, ?);", -1, &st_ent, NULL);
    sqlite3_bind_int(st_ent, 1, OBJ_NODE); // Assure-toi que OBJ_NODE est défini
    sqlite3_bind_int(st_ent, 2, layer_id);
    int rc = sqlite3_step(st_ent);
    sqlite3_int64 ent_id = sqlite3_last_insert_rowid(db->handle);
    sqlite3_finalize(st_ent);

    // 2. Création/Récupération du vertex avec injection du symbole et de la rotation
    // On utilise une version légèrement modifiée ou on fait un UPDATE juste après
    sqlite3_int64 vtx_id = bim_db_get_or_create_vertex(db, x, y, 1); // 1 car un NODE est toujours un noeud

    // Mise à jour des attributs spécifiques au symbole sur le vertex
    sqlite3_stmt *st_vtx_upd;
    sqlite3_prepare_v2(db->handle, "UPDATE bim_vertices SET symbol_id = ?, rotation = ? WHERE id = ?;", -1, &st_vtx_upd, NULL);
    sqlite3_bind_int(st_vtx_upd, 1, symbol_id);
    sqlite3_bind_double(st_vtx_upd, 2, rotation);
    sqlite3_bind_int64(st_vtx_upd, 3, vtx_id);
    sqlite3_step(st_vtx_upd);
    sqlite3_finalize(st_vtx_upd);

    // 3. Liaison dans la table de jonction
    sqlite3_stmt *st_jnc;
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_geometry_junction (entity_id, vertex_id, part_id, seq_order) VALUES (?, ?, 0, 0);", -1, &st_jnc, NULL);
    sqlite3_bind_int64(st_jnc, 1, ent_id);
    sqlite3_bind_int64(st_jnc, 2, vtx_id);
    sqlite3_step(st_jnc);
    sqlite3_finalize(st_jnc);

    // 4. Mise à jour de l'index spatial (RTree)
    // On donne une micro-dimension au point pour que le RTree soit toujours à l'aise
    sqlite3_stmt *st_idx;
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_spatial_index (id, minX, maxX, minY, maxY) VALUES (?, ?, ?, ?, ?);", -1, &st_idx, NULL);
    sqlite3_bind_int64(st_idx, 1, ent_id);
    sqlite3_bind_double(st_idx, 2, x - 0.01);
    sqlite3_bind_double(st_idx, 3, x + 0.01);
    sqlite3_bind_double(st_idx, 4, y - 0.01);
    sqlite3_bind_double(st_idx, 5, y + 0.01);
    sqlite3_step(st_idx);
    sqlite3_finalize(st_idx);

    return sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
}

static int bim_db_save_entity(BimDB *db, int layer_id, int type, BimPoint *pts, int pts_count, int *part_assignments) {
    if (type == OBJ_POLYGON)
        bim_db_force_ccw(pts, pts_count);
    // 1. Calcul de la BBox si non fournie (sécurité)
    double minX = pts[0].x, maxX = pts[0].x, minY = pts[0].y, maxY = pts[0].y;
    for(int i=1; i < pts_count; i++) {
        if(pts[i].x < minX) minX = pts[i].x; if(pts[i].x > maxX) maxX = pts[i].x;
        if(pts[i].y < minY) minY = pts[i].y; if(pts[i].y > maxY) maxY = pts[i].y;
    }

    sqlite3_exec(db->handle, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    // A. Création de l'entité (avec son type : POLYLINE ou POLYGON)
    sqlite3_stmt *st_ent;
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_entities (type, layer_id) VALUES (?, ?);", -1, &st_ent, NULL);
    sqlite3_bind_int(st_ent, 1, type);
    sqlite3_bind_int(st_ent, 2, layer_id);
    int rc = sqlite3_step(st_ent);
    CHECK_SQL(rc, db);
    sqlite3_int64 ent_id = sqlite3_last_insert_rowid(db->handle);
    sqlite3_finalize(st_ent);

    // B. Insertion dans la jonction (avec PART_ID)
    sqlite3_stmt *st_jnc;
    // On ajoute PART_ID dans le SQL
    sqlite3_prepare_v2(db->handle, 
        "INSERT INTO bim_geometry_junction (entity_id, vertex_id, part_id, seq_order) VALUES (?, ?, ?, ?);", 
        -1, &st_jnc, NULL);

    for (int i = 0; i < pts_count; i++) {
        // Un sommet est un "node" s'il est au début ou à la fin d'une PART
        // Pour faire simple ici, on considère tous les points de jonction
        int is_node = 0; 
        if (i == 0 || i == pts_count - 1 || (part_assignments && part_assignments[i] != part_assignments[i-1])) {
            is_node = 1;
        }

        sqlite3_int64 vtx_id = bim_db_get_or_create_vertex(db, pts[i].x, pts[i].y, is_node);

        sqlite3_bind_int64(st_jnc, 1, ent_id);
        sqlite3_bind_int64(st_jnc, 2, vtx_id);
        // Si part_assignments est NULL, on met 0 par défaut (enveloppe principale)
        sqlite3_bind_int(st_jnc, 3, part_assignments ? part_assignments[i] : 0);
        sqlite3_bind_int(st_jnc, 4, i);
        
        rc = sqlite3_step(st_jnc);
        CHECK_SQL(rc, db);
        sqlite3_reset(st_jnc);
    }
    sqlite3_finalize(st_jnc);

    // C. Index Spatial (RTree)
    sqlite3_stmt *st_idx;
    sqlite3_prepare_v2(db->handle, "INSERT INTO bim_spatial_index (id, minX, maxX, minY, maxY) VALUES (?, ?, ?, ?, ?);", -1, &st_idx, NULL);
    sqlite3_bind_int64(st_idx, 1, ent_id);
    sqlite3_bind_double(st_idx, 2, minX);
    sqlite3_bind_double(st_idx, 3, maxX);
    sqlite3_bind_double(st_idx, 4, minY);
    sqlite3_bind_double(st_idx, 5, maxY);
    rc = sqlite3_step(st_idx);
    CHECK_SQL(rc,db);
    sqlite3_finalize(st_idx);

    return sqlite3_exec(db->handle, "COMMIT;", NULL, NULL, NULL);
}

int bim_db_delete_entity(BimDB *db) {
    char *err_msg = 0;
    printf("in delete \n");
    char sql[] = "BEGIN; "
    "DELETE FROM bim_entities WHERE id IN (SELECT entity_id from bim_selection); "
    "COMMIT;";
    
    int rc = sqlite3_exec(db->handle, sql, 0, 0, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur suppression: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_exec(db->handle, "ROLLBACK;", 0, 0, 0); // Annule tout en cas d'erreur
        return 0;
    }
    return 1;
}
#ifdef __cplusplus
}
#endif
#endif