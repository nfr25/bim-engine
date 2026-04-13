/*
 * bim_db_ports.h — Gestion des ports de connexion des nodes
 * ──────────────────────────────────────────────────────────────────
 *
 *  CONCEPT
 *  ───────
 *  Chaque node (vertex avec symbol_id) a 8 ports de connexion
 *  potentiels disposés autour de lui (N, NE, E, SE, S, SW, W, NW).
 *
 *  Le bitfield est stocké à deux niveaux :
 *   - bim_svg_library.ports_mask  : défaut du type de symbole
 *   - bim_vertices.ports_active   : override par instance de node
 *
 *  À la création d'un node, ports_active est initialisé avec
 *  ports_mask du symbole (fait dans bim_db_entity.h).
 *
 *  CONSTANTES (définies dans bim_db_schema.h)
 *  ──────────
 *   BIM_PORT_N   0x01    BIM_PORT_NE  0x02
 *   BIM_PORT_E   0x04    BIM_PORT_SE  0x08
 *   BIM_PORT_S   0x10    BIM_PORT_SW  0x20
 *   BIM_PORT_W   0x40    BIM_PORT_NW  0x80
 *   BIM_PORT_ALL 0xFF    BIM_PORT_NONE 0x00
 *
 *  UTILISATION
 *  ───────────
 *   // Lire les ports actifs d'un node
 *   int ports = bim_db_get_node_ports(db, vertex_id);
 *
 *   // Modifier les ports d'un node
 *   bim_db_set_node_ports(db, vertex_id, BIM_PORT_EW);
 *
 *   // Lire/écrire le défaut d'un symbole
 *   int ports = bim_db_get_symbol_ports(db, symbol_id);
 *   bim_db_set_symbol_ports(db, symbol_id, BIM_PORT_4);
 *
 *   // Calculer la position XY d'un port (pour snapping)
 *   double px, py;
 *   bim_db_port_position(cx, cy, radius, BIM_PORT_NE, &px, &py);
 *
 * ──────────────────────────────────────────────────────────────────
 */

#ifndef BIM_DB_PORTS_H
#define BIM_DB_PORTS_H

#include <sqlite3.h>
#include <math.h>
#include "bim_db_schema.h"   /* pour BIM_PORT_* */
#include "bim_db_types.h"

/* ── Ports d'un node (instance) ──────────────────────────────────── */

/* Retourne le bitfield ports_active du vertex.
 * Retourne BIM_PORT_ALL si le vertex n'existe pas ou n'est pas un node. */
static int bim_db_get_node_ports(BimDB *db, sqlite3_int64 vertex_id)
{
    sqlite3_stmt *stmt;
    int ports = BIM_PORT_ALL;

    const char *sql =
        "SELECT ports_active FROM bim_vertices WHERE id = ? AND is_node = 1";

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ports;

    sqlite3_bind_int64(stmt, 1, vertex_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
            ports = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return ports;
}

/* Modifie le bitfield ports_active d'un node (override par instance). */
static void bim_db_set_node_ports(BimDB *db, sqlite3_int64 vertex_id,
                                   int ports_mask)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "UPDATE bim_vertices SET ports_active = ? WHERE id = ? AND is_node = 1";

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return;

    sqlite3_bind_int  (stmt, 1, ports_mask);
    sqlite3_bind_int64(stmt, 2, vertex_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/* Active un port supplémentaire sur un node (OR). */
static void bim_db_enable_port(BimDB *db, sqlite3_int64 vertex_id, int port)
{
    int current = bim_db_get_node_ports(db, vertex_id);
    bim_db_set_node_ports(db, vertex_id, current | port);
}

/* Désactive un port sur un node (AND NOT). */
static void bim_db_disable_port(BimDB *db, sqlite3_int64 vertex_id, int port)
{
    int current = bim_db_get_node_ports(db, vertex_id);
    bim_db_set_node_ports(db, vertex_id, current & ~port);
}

/* Teste si un port est actif sur un node. */
static int bim_db_port_active(BimDB *db, sqlite3_int64 vertex_id, int port)
{
    return (bim_db_get_node_ports(db, vertex_id) & port) != 0;
}

/* ── Ports d'un symbole (défaut du type) ─────────────────────────── */

/* Retourne le ports_mask par défaut d'un symbole. */
static int bim_db_get_symbol_ports(BimDB *db, int symbol_id)
{
    sqlite3_stmt *stmt;
    int ports = BIM_PORT_ALL;

    const char *sql =
        "SELECT ports_mask FROM bim_svg_library WHERE id = ?";

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return ports;

    sqlite3_bind_int(stmt, 1, symbol_id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_type(stmt, 0) != SQLITE_NULL)
            ports = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);
    return ports;
}

/* Modifie le ports_mask par défaut d'un symbole. */
static void bim_db_set_symbol_ports(BimDB *db, int symbol_id, int ports_mask)
{
    sqlite3_stmt *stmt;
    const char *sql =
        "UPDATE bim_svg_library SET ports_mask = ? WHERE id = ?";

    if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL) != SQLITE_OK)
        return;

    sqlite3_bind_int(stmt, 1, ports_mask);
    sqlite3_bind_int(stmt, 2, symbol_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

/* ── Géométrie des ports ──────────────────────────────────────────── */

/* Angles des 8 ports en radians (0 = droite = E, sens trigonométrique) */
static const double BIM_PORT_ANGLES[8] = {
    -M_PI / 2.0,          /* N   — 0x01 — 270° */
    -M_PI / 4.0,          /* NE  — 0x02 — 315° */
     0.0,                 /* E   — 0x04 —   0° */
     M_PI / 4.0,          /* SE  — 0x08 —  45° */
     M_PI / 2.0,          /* S   — 0x10 —  90° */
     3.0 * M_PI / 4.0,   /* SW  — 0x20 — 135° */
     M_PI,                /* W   — 0x40 — 180° */
    -3.0 * M_PI / 4.0,   /* NW  — 0x80 — 225° */
};

/* Retourne le bit correspondant à l'index (0=N, 1=NE, ..., 7=NW). */
static int bim_port_bit(int index) { return 1 << index; }

/* Calcule la position XY d'un port autour d'un node.
 *   cx, cy   : centre du node en coordonnées monde
 *   radius   : rayon du symbole (demi-taille) en coordonnées monde
 *   port_bit : BIM_PORT_N, BIM_PORT_E, etc.
 *   px, py   : position du port (sortie)
 */
static void bim_port_position(double cx, double cy, double radius,
                               int port_bit,
                               double *px, double *py)
{
    /* trouver l'index du port */
    int idx = 0;
    int bit = port_bit;
    while (bit > 1 && idx < 7) { bit >>= 1; idx++; }

    double angle = BIM_PORT_ANGLES[idx];
    *px = cx + radius * cos(angle);
    *py = cy + radius * sin(angle);
}

/* Trouve le port le plus proche d'un point (mx, my) autour d'un node.
 * Retourne le bit du port le plus proche parmi les ports actifs,
 * ou BIM_PORT_NONE si aucun port actif n'est dans la tolérance.
 *
 *   cx, cy     : centre du node
 *   radius     : rayon du symbole
 *   ports_mask : bitfield des ports actifs
 *   mx, my     : point à tester
 *   tolerance  : distance max pour considérer un hit
 */
static int bim_port_hit(double cx, double cy, double radius,
                         int ports_mask,
                         double mx, double my,
                         double tolerance)
{
    double best_dist = tolerance * tolerance;
    int    best_port = BIM_PORT_NONE;

    for (int i = 0; i < 8; i++) {
        int bit = 1 << i;
        if (!(ports_mask & bit)) continue;   /* port inactif */

        double px, py;
        bim_port_position(cx, cy, radius, bit, &px, &py);

        double dx = mx - px;
        double dy = my - py;
        double d2 = dx * dx + dy * dy;

        if (d2 < best_dist) {
            best_dist = d2;
            best_port = bit;
        }
    }

    return best_port;
}

#endif /* BIM_DB_PORTS_H */
