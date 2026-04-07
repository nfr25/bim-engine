
// ── Icône Polyligne ─────────────────────────────────────
void icon_polyline(cairo_t *cr, double cx, double cy, int mode)
{
    double pts[][2] = {
        {cx-9, cy+6}, {cx-4, cy-6}, {cx+1, cy+2}, {cx+6, cy-5}, {cx+9, cy+4}
    };
    cairo_set_line_width(cr, 1.8);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_set_source_rgb(cr, 0.3, 0.7, 1.0);
    cairo_move_to(cr, pts[0][0], pts[0][1]);
    for (int i = 1; i < 5; i++)
        cairo_line_to(cr, pts[i][0], pts[i][1]);
    cairo_stroke(cr);
    /* Points de sommet */
    for (int i = 0; i < 5; i++) {
        cairo_arc(cr, pts[i][0], pts[i][1], 2.0, 0, 2*M_PI);
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        cairo_fill(cr);
    }
}

// ── Icône Polygone / Surface ─────────────────────────────────────
void icon_polygon(cairo_t *cr, double cx, double cy, int mode)
{
    /* Hexagone irrégulier façon "surface topologique" */
    double pts[][2] = {
        {cx-7, cy-6}, {cx+4, cy-7}, {cx+9, cy},
        {cx+5, cy+7}, {cx-4, cy+6}, {cx-9, cy+1}
    };
    cairo_move_to(cr, pts[0][0], pts[0][1]);
    for (int i = 1; i < 6; i++)
        cairo_line_to(cr, pts[i][0], pts[i][1]);
    cairo_close_path(cr);
    /* Remplissage semi-transparent */
    cairo_set_source_rgba(cr, 0.3, 0.7, 1.0, 0.25);
    cairo_fill_preserve(cr);
    /* Contour */
    cairo_set_source_rgb(cr, 0.3, 0.7, 1.0);
    cairo_set_line_width(cr, 1.8);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    cairo_stroke(cr);
}

// ── Icône Nœud / Point ──────────────────────────────────────────
void icon_node(cairo_t *cr, double cx, double cy, int mode)
{
    /* Croix + cercle */
    cairo_set_line_width(cr, 1.5);
    cairo_set_source_rgb(cr, 0.3, 0.7, 1.0);
    cairo_move_to(cr, cx-8, cy); cairo_line_to(cr, cx+8, cy);
    cairo_move_to(cr, cx, cy-8); cairo_line_to(cr, cx, cy+8);
    cairo_stroke(cr);
    cairo_arc(cr, cx, cy, 4.0, 0, 2*M_PI);
    cairo_set_source_rgba(cr, 0.3, 0.7, 1.0, 0.30);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 0.3, 0.7, 1.0);
    cairo_set_line_width(cr, 1.5);
    cairo_stroke(cr);
}

// ── Icône Sélection ─────────────────────────────────────────────
void icon_select(cairo_t *cr, double cx, double cy, int mode)
{
    /* Rectangle pointillé de sélection */
    double dash[] = {3.0, 2.0};
    cairo_set_dash(cr, dash, 2, 0);
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, cx-8, cy-7, 16, 14);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
    /* Flèche curseur */
    cairo_set_source_rgb(cr, 1.0, 0.85, 0.0);
    cairo_move_to(cr, cx-8, cy-7);
    cairo_line_to(cr, cx-8, cy+2);
    cairo_line_to(cr, cx-5, cy-1);
    cairo_line_to(cr, cx-3, cy+4);
    cairo_set_line_width(cr, 1.8);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_stroke(cr);
}

void icon_zoom_plus(cairo_t *cr, double cx, double cy, int mode)
{
    /* Loupe */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.8);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_arc(cr, cx-2, cy-2, 6, 0, 2*M_PI);
    cairo_stroke(cr);
    /* Manche */
    cairo_move_to(cr, cx+3, cy+3);
    cairo_line_to(cr, cx+8, cy+8);
    cairo_stroke(cr);
    /* + */
    cairo_set_source_rgb(cr, 0.3, 0.8, 0.3);
    cairo_set_line_width(cr, 1.8);
    cairo_move_to(cr, cx-2, cy-5);
    cairo_line_to(cr, cx-2, cy+1);
    cairo_move_to(cr, cx-5, cy-2);
    cairo_line_to(cr, cx+1, cy-2);
    cairo_stroke(cr);
}

void icon_zoom_moins(cairo_t *cr, double cx, double cy, int mode)
{
    /* Loupe */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.8);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_arc(cr, cx-2, cy-2, 6, 0, 2*M_PI);
    cairo_stroke(cr);
    /* Manche */
    cairo_move_to(cr, cx+3, cy+3);
    cairo_line_to(cr, cx+8, cy+8);
    cairo_stroke(cr);
    /* - */
    cairo_set_source_rgb(cr, 0.9, 0.4, 0.3);
    cairo_set_line_width(cr, 1.8);
    cairo_move_to(cr, cx-5, cy-2);
    cairo_line_to(cr, cx+1, cy-2);
    cairo_stroke(cr);
}

void icon_zoom_extent(cairo_t *cr, double cx, double cy, int mode)
{
    /* Carré monde */
    cairo_set_source_rgb(cr, 0.5, 0.7, 1.0);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, cx-7, cy-7, 14, 14);
    cairo_stroke(cr);
    /* 4 flèches vers les coins */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    /* coin haut-gauche */
    cairo_move_to(cr, cx-3, cy-7); cairo_line_to(cr, cx-7, cy-7); cairo_line_to(cr, cx-7, cy-3);
    /* coin haut-droit */
    cairo_move_to(cr, cx+3, cy-7); cairo_line_to(cr, cx+7, cy-7); cairo_line_to(cr, cx+7, cy-3);
    /* coin bas-gauche */
    cairo_move_to(cr, cx-3, cy+7); cairo_line_to(cr, cx-7, cy+7); cairo_line_to(cr, cx-7, cy+3);
    /* coin bas-droit */
    cairo_move_to(cr, cx+3, cy+7); cairo_line_to(cr, cx+7, cy+7); cairo_line_to(cr, cx+7, cy+3);
    cairo_stroke(cr);
}

void icon_zoom_rectangle(cairo_t *cr, double cx, double cy, int mode)
{
    /* Rectangle pointillé de zoom */
    double dash[] = {3.0, 2.0};
    cairo_set_dash(cr, dash, 2, 0);
    cairo_set_source_rgb(cr, 0.3, 0.8, 1.0);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, cx-7, cy-5, 14, 10);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
    /* Loupe petite coin bas-droit */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_arc(cr, cx+4, cy+4, 3, 0, 2*M_PI);
    cairo_stroke(cr);
    cairo_move_to(cr, cx+6, cy+6);
    cairo_line_to(cr, cx+8, cy+8);
    cairo_stroke(cr);
}

void icon_add_layer(cairo_t *cr, double cx, double cy, int mode)
{
    /* 2 rectangles empilés — couches */
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    /* couche arrière */
    cairo_set_source_rgb(cr, 0.5, 0.6, 0.8);
    cairo_rectangle(cr, cx-5, cy-1, 11, 7);
    cairo_stroke(cr);
    /* couche avant */
    cairo_set_source_rgb(cr, 0.8, 0.85, 1.0);
    cairo_rectangle(cr, cx-7, cy-7, 11, 7);
    cairo_stroke(cr);
    /* + vert */
    cairo_set_source_rgb(cr, 0.3, 0.9, 0.3);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, cx+5, cy-4);
    cairo_line_to(cr, cx+5, cy+2);
    cairo_move_to(cr, cx+2, cy-1);
    cairo_line_to(cr, cx+8, cy-1);
    cairo_stroke(cr);
}

void icon_del_layer(cairo_t *cr, double cx, double cy, int mode)
{
    /* 2 rectangles empilés — couches */
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    /* couche arrière */
    cairo_set_source_rgb(cr, 0.5, 0.6, 0.8);
    cairo_rectangle(cr, cx-5, cy-1, 11, 7);
    cairo_stroke(cr);
    /* couche avant */
    cairo_set_source_rgb(cr, 0.8, 0.85, 1.0);
    cairo_rectangle(cr, cx-7, cy-7, 11, 7);
    cairo_stroke(cr);
    /* x rouge */
    cairo_set_source_rgb(cr, 0.9, 0.3, 0.3);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, cx+3, cy-4);
    cairo_line_to(cr, cx+8, cy+1);
    cairo_move_to(cr, cx+8, cy-4);
    cairo_line_to(cr, cx+3, cy+1);
    cairo_stroke(cr);
}

void icon_edit_style(cairo_t *cr, double cx, double cy, int mode)
{
    /* Pinceau */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.8);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);
    /* Manche */
    cairo_move_to(cr, cx-6, cy-7);
    cairo_line_to(cr, cx+2, cy+1);
    cairo_stroke(cr);
    /* Pointe pinceau */
    cairo_set_source_rgb(cr, 0.3, 0.70, 1.0);
    cairo_move_to(cr, cx+2, cy+1);
    cairo_line_to(cr, cx+5, cy+2);
    cairo_line_to(cr, cx+6, cy+5);
    cairo_line_to(cr, cx+3, cy+6);
    cairo_line_to(cr, cx+2, cy+1);
    cairo_fill(cr);
    /* Palette — 3 points couleur */
    cairo_arc(cr, cx-5, cy+4, 1.5, 0, 2*M_PI);
    cairo_set_source_rgb(cr, 0.9, 0.4, 0.4);
    cairo_fill(cr);
    cairo_arc(cr, cx-1, cy+6, 1.5, 0, 2*M_PI);
    cairo_set_source_rgb(cr, 0.4, 0.9, 0.4);
    cairo_fill(cr);
    cairo_arc(cr, cx+3, cy+8, 1.5, 0, 2*M_PI);
    cairo_set_source_rgb(cr, 0.4, 0.4, 0.9);
    cairo_fill(cr);
}

void icon_grid(cairo_t *cr, double cx, double cy, int mode)
{
    cairo_set_source_rgb(cr, 0.55, 0.65, 0.85);
    cairo_set_line_width(cr, 1.2);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    /* Lignes verticales */
    for (int i = -1; i <= 1; i++) {
        cairo_move_to(cr, cx + i*6, cy-7);
        cairo_line_to(cr, cx + i*6, cy+7);
    }
    /* Lignes horizontales */
    for (int i = -1; i <= 1; i++) {
        cairo_move_to(cr, cx-7, cy + i*6);
        cairo_line_to(cr, cx+7, cy + i*6);
    }
    cairo_stroke(cr);

    /* Points d'intersection accentués */
    cairo_set_source_rgb(cr, 0.30, 0.70, 1.00);
    for (int x = -1; x <= 1; x++) {
        for (int y = -1; y <= 1; y++) {
            cairo_arc(cr, cx + x*6, cy + y*6, 1.2, 0, 2*M_PI);
            cairo_fill(cr);
        }
    }
}

void icon_snap(cairo_t *cr, double cx, double cy, int mode)
{
    /* Croix d'accrochage */
    cairo_set_source_rgb(cr, 0.30, 0.70, 1.00);
    cairo_set_line_width(cr, 1.5);
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_move_to(cr, cx-7, cy); cairo_line_to(cr, cx+7, cy);
    cairo_move_to(cr, cx, cy-7); cairo_line_to(cr, cx, cy+7);
    cairo_stroke(cr);

    /* Carré d'accrochage central */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.5);
    cairo_rectangle(cr, cx-3.5, cy-3.5, 7, 7);
    cairo_stroke(cr);

    /* Point central */
    cairo_set_source_rgb(cr, 0.30, 0.70, 1.00);
    cairo_arc(cr, cx, cy, 1.5, 0, 2*M_PI);
    cairo_fill(cr);

    /* Tirets aux extrémités — indique l'accrochage actif */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.5);
    double dash[] = {2.0, 2.0};
    cairo_set_dash(cr, dash, 2, 0);
    cairo_move_to(cr, cx-7, cy-4); cairo_line_to(cr, cx-7, cy+4);
    cairo_move_to(cr, cx+7, cy-4); cairo_line_to(cr, cx+7, cy+4);
    cairo_move_to(cr, cx-4, cy-7); cairo_line_to(cr, cx+4, cy-7);
    cairo_move_to(cr, cx-4, cy+7); cairo_line_to(cr, cx+4, cy+7);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);
}

/* ── Zoom Extent — loupe avec 4 coins ───────────────────────────── */
static void icon_zoom_extent2(cairo_t *cr, double cx, double cy)
{
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    cairo_set_line_join(cr, CAIRO_LINE_JOIN_ROUND);

    /* Loupe */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.8);
    cairo_arc(cr, cx-2, cy-2, 6, 0, 2*M_PI);
    cairo_stroke(cr);

    /* Manche */
    cairo_move_to(cr, cx+3, cy+3);
    cairo_line_to(cr, cx+8, cy+8);
    cairo_stroke(cr);

    /* 4 flèches vers les coins — intérieur de la loupe */
    cairo_set_source_rgb(cr, 0.30, 0.70, 1.00);
    cairo_set_line_width(cr, 1.2);

    /* coin haut-gauche */
    cairo_move_to(cr, cx-2, cy-5); cairo_line_to(cr, cx-5, cy-5);
    cairo_line_to(cr, cx-5, cy-2);
    /* coin haut-droit */
    cairo_move_to(cr, cx+1, cy-5); cairo_line_to(cr, cx+3, cy-5);
    cairo_line_to(cr, cx+3, cy-2);
    /* coin bas-gauche */
    cairo_move_to(cr, cx-2, cy+1); cairo_line_to(cr, cx-5, cy+1);
    cairo_line_to(cr, cx-5, cy-1);
    /* coin bas-droit */
    cairo_move_to(cr, cx+1, cy+1); cairo_line_to(cr, cx+3, cy+1);
    cairo_line_to(cr, cx+3, cy-1);
    cairo_stroke(cr);
}

/* ── Zoom Window — loupe avec rectangle pointillé ───────────────── */
static void icon_zoom_window(cairo_t *cr, double cx, double cy)
{
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    /* Rectangle pointillé de sélection */
    double dash[] = {3.0, 2.0};
    cairo_set_dash(cr, dash, 2, 0);
    cairo_set_source_rgb(cr, 0.30, 0.70, 1.00);
    cairo_set_line_width(cr, 1.4);
    cairo_rectangle(cr, cx-8, cy-6, 11, 9);
    cairo_stroke(cr);
    cairo_set_dash(cr, NULL, 0, 0);

    /* Loupe petite coin bas-droit */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.6);
    cairo_arc(cr, cx+4, cy+4, 3.5, 0, 2*M_PI);
    cairo_stroke(cr);

    /* Manche */
    cairo_move_to(cr, cx+6.5, cy+6.5);
    cairo_line_to(cr, cx+8.5, cy+8.5);
    cairo_stroke(cr);
}

/* ── Zoom Selection — loupe avec étoile/highlight ───────────────── */
static void icon_zoom_selection(cairo_t *cr, double cx, double cy)
{
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

    /* Loupe */
    cairo_set_source_rgb(cr, 0.9, 0.9, 0.9);
    cairo_set_line_width(cr, 1.8);
    cairo_arc(cr, cx-2, cy-2, 6, 0, 2*M_PI);
    cairo_stroke(cr);

    /* Manche */
    cairo_move_to(cr, cx+3, cy+3);
    cairo_line_to(cr, cx+8, cy+8);
    cairo_stroke(cr);

    /* Objet sélectionné — petit carré jaune dans la loupe */
    cairo_set_source_rgba(cr, 1.0, 0.85, 0.0, 0.85);
    cairo_rectangle(cr, cx-5, cy-5, 6, 6);
    cairo_fill_preserve(cr);
    cairo_set_source_rgb(cr, 1.0, 0.85, 0.0);
    cairo_set_line_width(cr, 1.2);
    cairo_stroke(cr);
}

/* ── Fonction multimode — dispatcher ────────────────────────────── */
static void icon_zoom(cairo_t *cr, double cx, double cy, int mode)
{
    switch (mode) {
    case 0: icon_zoom_extent2   (cr, cx, cy); break;
    case 1: icon_zoom_window   (cr, cx, cy); break;
    case 2: icon_zoom_selection(cr, cx, cy); break;
    default: icon_zoom_extent2  (cr, cx, cy); break;
    }
}
