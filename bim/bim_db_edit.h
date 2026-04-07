/*
 * bim_db_edit.h  —  Édition interactive via tables SQLite temporaires
 * ─────────────────────────────────────────────────────────────────────
 *
 *  PRINCIPE
 *  ────────
 *  Plutôt que des structures C complexes en mémoire, l'édition utilise
 *  deux tables SQLite TEMP qui mirrorent les objets sélectionnés.
 *  Le drag/edit opère directement sur ces tables — ultra rapides car
 *  minuscules. Le commit recopie vers les tables permanentes.
 *
 *  FLUX
 *  ────
 *    // 1. Sélectionner un objet → bim_selection est peuplé
 *    bim_db_edit_begin(db);           // copie → edit_vertices / edit_junction
 *
 *    // 2. Drag vertex (WM_MOUSEMOVE)
 *    bim_db_edit_move_vertex(db, vertex_id, new_x, new_y);
 *
 *    // 3a. Valider (relâcher souris / touche Entrée)
 *    bim_db_edit_commit(db);          // edit_vertices → bim_vertices
 *
 *    // 3b. Annuler (Echap)
 *    bim_db_edit_rollback(db);        // DROP tables TEMP — rien ne change
 *
 *  RENDU
 *  ─────
 *  Initialiser stmt_edit via bim_db_edit_init_stmt(db) au démarrage.
 *  Pendant l'édition, appeler bim_db_render_edit(db, draw_cb, user_data)
 *  après le rendu normal pour dessiner les objets édités.
 *  Les handles (cercles sur vertices) sont dessinés séparément par
 *  bim_db_edit_load_handles(db, handles, max, count).
 *
 *  UTILISATION
 *  ───────────
 *    #define BIM_DB_EDIT_IMPLEMENTATION
 *    #include "bim_db_edit.h"
 *
 *  DÉPENDANCES
 *  ───────────
 *    bim_db_schema.h  — BimDB, BimStyle, BimObject, BimObjRenderCallBack
 *    sqlite3
 *
 * ─────────────────────────────────────────────────────────────────────
 */

#ifndef BIM_DB_EDIT_H
#define BIM_DB_EDIT_H

#include <sqlite3.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>


/* ── API publique ────────────────────────────────────────────────── */

/* Initialiser le statement de rendu édition — appeler au démarrage   */
int  bim_db_edit_init_stmt    (BimDB *db);

/* Entrée en mode édition — copie bim_selection → tables TEMP         */
int  bim_db_edit_begin        (BimDB *db);

/* Déplacer un vertex pendant le drag                                  */
int  bim_db_edit_move_vertex  (BimDB *db, int vertex_id,
                                double x, double y);

/* Déplacer tous les vertices d'une entité (drag nœud partagé)        */
int  bim_db_edit_move_entity  (BimDB *db, int entity_id,
                                double dx, double dy);

/* Valider — recopie edit_vertices → bim_vertices + màj RTree         */
int  bim_db_edit_commit       (BimDB *db);

/* Annuler — DROP tables TEMP, rien ne change en DB permanente        */
int  bim_db_edit_rollback     (BimDB *db);

/* Charger les handles pour le rendu Cairo                             */
int  bim_db_edit_load_handles (BimDB *db,
                                BimEditHandle *handles,
                                int max_handles,
                                int *out_count);

/* Rendu des objets en édition — même signature que render_viewport   */
void bim_db_render_edit       (BimDB *db,
                                BimObjRenderCallBack draw,
                                void *user_data);

/* ═══════════════════════════════════════════════════════════════════
   SECTION IMPLÉMENTATION
   ═══════════════════════════════════════════════════════════════════ */
#ifdef BIM_DB_EDIT_IMPLEMENTATION

/* ── Statement rendu édition ─────────────────────────────────────── */

/* ── Entrée en mode édition ──────────────────────────────────────── */
int bim_db_edit_begin(BimDB *db)
{
    const char *sql =
        "DROP TABLE IF EXISTS edit_vertices; "
        "DROP TABLE IF EXISTS edit_junction; "

        /* Tous les vertices des entités sélectionnées — DISTINCT      *
         * car un vertex peut être partagé entre plusieurs entités      */
        "CREATE TEMP TABLE edit_vertices AS "
        "    SELECT DISTINCT v.* "
        "    FROM bim_vertices v "
        "    JOIN bim_geometry_junction j ON v.id  = j.vertex_id "
        "    JOIN bim_selection         s ON j.entity_id = s.entity_id; "

        /* Toute la jonction des entités sélectionnées                  */
        "CREATE TEMP TABLE edit_junction AS "
        "    SELECT j.* "
        "    FROM bim_geometry_junction j "
        "    JOIN bim_selection s ON j.entity_id = s.entity_id; ";

    char *err = NULL;
    int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "bim_db_edit_begin: %s\n", err);
        sqlite3_free(err);
    }

    /* Réinitialiser le statement de rendu édition après CREATE TEMP   */
    if (db->stmt_edit) {
        sqlite3_finalize(db->stmt_edit);
        db->stmt_edit = NULL;
    }
    bim_db_edit_init_stmt(db);

    return rc;
}

/* ── Déplacer un vertex ──────────────────────────────────────────── */
int bim_db_edit_move_vertex(BimDB *db, int vertex_id, double x, double y)
{
    const char *sql = "UPDATE edit_vertices SET x=?, y=? WHERE id=?;";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, x);
        sqlite3_bind_double(stmt, 2, y);
        sqlite3_bind_int   (stmt, 3, vertex_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    return rc;
}

/* ── Déplacer tous les vertices d'une entité (delta) ────────────── */
int bim_db_edit_move_entity(BimDB *db, int entity_id, double dx, double dy)
{
    const char *sql =
        "UPDATE edit_vertices SET x = x + ?1, y = y + ?2 "
        "WHERE id IN ("
        "    SELECT vertex_id FROM edit_junction WHERE entity_id = ?3"
        ");";
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_double(stmt, 1, dx);
        sqlite3_bind_double(stmt, 2, dy);
        sqlite3_bind_int   (stmt, 3, entity_id);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    return rc;
}

/* ── Commit — recopie vers tables permanentes + màj RTree ───────── */
int bim_db_edit_commit(BimDB *db)
{
    /* Mettre à jour bim_vertices depuis edit_vertices */
    const char *sql_update =
        "UPDATE bim_vertices "
        "SET x = (SELECT x FROM edit_vertices WHERE edit_vertices.id = bim_vertices.id), "
        "    y = (SELECT y FROM edit_vertices WHERE edit_vertices.id = bim_vertices.id) "
        "WHERE id IN (SELECT id FROM edit_vertices);";

    /* Recalculer le RTree pour les entités modifiées */
    const char *sql_rtree =
        "UPDATE bim_spatial_index "
        "SET minX = (SELECT MIN(v.x) FROM edit_vertices v "
        "            JOIN edit_junction j ON v.id = j.vertex_id "
        "            WHERE j.entity_id = bim_spatial_index.id), "
        "    maxX = (SELECT MAX(v.x) FROM edit_vertices v "
        "            JOIN edit_junction j ON v.id = j.vertex_id "
        "            WHERE j.entity_id = bim_spatial_index.id), "
        "    minY = (SELECT MIN(v.y) FROM edit_vertices v "
        "            JOIN edit_junction j ON v.id = j.vertex_id "
        "            WHERE j.entity_id = bim_spatial_index.id), "
        "    maxY = (SELECT MAX(v.y) FROM edit_vertices v "
        "            JOIN edit_junction j ON v.id = j.vertex_id "
        "            WHERE j.entity_id = bim_spatial_index.id) "
        "WHERE id IN (SELECT DISTINCT entity_id FROM edit_junction);";

    const char *sql_drop =
        "DROP TABLE IF EXISTS edit_vertices; "
        "DROP TABLE IF EXISTS edit_junction; ";

    char *err = NULL;
    int rc;

    rc = sqlite3_exec(db->handle, sql_update, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "bim_db_edit_commit update: %s\n", err);
        sqlite3_free(err);
        return rc;
    }

    rc = sqlite3_exec(db->handle, sql_rtree, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "bim_db_edit_commit rtree: %s\n", err);
        sqlite3_free(err);
        return rc;
    }

    sqlite3_exec(db->handle, sql_drop, NULL, NULL, NULL);

    /* Invalider le statement de rendu édition */
    if (db->stmt_edit) {
        sqlite3_finalize(db->stmt_edit);
        db->stmt_edit = NULL;
    }

    return SQLITE_OK;
}

/* ── Rollback — DROP tables TEMP, rien ne change ────────────────── */
int bim_db_edit_rollback(BimDB *db)
{
    const char *sql =
        "DROP TABLE IF EXISTS edit_vertices; "
        "DROP TABLE IF EXISTS edit_junction; ";

    char *err = NULL;
    sqlite3_exec(db->handle, sql, NULL, NULL, &err);
    sqlite3_free(err);

    if (db->stmt_edit) {
        sqlite3_finalize(db->stmt_edit);
        db->stmt_edit = NULL;
    }

    return SQLITE_OK;
}

/* ── Charger les handles pour le rendu Cairo ─────────────────────── */
int bim_db_edit_load_handles(BimDB *db,
                              BimEditHandle *handles,
                              int max_handles,
                              int *out_count)
{
    *out_count = 0;
    const char *sql =
        "SELECT v.id, v.x, v.y, v.is_node, j.entity_id, j.seq_order "
        "FROM edit_vertices v "
        "JOIN edit_junction j ON v.id = j.vertex_id "
        "ORDER BY j.entity_id, j.seq_order;";

    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return rc;

    while (sqlite3_step(stmt) == SQLITE_ROW && *out_count < max_handles) {
        BimEditHandle *h = &handles[*out_count];
        h->vertex_id = sqlite3_column_int   (stmt, 0);
        h->x         = sqlite3_column_double(stmt, 1);
        h->y         = sqlite3_column_double(stmt, 2);
        h->is_node   = sqlite3_column_int   (stmt, 3);
        h->entity_id = sqlite3_column_int   (stmt, 4);
        h->seq_order = sqlite3_column_int   (stmt, 5);
        (*out_count)++;
    }
    sqlite3_finalize(stmt);
    return SQLITE_OK;
}

/* ── Rendu des objets en édition ─────────────────────────────────── */
void bim_db_render_edit(BimDB *db,
                         BimObjRenderCallBack draw,
                         void *user_data)
{
    if (!db->stmt_edit) return;

    sqlite3_reset(db->stmt_edit);

    BimObject   obj      = {0};
    BimStyle    style    = {0};
    int         cur_id   = -1;
    int         cur_part = -1;

    while (sqlite3_step(db->stmt_edit) == SQLITE_ROW) {
        int    eid      = sqlite3_column_int   (db->stmt_edit, 0);
        int    type     = sqlite3_column_int   (db->stmt_edit, 1);
        double x        = sqlite3_column_double(db->stmt_edit, 2);
        double y        = sqlite3_column_double(db->stmt_edit, 3);
        int    part_id  = sqlite3_column_int   (db->stmt_edit, 4);
        int    sym_id   = sqlite3_column_int   (db->stmt_edit, 5);
        double rotation = sqlite3_column_double(db->stmt_edit, 6);
        double scale    = sqlite3_column_double(db->stmt_edit, 7);

        style.stroke_color = sqlite3_column_int(db->stmt_edit, 8);
        style.stroke_width = sqlite3_column_int(db->stmt_edit, 9);
        style.line_type    = sqlite3_column_int(db->stmt_edit, 10);

        /* Nouvelle entité — flusher la précédente */
        if (eid != cur_id) {
            if (cur_id >= 0 && obj.poly.count > 0)
                draw(0, &style, false, &obj, user_data);

            obj.type       = (ObjType)type;
            obj.symbol_id  = sym_id;
            obj.rotation   = rotation;
            obj.scale      = scale;
            obj.poly.count = 0;
            cur_id         = eid;
            cur_part       = part_id;
        }

        /* Nouveau part — marquer la rupture */
        if (part_id != cur_part && obj.poly.count > 0) {
            /* Stocker part_id pour que draw_cb puisse segmenter */
            cur_part = part_id;
        }

        if (obj.poly.count < MAX_POLY_PTS) {
            obj.poly.vertices[obj.poly.count].x  = x;
            obj.poly.vertices[obj.poly.count].y  = y;
            obj.poly.part_ids[obj.poly.count]    = part_id;
            obj.poly.count++;
        }

        /* Pour un NODE — stocker sx/sy */
        if (type == 1 /* NODE */) {
            obj.sx = x;
            obj.sy = y;
        }
    }

    /* Flusher le dernier objet */
    if (cur_id >= 0 && obj.poly.count > 0)
        draw(0, &style, false, &obj, user_data);
}

#endif /* BIM_DB_EDIT_IMPLEMENTATION */
#endif /* BIM_DB_EDIT_H */
