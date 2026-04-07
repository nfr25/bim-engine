///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CV_DB_EXTENT_H
#define CV_DB_EXTENT_H
#ifdef __cplusplus
extern "C" {
#endif

static void bim_db_get_full_extents(BimDB *db, double *minX, double *maxX, double *minY, double *maxY);

// Helper : Convertit un point écran (pixel) en point monde (coordonnée BIM)
static void stw(GuiCanvas* cv, int x, int y, double* worldX, double* worldY) {
    *worldX = ((double)x - cv->offset.x) / cv->zoom;
    *worldY = (cv->offset.y - (double)y) / cv->zoom;
}

static void wts(GuiCanvas* cv, float x, float y, int *screen_x, int *screen_y) {
    *screen_x = (int)((x * cv->zoom) + cv->offset.x);
    *screen_y = (int)(cv->offset.y - (y * cv->zoom));
}

static void wtsd(GuiCanvas* cv, float x, float y, double *screen_x, double *screen_y) {
    *screen_x = ((x * cv->zoom) + cv->offset.x);
    *screen_y = (cv->offset.y - (y * cv->zoom));
}

static double bim_distance(BimPoint *a, BimPoint *b){
    return sqrt((a->x - b->x)*(a->x - b->x) + (a->y - b->y)*(a->y - b->y));
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////

static void canvas_get_object_bounds(BimObject* obj, float* minX, float* minY, float* maxX, float* maxY) {
    *minX = 1e10f;  *minY = 1e10f;
    *maxX = -1e10f; *maxY = -1e10f; 
    for (int i = 0; i < obj->poly.count; i++) {
        if (obj->poly.vertices[i].x < *minX) *minX = obj->poly.vertices[i].x;
        if (obj->poly.vertices[i].x > *maxX) *maxX = obj->poly.vertices[i].x;
        if (obj->poly.vertices[i].y < *minY) *minY = obj->poly.vertices[i].y;
        if (obj->poly.vertices[i].y > *maxY) *maxY = obj->poly.vertices[i].y;
    }
}

static void canvas_get_bounds(GuiCanvas* cv, double* minX, double* minY, double* maxX, double* maxY){
    bim_db_get_full_extents(&(cv->db), minX, maxX, minY, maxY);
 }

static void canvas_zoom_extents(GuiCanvas* cv) {
   
    if (!cv->refit) return;
    // 1. Initialiser les bornes avec le premier objet
    double minX = 1e10f,  minY =  1e10f;
    double maxX = -1e10f, maxY = -1e10f;

    // 2. Calculer la Bounding Box de la Display List
    canvas_get_bounds(cv,&minX, &minY, &maxX, &maxY);

    float winW = cv->w;
    float winH = cv->h;
      
    if (winW <= 0 || winH <= 0) return;

    // 4. Calculer la taille du dessin
    float drawW = maxX - minX;
    float drawH = maxY - minY;
 
    // Sécurité si le dessin est un point unique
    if (drawW < 1.0f) drawW = 10.0f; 
    if (drawH < 1.0f) drawH = 10.0f;

    // 5. Calculer le zoom idéal avec une marge (90% de l'écran)
    float zoomX = (winW * 0.9f) / drawW;
    float zoomY = (winH * 0.9f) / drawH;
    
    // On prend le zoom le plus restrictif
    cv->zoom = (zoomX < zoomY) ? zoomX : zoomY;

    // 6. Centrer le dessin
    // On aligne le centre du monde sur le centre de l'écran
    float centerX = minX + (drawW / 2.0f);
    float centerY = minY + (drawH / 2.0f);

    cv->offset.x = (winW / 2.0f) - (centerX * cv->zoom);
    cv->offset.y = (winH / 2.0f) + (centerY * cv->zoom);  //inversion du y
    cv->refit = false;
    cv->cache_valid = false;
}


static void canvas_zoom_center(GuiCanvas *cv , float ratio) {
    float midX = cv->w / 2.0f;
    float midY = cv->h / 2.0f;

    // 1. Position monde du centre avant zoom
    float wx = (midX - cv->offset.x) / cv->zoom;
    float wy = (midY - cv->offset.y) / cv->zoom;

    // 2. Appliquer le zoom
    cv->zoom *= ratio;

    // 3. Réaligner
    cv->offset.x = midX - (wx * cv->zoom);
    cv->offset.y = midY - (wy * cv->zoom);
    cv->cache_valid = false;
}

static void canvas_zoom_plus(GuiCanvas *cv)
{
    canvas_zoom_center(cv, 1.2f);
}

static void canvas_zoom_minus(GuiCanvas *cv)
{
    canvas_zoom_center(cv, 0.833f);
}

static void canvas_zoom_window(GuiCanvas* cv, double x1, double y1, double x2, double y2) {
    // 1. Swap pour avoir x1, y1 en bas-gauche et x2, y2 en haut-droite (Monde)
    if (x1 > x2) { double t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { double t = y1; y1 = y2; y2 = t; }

    double worldW = x2 - x1;
    double worldH = y2 - y1;

    if (worldW < 1e-6 || worldH < 1e-6) return;

    // 2. Calcul du zoom pour tenir dans la fenêtre (avec marge)
    double zoomX = (cv->w * 0.90) / worldW;
    double zoomY = (cv->h * 0.90) / worldH;
    cv->zoom = (zoomX < zoomY) ? zoomX : zoomY;

    // 3. Calcul du centre du rectangle (Monde)
    double centerX = (x1 + x2) / 2.0;
    double centerY = (y1 + y2) / 2.0;

    // 4. Inversion de la logique wtsd pour trouver l'offset écran
    // On veut que wtsd(centerX, centerY) tombe au milieu de l'écran (cv->w/2, cv->h/2)
    
    // Pour X : (centerX * zoom) + offset.x = w/2  =>  offset.x = w/2 - (centerX * zoom)
    cv->offset.x = (cv->w / 2.0) - (centerX * cv->zoom);

    // Pour Y : offset.y - (centerY * zoom) = h/2  =>  offset.y = h/2 + (centerY * zoom)
    cv->offset.y = (cv->h / 2.0) + (centerY * cv->zoom);
    cv->cache_valid = false;
}

static void canvas_get_viewport_bounds(GuiCanvas* cv, float* vMinX, float* vMaxX, float* vMinY, float* vMaxY) {
    // 1. Récupérer la taille de la zone de dessin en pixels
    //RECT rc;
    double x_top_left, y_top_left;
    double x_bottom_right, y_bottom_right;

    stw(cv, 0, 0, &x_top_left, &y_top_left);
    stw(cv, cv->w, cv->h, &x_bottom_right, &y_bottom_right);
   // 3. Déterminer les Min/Max (essentiel si l'axe Y est inversé ou s'il y a rotation)
    *vMinX = (x_top_left < x_bottom_right) ? x_top_left : x_bottom_right;
    *vMaxX = (x_top_left > x_bottom_right) ? x_top_left : x_bottom_right;
    
    *vMinY = (y_top_left < y_bottom_right) ? y_top_left : y_bottom_right;
    *vMaxY = (y_top_left > y_bottom_right) ? y_top_left : y_bottom_right;
}

static void update_extent_from_rtree(BimDB *db, const char *table_name, 
                                     double *minX, double *maxX, double *minY, double *maxY, 
                                     int *found) {
    char sql[256] = "SELECT MIN(minX), MAX(maxX), MIN(minY), MAX(maxY) FROM bim_spatial_index;";
    sqlite3_stmt *stmt;
    if (table_name != NULL)
        snprintf(sql, sizeof(sql), "SELECT MIN(minX), MAX(maxX), MIN(minY), MAX(maxY) FROM idx_%s;", table_name);

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            double lMinX = sqlite3_column_double(stmt, 0);
            double lMaxX = sqlite3_column_double(stmt, 1);
            double lMinY = sqlite3_column_double(stmt, 2);
            double lMaxY = sqlite3_column_double(stmt, 3);

            if (lMinX < *minX) *minX = lMinX;
            if (lMaxX > *maxX) *maxX = lMaxX;
            if (lMinY < *minY) *minY = lMinY;
            if (lMaxY > *maxY) *maxY = lMaxY;
            *found = 1;
        }
        sqlite3_finalize(stmt);
    }
}

static void bim_db_get_full_extents(BimDB *db, double *minX, double *maxX, double *minY, double *maxY) {
    int found = 0;
    sqlite3_stmt *stmt;
    // Initialisation avec des valeurs neutres (ou extrêmes)
    *minX = 0.0; *maxX = 0.0; *minY = 0.0; *maxY = 0.0;
    double tminX = 1e30, tmaxX = -1e30, tminY = 1e30, tmaxY = -1e30;
    //initialisation avec les layers topologiques
    update_extent_from_rtree(db,NULL, &tminX, &tmaxX, &tminY, &tmaxY, &found);
    const char *sql_layers = "SELECT name FROM bim_layers where geom_type > 0;";
    if (sqlite3_prepare_v2(db->handle, sql_layers, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Erreur lors de la lecture des layers : %s\n", sqlite3_errmsg(db->handle));
        return;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) { 
        const char *table_name = (const char *)sqlite3_column_text(stmt, 0);
        if (!table_name) continue;
        update_extent_from_rtree(db, table_name, &tminX, &tmaxX, &tminY, &tmaxY, &found);
    }
    sqlite3_finalize(stmt);
    if (found) {
        *minX = tminX; *maxX = tmaxX; *minY = tminY; *maxY = tmaxY;
    } else {
        // Fallback si la base est totalement vide
        *minX = 0.0; *maxX = 100.0; *minY = 0.0; *maxY = 100.0;
    }        
}

static int bim_db_get_selection_extents(BimDB *db, double *minX, double *maxX, double *minY, double *maxY) {
    // Initialisation par défaut (cas où la sélection est vide)
    *minX = *minY = 1e10;
    *maxX = *maxY = -1e10;
    int found = 0;
    const char *sql = 
        "SELECT MIN(s.minX), MAX(s.maxX), MIN(s.minY), MAX(s.maxY) "
        "FROM bim_selection sel "
        "JOIN bim_spatial_index s ON sel.entity_id = s.id;";

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            // On vérifie si le résultat n'est pas NULL (SQL renvoie NULL si la table est vide)
            if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
                *minX = sqlite3_column_double(stmt, 0);
                *maxX = sqlite3_column_double(stmt, 1);
                *minY = sqlite3_column_double(stmt, 2);
                *maxY = sqlite3_column_double(stmt, 3);
                found = 1;
            }
        }
        sqlite3_finalize(stmt);
    }
    return found;
}

#ifdef __cplusplus
}
#endif
#endif