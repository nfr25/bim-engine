///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef CV_DB_SCHEMA_H
#define CV_DB_SCHEMA_H
#ifdef __cplusplus
extern "C" {
#endif

//DB building//////////////////////////////////////////////////////////////////////////////////////////////////////
static int bim_db_init_schema(BimDB *db) {

    const char *sql = 
    "PRAGMA foreign_keys = ON; "

    "CREATE TABLE IF NOT EXISTS bim_config ( "
    "key   TEXT PRIMARY KEY, "
    "value TEXT "
    "); "

    "CREATE TABLE IF NOT EXISTS bim_styles ( "
    "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "  name TEXT NOT NULL, "
    "  stroke_color INTEGER DEFAULT 0, "
    "  stroke_width INTEGER DEFAULT 1, "
    "  fill_color INTEGER DEFAULT 16777215, "
    "  line_type INTEGER DEFAULT 0 "
    "); "

    "CREATE TABLE IF NOT EXISTS bim_layers ( "
    "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "  name TEXT NOT NULL UNIQUE, "
    "  geom_type INTEGER default 0, "        
    "  z_order INTEGER DEFAULT 0, "
    "  is_visible INTEGER DEFAULT 1, "
    "  is_locked INTEGER DEFAULT 0, "
    "  style_id INTEGER REFERENCES bim_styles(id) "
    "); "

    "CREATE TABLE IF NOT EXISTS bim_svg_library ( "
    " id INTEGER PRIMARY KEY AUTOINCREMENT, "
    " name TEXT NOT NULL, "
    " category TEXT, "
    " svg_data TEXT NOT NULL, "
    " hash TEXT UNIQUE, "
    " ports_mask INTEGER DEFAULT 255 "
    "); "       

    "CREATE TABLE IF NOT EXISTS bim_entities ( "
    "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "  type INTEGER NOT NULL, "
    "  layer_id INTEGER DEFAULT 100, "
    "  style_id INTEGER DEFAULT NULL, "
    "  FOREIGN KEY (layer_id) REFERENCES bim_layers(id) ON DELETE CASCADE, "
    "  FOREIGN KEY (style_id) REFERENCES bim_styles(id) ON DELETE SET NULL "
    "); "

    "CREATE TABLE IF NOT EXISTS bim_vertices ( "
    "  id INTEGER PRIMARY KEY AUTOINCREMENT, "
    "  x REAL NOT NULL, "
    "  y REAL NOT NULL, "
    "  symbol_id INTEGER DEFAULT NULL, "
    "  scale REAL DEFAULT 1, "
    "  rotation REAL DEFAULT 0, "    
    "  is_node INTEGER DEFAULT 1, "
    "  use_count INTEGER DEFAULT 1, "
    "  ports_active INTEGER DEFAULT 255, "
    "  FOREIGN KEY(symbol_id) REFERENCES bim_svg_library(id) ON DELETE SET NULL "    
    "); "

   
    "CREATE INDEX IF NOT EXISTS idx_vertices_coords ON bim_vertices (x, y); "

    "CREATE TABLE IF NOT EXISTS bim_geometry_junction ( "
    "  entity_id INTEGER, "
    "  vertex_id INTEGER, "
    "  seq_order INTEGER, "
    "  part_id   INTEGER, "
    "  PRIMARY KEY (entity_id, seq_order), "
    "  FOREIGN KEY (entity_id) REFERENCES bim_entities(id) ON DELETE CASCADE, "
    "  FOREIGN KEY (vertex_id) REFERENCES bim_vertices(id) "
    "); "

    "CREATE VIRTUAL TABLE IF NOT EXISTS bim_spatial_index "
    "USING rtree(id, minX, maxX, minY, maxY); "

    "CREATE TABLE IF NOT EXISTS bim_selection ( "
    "  entity_id INTEGER PRIMARY KEY REFERENCES bim_entities(id) ON DELETE CASCADE "
    "); "

    "CREATE TRIGGER IF NOT EXISTS trg_cleanup_rtree_on_entity_delete "
    "AFTER DELETE ON bim_entities "
    "BEGIN "
    "  DELETE FROM bim_spatial_index WHERE id = OLD.id; "
    "END; "

    "CREATE TRIGGER IF NOT EXISTS trg_update_and_cleanup_vertices "
    "AFTER DELETE ON bim_geometry_junction "
    "BEGIN "
        "UPDATE bim_vertices " 
        "SET use_count = use_count - 1 "
        "WHERE id = OLD.vertex_id; "
        "DELETE FROM bim_vertices "
        "WHERE id = OLD.vertex_id "
        "AND use_count <= 0;"
    "END; ";

    char *err = NULL;
    int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        // Log error via votre future console
        printf("ECHEC CRITIQUE RTREE : %s (Code %d)\n", err, rc);
        sqlite3_free(err);
    }
    else
    {
        printf("DB created\n");
    }
    return rc;
}


//On polyline////////////////////////////////////////////////////////

double calculate_segment_dist(double mx, double my, double xA, double yA, double xB, double yB) {
    double dx = xB - xA;
    double dy = yB - yA;
    double l2 = dx*dx + dy*dy; // Longueur du segment au carré

    if (l2 == 0.0) return sqrt((mx-xA)*(mx-xA) + (my-yA)*(my-yA));

    // Calcul de t (projection relative)
    double t = ((mx - xA) * dx + (my - yA) * dy) / l2;

    // Clamping pour rester sur le segment
    if (t < 0.0) t = 0.0;
    else if (t > 1.0) t = 1.0;

    double projX = xA + t * dx;
    double projY = yA + t * dy;

    return sqrt((mx - projX)*(mx - projX) + (my - projY)*(my - projY));
}


void bim_sql_dist_blob(sqlite3_context *context, int argc, sqlite3_value **argv) {
    // argv[0] : mx, argv[1] : my, argv[2] : le BLOB (data)
    double mx = sqlite3_value_double(argv[0]);
    double my = sqlite3_value_double(argv[1]);
    
    // On récupère le pointeur direct sur le blob
    const double *pts = (const double*)sqlite3_value_blob(argv[2]);
    int byte_size = sqlite3_value_bytes(argv[2]);
    int num_coords = byte_size / sizeof(double);

    if (num_coords < 2 || pts == NULL) {
        sqlite3_result_double(context, 1e10);
        return;
    }

    double min_dist = 1e10;

    // Cas d'un point unique stocké en Blob (si ça arrive)
    if (num_coords == 2) {
        min_dist = sqrt(pow(mx - pts[0], 2) + pow(my - pts[1], 2));
    } 
    else {
        // Parcours des segments (X1, Y1, X2, Y2...)
        for (int i = 0; i < num_coords - 3; i += 2) {
            double xA = pts[i];
            double yA = pts[i+1];
            double xB = pts[i+2];
            double yB = pts[i+3];

            double d = calculate_segment_dist(mx, my, xA, yA, xB, yB);
            if (d < min_dist) min_dist = d;
        }
    }

    sqlite3_result_double(context, min_dist);
}


void bim_sql_dist_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    BimDB *db = (BimDB*)sqlite3_user_data(context);
    double mx = sqlite3_value_double(argv[0]);
    double my = sqlite3_value_double(argv[1]);
    int entity_id = sqlite3_value_int(argv[2]);

    // Gestion du cache du statement
    if (db->stmt_dist_cache == NULL) {
        const char *sql = "SELECT v.x, v.y, e.type FROM bim_vertices v "
                          "JOIN bim_geometry_junction j ON v.id = j.vertex_id "
                          "JOIN bim_entities e ON j.entity_id = e.id "
                          "WHERE j.entity_id = ?1 ORDER BY j.seq_order;";
        if (sqlite3_prepare_v2(db->handle, sql, -1, &db->stmt_dist_cache, NULL) != SQLITE_OK) {
            sqlite3_result_error(context, "Failed to prepare dist geom query", -1);
            return;
        }
    } else {
        sqlite3_reset(db->stmt_dist_cache);
    }

    sqlite3_bind_int(db->stmt_dist_cache, 1, entity_id);

    double min_dist = 1e10; // Très grand
    double xA, yA, xB, yB;
    int first = 1;
    int type = -1;
    while (sqlite3_step(db->stmt_dist_cache) == SQLITE_ROW) {
        double x = sqlite3_column_double(db->stmt_dist_cache, 0);
        double y = sqlite3_column_double(db->stmt_dist_cache, 1);
        if (first) type = sqlite3_column_int(db->stmt_dist_cache, 2);

        if (type == OBJ_NODE) {
            // Distance point à point pure
            min_dist = sqrt(pow(mx - x, 2) + pow(my - y, 2));
            break; // Un seul point, on a fini
        } 
        if (first) {
            xA = x;
            yA = y;
            first = 0;
            continue;
        }
        xB = x;
        yB = y;

        double d = calculate_segment_dist(mx, my, xA, yA, xB, yB);
        if (d < min_dist) min_dist = d;

        xA = xB; yA = yB;
    }

    sqlite3_result_double(context, min_dist);
}
/// INSIDE surface //////////////////////////

int is_point_in_entity(float x, float y, float* px, float* py, int* p_part_ids, int count) {
    int inside = 0;
    int start = 0;

    for (int i = 1; i <= count; i++) {
        // Détection de la fin d'une part (trou ou enveloppe)
        if (i == count || p_part_ids[i] != p_part_ids[start]) {
            int part_len = i - start;
            
            // On applique le Ray Casting sur cette part précise
            // On accumule le résultat avec un XOR (^)
            for (int k = 0, l = part_len - 1; k < part_len; l = k++) {
                if (((py[start + k] > y) != (py[start + l] > y)) &&
                    (x < (px[start + l] - px[start + k]) * (y - py[start + k]) / 
                    (py[start + l] - py[start + k]) + px[start + k])) {
                    inside = !inside;
                }
            }
            start = i;
        }
    }
    return inside;
}

void bim_sql_inside_func(sqlite3_context *context, int argc, sqlite3_value **argv) {
    BimDB *db = (BimDB*)sqlite3_user_data(context);
    double mx = sqlite3_value_double(argv[0]);
    double my = sqlite3_value_double(argv[1]);
    int entity_id = sqlite3_value_int(argv[2]);

    // 1. Préparation ou Reset du cache
    if (db->stmt_inside_cache == NULL) {
        // IMPORTANT : On récupère aussi part_id pour gérer les trous
        const char *sql = "SELECT v.x, v.y, j.part_id FROM bim_vertices v "
                          "JOIN bim_geometry_junction j ON v.id = j.vertex_id "
                          "WHERE j.entity_id = ?1 ORDER BY j.part_id, j.seq_order;";
        if (sqlite3_prepare_v2(db->handle, sql, -1, &db->stmt_inside_cache, NULL) != SQLITE_OK) {
            sqlite3_result_error(context, "Failed to prepare inside geom query", -1);
            return;
        }
    } else {
        sqlite3_reset(db->stmt_inside_cache);
    }

    sqlite3_bind_int(db->stmt_inside_cache, 1, entity_id);

    int inside = 0;
    double xA, yA, xB, yB;
    int first_in_part = 1;
    int current_part = -1;
    double part_start_x, part_start_y;

    while (sqlite3_step(db->stmt_inside_cache) == SQLITE_ROW) {
        double x = sqlite3_column_double(db->stmt_inside_cache, 0);
        double y = sqlite3_column_double(db->stmt_inside_cache, 1);
        int p_id = sqlite3_column_int(db->stmt_inside_cache, 2);

        // Détection de changement de "part" (contour ou trou)
        if (p_id != current_part) {
            // Si on finit une part, il faut fermer le dernier segment 
            // entre (xA, yA) et (part_start_x, part_start_y)
            if (current_part != -1) {
                if (((part_start_y > my) != (yA > my)) &&
                    (mx < (xA - part_start_x) * (my - part_start_y) / (yA - part_start_y) + part_start_x)) {
                    inside = !inside;
                }
            }
            current_part = p_id;
            part_start_x = x; part_start_y = y;
            xA = x; yA = y;
            continue;
        }

        xB = x; yB = y;

        // Algorithme de Ray Casting (Parité)
        if (((yB > my) != (yA > my)) &&
            (mx < (xA - xB) * (my - yB) / (yA - yB) + xB)) {
            inside = !inside;
        }

        xA = xB; yA = yB;
    }

    // Fermeture du tout dernier segment de la dernière part
    if (current_part != -1) {
        if (((part_start_y > my) != (yA > my)) &&
            (mx < (xA - part_start_x) * (my - part_start_y) / (yA - part_start_y) + part_start_x)) {
            inside = !inside;
        }
    }

    sqlite3_result_int(context, inside);
}

/* ── Constantes ports de connexion (bitfield 8 directions) ─────────
 *
 *   NW  N  NE
 *    W  ●  E
 *   SW  S  SE
 *
 * bim_svg_library.ports_mask  = ports disponibles par défaut du symbole
 * bim_vertices.ports_active   = ports actifs pour cette instance
 * ─────────────────────────────────────────────────────────────────── */
#define BIM_PORT_N    0x01
#define BIM_PORT_NE   0x02
#define BIM_PORT_E    0x04
#define BIM_PORT_SE   0x08
#define BIM_PORT_S    0x10
#define BIM_PORT_SW   0x20
#define BIM_PORT_W    0x40
#define BIM_PORT_NW   0x80
#define BIM_PORT_ALL  0xFF
#define BIM_PORT_NONE 0x00
/* Paires courantes */
#define BIM_PORT_EW   (BIM_PORT_E | BIM_PORT_W)    /* vanne, tronçon  */
#define BIM_PORT_NS   (BIM_PORT_N | BIM_PORT_S)    /* vertical        */
#define BIM_PORT_4    (BIM_PORT_EW | BIM_PORT_NS)  /* croix 4 ports   */

/* ── Système de notification DB → application ───────────────────────
 * Permet à la DB d'émettre des événements via des triggers SQLite.
 * L'application enregistre un callback via bim_db_set_notify_cb().
 * Si NULL → bim_notify() dans SQL ne fait rien.
 *
 * Usage SQL :
 *   SELECT bim_notify('EV_SELECT');
 *
 * Triggers automatiques sur bim_selection :
 *   INSERT/DELETE → publie 'EV_SELECT'
 * ──────────────────────────────────────────────────────────────────── */

typedef void (*BimNotifyCb)(const char *event, void *userdata);

static BimNotifyCb bim__notify_cb = NULL;
static void       *bim__notify_ud = NULL;

void bim_db_set_notify_cb(BimNotifyCb cb, void *ud)
{
    bim__notify_cb = cb;
    bim__notify_ud = ud;
}

static void bim__notify_func(sqlite3_context *ctx, int argc,
                              sqlite3_value **argv)
{
    if (bim__notify_cb && argc > 0) {
        const char *event = (const char*)sqlite3_value_text(argv[0]);
        if (event) bim__notify_cb(event, bim__notify_ud);
    }
    sqlite3_result_null(ctx);
}

void bim_db_register_user_fct(BimDB *db){
    sqlite3_create_function(
    db->handle,               // Handle de la base
    "BIM_DISTANCE",   // Nom de la fonction dans le SQL
    3,                // Nombre d'arguments (mx, my, entity_id)
    SQLITE_UTF8,      // Encodage
    db,               // Contexte (on passe le handle db pour l'utiliser à l'intérieur)
    bim_sql_dist_func,// Pointeur vers la fonction C
    NULL, NULL
);
    sqlite3_create_function(
    db->handle,               // Handle de la base
    "BIM_INSIDE",     // Nom de la fonction dans le SQL
    3,                // Nombre d'arguments (mx, my, entity_id)
    SQLITE_UTF8,      // Encodage
    db,               // Contexte (on passe le handle db pour l'utiliser à l'intérieur)
    bim_sql_inside_func,// Pointeur vers la fonction C
    NULL, NULL
);
}

//prepared statements////////////////////////////////////////////////////////////////////////////////////////

static void bim_db_init_render_statements(BimDB *db) {
    // Requête pour le rendu normal (avec jointure pour les styles)
    const char *sql_all =  
        "SELECT e.id, e.type, v.x, v.y, j.part_id, v.symbol_id, v.rotation, v.scale, "
        "COALESCE(s.stroke_color, ls.stroke_color, 4278190080) AS color, "
        "COALESCE(s.stroke_width, ls.stroke_width, 1) AS width, "
        "COALESCE(s.line_type, ls.line_type, 1) AS line_type "         
        "FROM bim_spatial_index si "
        "JOIN bim_entities e ON si.id = e.id "
        "JOIN bim_layers l ON e.layer_id = l.id "
        "LEFT JOIN bim_styles s ON e.style_id = s.id "
        "LEFT JOIN bim_styles ls ON l.style_id = ls.id "        
        "JOIN bim_geometry_junction j ON e.id = j.entity_id "
        "JOIN bim_vertices v ON j.vertex_id = v.id "
        "WHERE si.minX <= ? AND si.maxX >= ? AND si.minY <= ? AND si.maxY >= ? "
        "AND l.is_visible = 1 "
        "AND l.geom_type = 0 "
        "ORDER BY l.z_order ASC, e.id ASC, j.part_id ASC, j.seq_order ASC;";
    const char *sql_sel = 
        "SELECT e.id, e.type, v.x, v.y, j.part_id, v.symbol_id, v.rotation, v.scale, "
        "4294902169 AS color, "
        "3 AS width "        
        "FROM bim_spatial_index s "
        "JOIN bim_entities e ON s.id = e.id "
        "JOIN bim_geometry_junction j ON e.id = j.entity_id "
        "JOIN bim_vertices v ON j.vertex_id = v.id "
        "JOIN bim_selection z ON z.entity_id = e.id "
        "WHERE s.minX <= ? AND s.maxX >= ? AND s.minY <= ? AND s.maxY >= ? "
        "ORDER BY e.id ASC , j.part_id ASC, j.seq_order ASC;";    
    sqlite3_prepare_v2(db->handle, sql_all, -1, &db->stmt_paint, NULL);
    if (!db->stmt_paint) {
        fprintf(stderr, "Erreur : Statement SQL paint non initialisé.\n");
    }
    sqlite3_prepare_v2(db->handle, sql_sel, -1, &db->stmt_highlight, NULL);
    if (!db->stmt_highlight) {
        fprintf(stderr, "Erreur : Statement SQL highlight non initialisé.\n");
    }    
}

int bim_db_edit_init_stmt(BimDB *db)
{
    const char *sql =
        "SELECT e.id, e.type, v.x, v.y, j.part_id, "
        "       v.symbol_id, v.rotation, v.scale, "
        "       COALESCE(s.stroke_color, ls.stroke_color, 4278190080) AS color, "
        "       COALESCE(s.stroke_width, ls.stroke_width, 1) AS width, "
        "       COALESCE(s.line_type,   ls.line_type,   0) AS line_type "
        "FROM edit_junction j "
        "JOIN bim_entities e  ON j.entity_id  = e.id "
        "JOIN edit_vertices v ON j.vertex_id  = v.id "
        "JOIN bim_layers l    ON e.layer_id   = l.id "
        "LEFT JOIN bim_styles s  ON e.style_id  = s.id "
        "LEFT JOIN bim_styles ls ON l.style_id  = ls.id "
        "ORDER BY e.id ASC, j.part_id ASC, j.seq_order ASC;";

    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &db->stmt_edit, NULL);
    if (rc != SQLITE_OK)
        fprintf(stderr, "bim_db_edit_init_stmt: %s\n",
                sqlite3_errmsg(db->handle));
    return rc;
}


/// Open and Close ////////////////////////////////////////////////////////////////////////////////////////////
static int bim_db_open(BimDB *db, const char *path) {
    int rc = sqlite3_open(path, &db->handle);
    
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Erreur SQLite : %s\n", sqlite3_errmsg(db->handle));
        db->is_open = FALSE;
        return rc;
    }

    db->is_open = TRUE;
    db->db_path = _strdup(path);
    db->stmt_dist_cache = NULL;
    db->stmt_inside_cache = NULL;
    db->stmt_paint = NULL;
    db->stmt_highlight = NULL;
    bim_db_register_user_fct(db);
    // Optimisations pour le mode CAO/Temps réel
    sqlite3_exec(db->handle, "PRAGMA journal_mode = WAL;", NULL, NULL, NULL);
    sqlite3_exec(db->handle, "PRAGMA synchronous = NORMAL;", NULL, NULL, NULL);
    sqlite3_exec(db->handle, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);
    // Initialisation des tables si elles n'existent pas
    bim_db_init_schema(db);    
    const char *sql_init_defaults = 
    "BEGIN;"
    // 1. Créer un style par défaut (noir, épaisseur 1)
    "INSERT OR IGNORE INTO bim_styles (id, name, stroke_color, stroke_width) "
    "VALUES (0, 'Standard', 0, 1);"
    
    // 2. Créer le Layer 0 lié au style 0
    "INSERT OR IGNORE INTO bim_layers (id, name, style_id, is_visible, z_order) "
    "VALUES (0, 'Default', 0, 1, 0);"
    "COMMIT;";

    sqlite3_exec(db->handle, sql_init_defaults, 0, 0, 0);
    bim_db_init_render_statements(db);
    bim_db_edit_init_stmt(db);
    return 0;
}


static void bim_db_close(BimDB *db) {
    if (db->handle) {
        if (db->stmt_dist_cache) {
            sqlite3_finalize(db->stmt_dist_cache);
            db->stmt_dist_cache = NULL;
        }   
        if (db->stmt_inside_cache) {
            sqlite3_finalize(db->stmt_inside_cache);
            db->stmt_inside_cache = NULL;
        }           
        if (db->stmt_paint) {
            sqlite3_finalize(db->stmt_paint);
            db->stmt_paint = NULL;
        }       
        if (db->stmt_highlight) {
            sqlite3_finalize(db->stmt_highlight);
            db->stmt_highlight = NULL;
        }     
        if (db->stmt_edit) {
            sqlite3_finalize(db->stmt_edit);
            db->stmt_edit = NULL;
        }                               
        sqlite3_close(db->handle);
        db->handle = NULL;
    }
    if (db->db_path) free(db->db_path);
    db->is_open = FALSE;
}
#ifdef __cplusplus
}
#endif
#endif