/*
 * icons_zoom.h  —  Icône Cairo pour bouton zoom multimode
 * ────────────────────────────────────────────────────────
 *  Mode 0 : Zoom Extent    — tout afficher
 *  Mode 1 : Zoom Window    — rectangle de zoom
 *  Mode 2 : Zoom Selection — zoom sur la sélection
 *
 *  Utilisation :
 *    cl_toolbar_add_icon_multimode(layout, ID_ZOOM, icon_zoom, "Zoom", 3);
 *
 *    // Dans le callback :
 *    case ID_ZOOM: {
 *        int mode = cl_toolbar_get_mode(layout, ID_ZOOM);
 *        switch (mode) {
 *        case 0: canvas_zoom_extent(cv);    break;
 *        case 1: canvas_set_mode(cv, ZOOM_WINDOW);    break;
 *        case 2: canvas_zoom_selection(cv); break;
 *        }
 *        break;
 *    }
 */

#ifndef ICONS_ZOOM_H
#define ICONS_ZOOM_H

#include <cairo/cairo.h>
#include <math.h>

/* ── Zoom Extent — loupe avec 4 coins ───────────────────────────── */
static void icon_zoom_extent(cairo_t *cr, double cx, double cy)
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
    case 0: icon_zoom_extent   (cr, cx, cy); break;
    case 1: icon_zoom_window   (cr, cx, cy); break;
    case 2: icon_zoom_selection(cr, cx, cy); break;
    default: icon_zoom_extent  (cr, cx, cy); break;
    }
}

#endif /* ICONS_ZOOM_H */
