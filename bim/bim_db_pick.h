///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CV_DB_PICK_H
#define CV_DB_PICK_H
#ifdef __cplusplus
extern "C" {
#endif

/// picking ////////////////////////////////////////////////////////////////////////////////////////////


/*
INSERT INTO bim_selection (entity_id)
SELECT e.id FROM bim_entities e
JOIN bim_spatial_index s ON e.id = s.id
WHERE s.minX <= ?1 + ?3 AND s.maxX >= ?1 - ?3
  AND s.minY <= ?2 + ?3 AND s.maxY >= ?2 - ?3
ORDER BY 
    -- On divise la distance par 3 pour les NODES (type élevé) 
    -- Cela les rend artificiellement "plus proches" du curseur
    CASE WHEN e.type >= 100 THEN bim_distance(?1, ?2, e.id) / 3.0 
         ELSE bim_distance(?1, ?2, e.id) END ASC,
    e.type DESC
LIMIT 1;
*/

int bim_db_pick(BimDB *db, double worldX, double worldY, double tolerance, int add) {
    
    if (!add)
    {
        sqlite3_exec(db->handle, "DELETE FROM bim_selection;", NULL, NULL, NULL);
    }

    // 2. TODO ajouter la discrimination de type et la sélection BLOB
    const char *sql = 
    "INSERT INTO bim_selection (entity_id) "
    "SELECT e.id FROM bim_entities e "
    "JOIN bim_spatial_index s ON e.id = s.id " // Le filtre spatial rapide
    "WHERE s.minX <= ?1 + ?3 AND s.maxX >= ?1 - ?3 "
    "  AND s.minY <= ?2 + ?3 AND s.maxY >= ?2 - ?3 "
    "  AND bim_distance(?1, ?2, e.id) <= ?3 "    // La précision chirurgicale
    "ORDER BY bim_distance(?1, ?2, e.id) ASC, "   // On prend le plus proche
    "e.type DESC "
    "LIMIT 1;";
    

    //printf("%s\n", sql);
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    CHECK_SQL(rc,db);
    if ( rc == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, worldX);
        sqlite3_bind_double(stmt, 2, worldY);
        sqlite3_bind_double(stmt, 3, tolerance);

        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }

    // 3. On compte combien on a sélectionné (0 ou 1)
    int changes = sqlite3_changes(db->handle);
    
    return changes;    
}

int bim_db_pick_rectangle(BimDB *db, double x1, double y1, double x2, double y2) {
    // 1. Normalisation des coordonnées (s'assurer que min < max)
    double minX = (x1 < x2) ? x1 : x2;
    double maxX = (x1 < x2) ? x2 : x1;
    double minY = (y1 < y2) ? y1 : y2;
    double maxY = (y1 < y2) ? y2 : y1;

    // 2. On vide la sélection précédente (Optionnel, selon ton UI)
    sqlite3_exec(db->handle, "DELETE FROM bim_selection;", NULL, NULL, NULL);

    // 3. Insertion massive via l'index spatial
    // On sélectionne tous les IDs qui intersectent le rectangle
    const char *sql = 
        "INSERT INTO bim_selection (entity_id) "
        "SELECT id FROM bim_spatial_index "
        "WHERE minX <= ?2 AND maxX >= ?1 "
        "  AND minY <= ?4 AND maxY >= ?3;";

    sqlite3_stmt *stmt;
    int count = 0;

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, minX);
        sqlite3_bind_double(stmt, 2, maxX);
        sqlite3_bind_double(stmt, 3, minY);
        sqlite3_bind_double(stmt, 4, maxY);

        if (sqlite3_step(stmt) == SQLITE_DONE) {
            // On récupère le nombre de lignes insérées
            count = sqlite3_changes(db->handle);
        }
        sqlite3_finalize(stmt);
    }

    return count; // Retourne le nombre d'objets sélectionnés
}



bool bim_db_get_snapped_coordinates(BimDB* db, double mouseX, double mouseY, double tolerance, double* outX, double* outY) {
    sqlite3_stmt* stmt = NULL;
    bool snapped = false;

    // 1. Préparation de la requête
    const char* sql = "SELECT x, y FROM bim_vertices "
                      "WHERE x BETWEEN ?1 - ?3 AND ?1 + ?3 "
                      "AND y BETWEEN ?2 - ?3 AND ?2 + ?3 "
                      "ORDER BY ((x - ?1)*(x - ?1) + (y - ?2)*(y - ?2)) ASC " // Optionnel : pour avoir le plus proche
                      "LIMIT 1;";

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK) {
        // Optionnel : fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return false;
    }

    // 2. Liaison des paramètres (Binding)
    // ?1 = mouseX, ?2 = mouseY, ?3 = tolerance
    sqlite3_bind_double(stmt, 1, mouseX);
    sqlite3_bind_double(stmt, 2, mouseY);
    sqlite3_bind_double(stmt, 3, tolerance);

    // 3. Exécution et récupération des résultats
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        *outX = sqlite3_column_double(stmt, 0);
        *outY = sqlite3_column_double(stmt, 1);
        snapped = true;
    } else {
        // Si aucun point n'est trouvé, on conserve les coordonnées originales
        *outX = mouseX;
        *outY = mouseY;
    }

    // 4. Nettoyage impératif du statement
    sqlite3_finalize(stmt);

    return snapped;
}
#ifdef __cplusplus
}
#endif
#endif