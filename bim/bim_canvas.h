#ifdef RC_INVOKED
#include <winuser.h>

1 24 {
    "<?xml version=""1.0"" encoding=""UTF-8"" standalone=""yes""?>"
    "<assembly xmlns=""urn:schemas-microsoft-com:asm.v1"" manifestVersion=""1.0"">"
    "<asmv3:application xmlns:asmv3=""urn:schemas-microsoft-com:asm.v3"">"
        "<asmv3:windowsSettings>"
            "<dpiAware xmlns=""http://schemas.microsoft.com/SMI/2005/WindowsSettings"">true</dpiAware>"
            "<dpiAwareness xmlns=""http://schemas.microsoft.com/SMI/2016/WindowsSettings"">PerMonitorV2</dpiAwareness>"
        "</asmv3:windowsSettings>"
    "</asmv3:application>"
    "<trustInfo xmlns=""urn:schemas-microsoft-com:asm.v3"">"
        "<security><requestedPrivileges>"
            "<requestedExecutionLevel level=""asInvoker"" uiAccess=""false""/>"
        "</requestedPrivileges></security>"
    "</trustInfo>"
    "<dependency>"
        "<dependentAssembly>"
            "<assemblyIdentity type=""win32"" name=""Microsoft.Windows.Common-Controls"" version=""6.0.0.0"" processorArchitecture=""*"" publicKeyToken=""6595b64144ccf1df"" language=""*""/>"
        "</dependentAssembly>"
    "</dependency>"
    "</assembly>"
}

// Icône — même dossier que le .h
101 ICON "app.ico"

#else
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// BIM
// a lightweight win32/sqlite3 bim system
// (c) NFR 2026.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#ifndef WIN32_CANVASWNDPROC_H
#define WIN32_CANVASWNDPROC_H
#ifdef __cplusplus
extern "C" {
#endif


#include <stdio.h>
#include <stdbool.h>
#include <sqlite3.h>
#include <cairo/cairo.h>
#include <cairo/cairo-win32.h>
#include <math.h>

#define CANVAS_CLASS_NAME L"CairoCanvasClass"
typedef struct {
    GuiCanvas *cv;
    cairo_t   *cr;
} CanvasCairo;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#define GRID_STEP 10.0f
void canvas_draw_grid(cairo_t *cr, GuiCanvas *cv)
{
    float step      = GRID_STEP;
    float majorStep = 100.0f;

    // Bornes du viewport en coordonnées monde
    float vMinX, vMaxX, vMinY, vMaxY;
    canvas_get_viewport_bounds(cv, &vMinX, &vMaxX, &vMinY, &vMaxY);

    // Premier multiple de step <= vMinX
    float startX = floorf(vMinX / step) * step;
    float startY = floorf(vMinY / step) * step;

    // Lignes verticales
    for (float x = startX; x <= vMaxX; x += step) {
        BOOL isMajor = (fmodf(fabsf(x) + 0.1f, majorStep) < 0.5f);
        cairo_set_source_rgba(cr, 1, 1, 1, isMajor ? 0.18 : 0.07);
        cairo_set_line_width(cr, isMajor ? 1.0 : 0.5);

        double xs1, ys1, xs2, ys2;
        wtsd(cv, x, vMinY, &xs1, &ys1);
        wtsd(cv, x, vMaxY, &xs2, &ys2);
        cairo_move_to(cr, xs1, ys1);
        cairo_line_to(cr, xs2, ys2);
        cairo_stroke(cr);
    }

    // Lignes horizontales
    for (float y = startY; y <= vMaxY; y += step) {
        BOOL isMajor = (fmodf(fabsf(y) + 0.1f, majorStep) < 0.5f);
        cairo_set_source_rgba(cr, 1, 1, 1, isMajor ? 0.18 : 0.07);
        cairo_set_line_width(cr, isMajor ? 1.0 : 0.5);

        double xs1, ys1, xs2, ys2;
        wtsd(cv, vMinX, y, &xs1, &ys1);
        wtsd(cv, vMaxX, y, &xs2, &ys2);
        cairo_move_to(cr, xs1, ys1);
        cairo_line_to(cr, xs2, ys2);
        cairo_stroke(cr);
    }
}

static void canvas_apply_snap(GuiCanvas *cv, BimPoint *p) {
    float gridSize = GRID_STEP; 
    if (cv->snap) {
        p->x = roundf(p->x / gridSize) * gridSize;
        p->y = roundf(p->y / gridSize) * gridSize;
    }
}

static void apply_hex_color_alpha(cairo_t *cr, int hex_color, double alpha) {
    double r = ((hex_color >> 16) & 0xFF) / 255.0;
    double g = ((hex_color >> 8) & 0xFF) / 255.0;
    double b = (hex_color & 0xFF) / 255.0;
    cairo_set_source_rgba(cr, r, g, b, alpha); 
}

/// rendering ////////////////////////////////////////////////////////////////////////////////////////
void cairo_blob_renderer(void *user_data, const void *blob, int size, int type, BimStyle *style) {
    CanvasCairo * CvCr = (CanvasCairo *) user_data;
    cairo_t   *cr = CvCr->cr; 
    GuiCanvas *cv = CvCr->cv;
    
    // a. Appliquer le style (on pourrait faire un cache de styles pour éviter de requêter la DB)
    //apply_style_to_cairo(cr, style_id);
    if (blob == NULL) {
        if (type == 5) {
            apply_hex_color_alpha(cr, style->fill_color, 0.8); // Couleur de fond
            cairo_fill_preserve(cr); // On remplit et on garde le tracé
        }

        // 2. On configure et on dessine le contour
        // (Même si c'est un polygone, on veut son contour par-dessus le fond)
        apply_hex_color_alpha(cr, style->stroke_color, 1.0); // Couleur du trait
        cairo_set_line_width(cr, style->stroke_width);
        cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
    
        cairo_stroke(cr); // Dessine le contour et vide enfin le tracé
    return;
    }  
   
    // b. Désérialiser le BLOB (Format: [int n][double x1][double y1]...)
    int *n_ptr = (int*)blob;
    int n_points = *n_ptr;
    double *coords = (double*)((char*)blob + sizeof(int));

    if (n_points < 1) return;
  
    // c. Dessiner
    double xs, ys;
    double *p = coords;
    wtsd(cv,p[0], p[1], &xs, &ys);
    cairo_move_to(cr, xs, ys );
    p +=2;
    
    for (int i = 1; i < n_points; i++, p+=2) {
        wtsd(cv,p[0], p[1], &xs, &ys);
        cairo_line_to(cr, xs, ys);
    }
}


static void apply_line_type(cairo_t *cr, int line_type, double scale) {
    switch (line_type) {
    case 0: /* solid */
        cairo_set_dash(cr, NULL, 0, 0);
        break;
    case 1: /* dash */
        { double d[] = {8*scale, 4*scale};
          cairo_set_dash(cr, d, 2, 0); }
        break;
    case 2: /* dot */
        { double d[] = {2*scale, 4*scale};
          cairo_set_dash(cr, d, 2, 0); }
        break;
    case 3: /* dashdot */
        { double d[] = {8*scale, 4*scale, 2*scale, 4*scale};
          cairo_set_dash(cr, d, 4, 0); }
        break;
    case 4: /* dashdotdot */
        { double d[] = {8*scale, 3*scale, 2*scale, 3*scale, 2*scale, 3*scale};
          cairo_set_dash(cr, d, 6, 0); }
        break;
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int draw_cb(int layer, BimStyle *style, bool highlight, BimObject *obj, void* user_data){
    CanvasCairo * CvCr = (CanvasCairo *) user_data;
    cairo_t *cr = CvCr->cr; 
    GuiCanvas *cv = CvCr->cv;
    double xs, ys;
    if (highlight){
        cairo_set_source_rgba(cr, 1.0, 0.85, 0.0, 0.85);
        cairo_set_line_width(cr, 6.0);
    }    
    else {    
        apply_hex_color_alpha(cr, style->stroke_color, 0.6);
        cairo_set_line_width(cr, style->stroke_width );
        apply_line_type(cr, style->line_type, 1.0);
    }    
    cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);     
 
    switch(obj->type) {
        case OBJ_NODE     : {
            //position
            wtsd(cv, obj->sx, obj->sy, &xs, &ys);
            SymbolCacheEntry *entry = bim_db_get_symbol(&(cv->db),obj->symbol_id);
            if (entry == NULL)
            {
                printf("symbold %d pas en cache\n", obj->symbol_id );
                cairo_move_to(cr, xs-5,ys);
                cairo_line_to(cr, xs+5,ys);
                cairo_move_to(cr, xs,ys-5);
                cairo_line_to(cr, xs,ys+5);
                cairo_stroke(cr);
                return 0;
            }    
            cairo_save(cr);
            cairo_translate(cr, xs, ys);
            cairo_rotate(cr, obj->rotation); // Rotation en radians
            cairo_scale(cr, 1.0, 1.0);
            int w = 50; //entry->w
            int h = 50; //entry->h
            cairo_translate(cr, -w / 2.0, -h / 2.0);
            RsvgRectangle viewport = {0, 0, w, h }; //entry->w, entry->h};
            rsvg_handle_render_document(entry->handle, cr, &viewport, NULL);
            if (highlight) {
                // halo de sélection
                double halo_size = 30.0; // rayon
                cairo_translate(cr, w/2, h/2);
                // Un dégradé radial donne un aspect "glow" très pro
                cairo_pattern_t *pat = cairo_pattern_create_radial(0, 0, 5, 0, 0, halo_size);
                // Stop 0 (Centre) : Jaune doré, assez opaque
                cairo_pattern_add_color_stop_rgba(pat, 0, 1.0, 0.9, 0.0, 0.7); 
                // Stop 1 (Bord) : Jaune clair, transparent
                cairo_pattern_add_color_stop_rgba(pat, 1, 1.0, 1.0, 0.4, 0.0);
                           
                cairo_set_source(cr, pat);
                cairo_arc(cr, 0, 0, halo_size, 0, 2 * 3.14159);
                cairo_fill(cr);
                cairo_pattern_destroy(pat); 
            }
            cairo_restore(cr);
        }
        break;
        case OBJ_POLYLINE :{
            wtsd(cv, obj->poly.vertices[0].x, obj->poly.vertices[0].y, &xs, &ys);
            cairo_move_to (cr, xs, ys);
            for(int i= 1; i < obj->poly.count; i++) {
                wtsd(cv, obj->poly.vertices[i].x, obj->poly.vertices[i].y, &xs, &ys);
                cairo_line_to (cr, xs, ys);
            }
            cairo_stroke(cr);
        }  
        break;
        case OBJ_POLYGON : {
            wtsd(cv, obj->poly.vertices[0].x, obj->poly.vertices[0].y, &xs, &ys);
            cairo_move_to(cr, xs, ys);
            for (int i = 1; i < obj->poly.count; i++) {
                wtsd(cv, obj->poly.vertices[i].x, obj->poly.vertices[i].y, &xs, &ys);
                cairo_line_to(cr, xs, ys);
            }
            cairo_close_path(cr);                      // ferme le polygone
            cairo_set_source_rgba(cr, 0.3, 0.7, 1.0, 0.25);
            cairo_fill_preserve(cr);                   // remplit
            cairo_set_source_rgb(cr, 0.3, 0.7, 1.0);
            cairo_stroke(cr);    
        }
        break;          
    }
  return 0;  
}


static void canvas_rubber_band(GuiCanvas *cv, cairo_t *cr, BimObject *temp, ObjType tool, BimPoint mouse)
{
    switch (tool)
    {
    case ZOOM_WINDOW :
        double x; 
        double y;
        wtsd(cv, cv->start.x, cv->start.y, &x, &y);

        double w = cv->mx - x;
        double h = cv->my - y;

        // Configuration du style "Rubber Band"
        cairo_set_source_rgba(cr, 0.1, 0.5, 1.0, 0.3); // Bleu transparent pour l'intérieur
        cairo_rectangle(cr, x, y, w, h);
        cairo_fill_preserve(cr);

        cairo_set_source_rgb(cr, 0.2, 0.6, 1.0);      // Bleu vif pour le contour
        cairo_set_line_width(cr, 1.0);

        const double dashed[] = {4.0, 4.0};
        cairo_set_dash(cr, dashed, 2, 0);
    
        cairo_stroke(cr);
        cairo_set_dash(cr, NULL, 0, 0);      
        break;
    case OBJ_NODE : {
        int mx,my;
        wts(cv, (cv->mouseW.x), (cv->mouseW.y), &mx,&my);    
        cairo_set_source_rgba(cr, 1, 1, 1, 0.35);
        cairo_set_line_width(cr, 1.0);
        cairo_move_to(cr, mx - 12, my);
        cairo_line_to(cr, mx + 12, my);
        cairo_move_to(cr, mx, my - 12);
        cairo_line_to(cr, mx, my + 12);
        cairo_stroke(cr);

        cairo_arc(cr, mx, my, 14, 0, 2*M_PI);
        cairo_set_source_rgba(cr, 0.30, 0.70, 1.00, 0.25);
        cairo_fill_preserve(cr);
        cairo_set_source_rgba(cr, 0.30, 0.70, 1.00, 0.70);
        cairo_set_line_width(cr, 1.0);
        cairo_stroke(cr);

        cairo_select_font_face(cr, "Segoe UI",
            CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
        cairo_set_font_size(cr, 11.0);
        cairo_set_source_rgba(cr, 0.30, 0.70, 1.00, 0.85);
        cairo_move_to(cr, mx + 18, my - 4);
        //cairo_show_text(cr, g_symbols[g_last_result.symbol_idx].name);
        //cairo_set_font_size(cr, 9.5);
        //cairo_set_source_rgba(cr, 0.55, 0.60, 0.75, 0.70);
        //cairo_move_to(cr, mx + 18, my + 10);
        //cairo_show_text(cr, "Clic=placer  Clic droit/Echap=terminer");
        if (cv->picked) {
            cairo_move_to(cr, mx, my);
            cairo_arc(cr, mx, my, 20, 0, 2*M_PI);
            cairo_set_source_rgba(cr, 1.0, 0.85, 0.0, 0.85);
            cairo_set_line_width(cr, 6.0);
            cairo_stroke(cr);
        }
        }
        break;    
    case OBJ_POLYLINE:
    case OBJ_POLYGON: {
        int nb = temp->poly.count;
        double xs, ys;

        // ── 1. Segments déjà fixés ──────────────────────────────
        if (nb >= 2) {
            // Polygone : remplissage semi-transparent d'abord
            if (tool == OBJ_POLYGON && nb >= 3) {
                wtsd(cv, temp->poly.vertices[0].x, temp->poly.vertices[0].y, &xs, &ys);
                cairo_move_to(cr, xs, ys);
                for (int i = 1; i < nb; i++) {
                    wtsd(cv, temp->poly.vertices[i].x, temp->poly.vertices[i].y, &xs, &ys);
                    cairo_line_to(cr, xs, ys);
                }
                cairo_close_path(cr);
                cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.25);  // 0x40FF0000
                cairo_fill(cr);
            }

            // Trait des segments fixés
            cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);             // 0xFFFF0000
            cairo_set_line_width(cr, 1.0);
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);

            wtsd(cv, temp->poly.vertices[0].x, temp->poly.vertices[0].y, &xs, &ys);
            cairo_move_to(cr, xs, ys);
            for (int i = 1; i < nb; i++) {
                wtsd(cv, temp->poly.vertices[i].x, temp->poly.vertices[i].y, &xs, &ys);
                cairo_line_to(cr, xs, ys);
            }
            cairo_stroke(cr);
        }

        // ── 2. Segment élastique dernier point → souris ─────────
        if (nb > 0) {
            BimPoint last = temp->poly.vertices[nb - 1];
            double lx, ly, mx, my;
            wtsd(cv, last.x,    last.y,    &lx, &ly);
            wtsd(cv, mouse.x,   mouse.y,   &mx, &my);

            cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.60);  // légèrement transparent
            cairo_set_line_width(cr, 1.0);
            cairo_set_dash(cr, (double[]){6.0, 3.0}, 2, 0);  // pointillé
            cairo_move_to(cr, lx, ly);
            cairo_line_to(cr, mx, my);
            cairo_stroke(cr);
            cairo_set_dash(cr, NULL, 0, 0);                   // reset pointillé

        }
        //indicateur de proximité.
        if (cv->picked) {
            double mx, my;
            wtsd(cv, mouse.x,   mouse.y,   &mx, &my);
            cairo_move_to(cr, mx, my);
            cairo_arc(cr, mx, my, 20, 0, 2*M_PI);
            cairo_set_source_rgba(cr, 1.0, 0.85, 0.0, 0.85);
            cairo_set_line_width(cr, 6.0);
            cairo_stroke(cr);
        }        
        break;
    }
    }
}

static void canvas_draw_edit_handles(GuiCanvas *cv, cairo_t *cr) {
    for (int i = 0; i < cv->edit_handle_count; i++) {
        BimEditHandle *h = &cv->edit_handles[i];
        double sx, sy;
        wtsd(cv, h->x, h->y, &sx, &sy);  /* monde → écran */
        
        double r = h->is_node ? 10.0 : 7.0;
        
        /* Fond */
       
        if (i == cv->dragging_handle)
        {
            cairo_set_source_rgba(cr, 1.0, 0.85, 0.0, 0.9);  /* jaune drag */
            r = 12.0;
        }    
        else if (h->is_node)
            cairo_set_source_rgba(cr, 0.3, 0.8, 1.0, 0.85);  /* bleu nœud */
        else
            cairo_set_source_rgba(cr, 0.8, 0.8, 0.8, 0.75);  /* gris vertex */
        cairo_arc(cr, sx, sy, r, 0, 2*M_PI);    
        cairo_fill_preserve(cr);
        
        /* Bordure */
        cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.6);
        cairo_set_line_width(cr, 1.7);
        cairo_stroke(cr);
    }
}

static void canvas_render_viewport(GuiCanvas* cv, BimObjRenderCallBack draw, cairo_t *cr) {
    float vMinX, vMaxX, vMinY, vMaxY; 
    CanvasCairo CvCr = {cv, cr};
  
    canvas_get_viewport_bounds(cv, &vMinX, &vMaxX, &vMinY, &vMaxY);
    bim_db_render_blobs(&(cv->db), vMinX, vMaxX, vMinY, vMaxY, cairo_blob_renderer , &CvCr);
    bim_db_render_viewport(&(cv->db), vMinX, vMaxX, vMinY, vMaxY, false, draw, &CvCr);
    bim_db_render_viewport(&(cv->db), vMinX, vMaxX, vMinY, vMaxY, true,  draw, &CvCr);
    //if (cv->is_interacting)
    //    canvas_rubber_band(cv, cr, &(cv->temp_obj), cv->current_tool, cv->mouseW);
}

static void canvas_cache_free(GuiCanvas *cv) {
    if (cv->hbmCache) {
        SelectObject(cv->hdcCache, NULL);
        DeleteObject(cv->hbmCache);
        cv->hbmCache = NULL;
    }
    if (cv->hdcCache) {
        DeleteDC(cv->hdcCache);
        cv->hdcCache = NULL;
    }
    cv->cache_valid = false;
}

static void canvas_paint(GuiCanvas *cv, HWND hwnd)
{
    
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    if (cv->cache_valid) {
        // Copier cache dans un buffer intermédiaire
        HDC     hdcMem = CreateCompatibleDC(hdc);
        HBITMAP hbmMem = CreateCompatibleBitmap(hdc, cv->w, cv->h);
        HGDIOBJ hOld   = SelectObject(hdcMem, hbmMem);
        
        BitBlt(hdcMem, 0, 0, cv->w, cv->h, cv->hdcCache, 0, 0, SRCCOPY);
        
        if (cv->is_interacting) {
            cairo_surface_t *surf = cairo_win32_surface_create(hdcMem);
            cairo_t *cr = cairo_create(surf);
            canvas_rubber_band(cv, cr, &cv->temp_obj, cv->current_tool, cv->mouseW);
            cairo_destroy(cr);
            cairo_surface_destroy(surf);
        }
        if (cv->edit_active) {
            cairo_surface_t *surf = cairo_win32_surface_create(hdcMem);
            cairo_t *cr = cairo_create(surf);
            CanvasCairo CvCr = {cv, cr};
            bim_db_render_edit(&cv->db, draw_cb, &CvCr);
            canvas_draw_edit_handles(cv, cr);
            cairo_destroy(cr);
            cairo_surface_destroy(surf);
        }        
        
        
        BitBlt(hdc, 0, 0, cv->w, cv->h, hdcMem, 0, 0, SRCCOPY);
        
        SelectObject(hdcMem, hOld);
        DeleteObject(hbmMem);
        DeleteDC(hdcMem);
        EndPaint(hwnd, &ps);
        return;
        }    
    // ── Double buffering ────────────────────────────────────────
    HDC     hdcMem = CreateCompatibleDC(hdc);
    HBITMAP hbmMem = CreateCompatibleBitmap(hdc, cv->w, cv->h);
    HGDIOBJ hOld   = SelectObject(hdcMem, hbmMem);

    if (!cv->hdcCache)
    {
       cv->hdcCache = CreateCompatibleDC(hdc);
       cv->hbmCache = CreateCompatibleBitmap(hdc, cv->w, cv->h);
       HGDIOBJ prev = SelectObject(cv->hdcCache, cv->hbmCache);
       //printf("SelectObject prev=%p\n", (void*)prev);
    }
    // ── Surface Cairo sur le buffer mémoire ────────────────────
    cairo_surface_t *surface = cairo_win32_surface_create(hdcMem);
    cairo_t         *cr      = cairo_create(surface);

    // ── Fond noir "BIM"  ──────────
    cairo_set_source_rgb(cr, 0.08, 0.09, 0.12);
    cairo_paint(cr);    
    

    // ── 1. Grille ───────────────────────────────────────────────
    if (cv->grid) canvas_draw_grid(cr, cv);
    // ── 2. Objets — deux passes (normal + highlight) ────────────
    canvas_render_viewport(cv, draw_cb, cr);
    cairo_surface_flush(surface); 
    BitBlt(cv->hdcCache, 0, 0,cv->w, cv->h, hdcMem, 0, 0, SRCCOPY); 
    cv->cache_valid = true;
    //probably not used.
    if (cv->is_interacting || cv->current_tool != SELECTION_POINT) {
        canvas_rubber_band(cv, cr, &cv->temp_obj, cv->current_tool, cv->mouseW);
    }     
    //probably not used.
    if (cv->edit_active) {
        CanvasCairo CvCr = {cv, cr};
        bim_db_render_edit(&cv->db, draw_cb, &CvCr);
        canvas_draw_edit_handles(cv, cr);
    }
    // ── Libération Cairo ────────────────────────────────────────
    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    // ── Transfert buffer → écran ────────────────────────────────
    BitBlt(hdc, 0, 0, cv->w, cv->h, hdcMem, 0, 0, SRCCOPY);
    
    SelectObject(hdcMem, hOld);
    DeleteObject(hbmMem);
    DeleteDC(hdcMem);
    EndPaint(hwnd, &ps);
}

//end rendering ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void canvas_save_entity(GuiCanvas* cv) {
    int assignments[MAX_POLY_PTS]; 
    memset(assignments, 0, sizeof(int) * cv->temp_obj.poly.count);    
    bim_db_save_entity(&(cv->db), cv->current_layer, cv->temp_obj.type, cv->temp_obj.poly.vertices, cv->temp_obj.poly.count, assignments );
 }

static void canvas_reset_temp_object(GuiCanvas *cv) {
    memset(&cv->temp_obj, 0, sizeof(BimObject));
    cv->is_interacting = false; //faut-il ?
}

static void canvas_commit_temp_object(GuiCanvas* cv) {
    //if (!cv->is_interacting) return;
    canvas_save_entity(cv);
    canvas_reset_temp_object(cv);
    cv->cache_valid = false;
}

static void canvas_save_node(GuiCanvas *cv, int symbol, double x, double y, double rotation) {
    bim_db_save_node(&(cv->db),cv->current_layer, symbol, x, y, rotation);
    canvas_reset_temp_object(cv);
    cv->cache_valid = false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void canvas_wheel_zoom(GuiCanvas *cv,HWND hwnd, int x, int y, int delta) {
    POINT pt = { x, y };
    ScreenToClient(hwnd, &pt);
    double worldXBefore, worldYBefore;
    stw(cv, pt.x, pt.y, &worldXBefore, &worldYBefore);
    float zoomFactor = (delta > 0) ? 1.1f : (1.0f / 1.1f);
    cv->zoom *= zoomFactor;
    cv->offset.x = pt.x - (worldXBefore * cv->zoom);
    cv->offset.y = pt.y + (worldYBefore * cv->zoom); //inversion du y 
    cv->cache_valid = false;   
}

static void canvas_add_to_temp(GuiCanvas *cv, BimPoint pt) {
    cv->start = pt;
  
    switch (cv->current_tool) {
        case OBJ_POLYGON  :
        case OBJ_POLYLINE :  
        if (cv->temp_obj.poly.count < MAX_POLY_PTS) 
            cv->temp_obj.poly.vertices[cv->temp_obj.poly.count++] = pt;
        break;
    }    
}

static void canvas_refresh(GuiCanvas *cv) {
    InvalidateRect(cv->hwnd, NULL, TRUE);    
}

static void canvas_zoom_extent(GuiCanvas *cv) {
    cv->refit = true;
    canvas_refresh(cv);
}


static void canvas_zoom_selection(GuiCanvas *cv)
{double x1,y1,x2,y2;
    if (bim_db_get_selection_extents(&cv->db,&x1,&x2,&y1,&y2))
    {
        canvas_zoom_window(cv, x1, y1, x2, y2);
        canvas_refresh(cv);
    }
}


static void canvas_mouse_info(GuiCanvas *cv, float x, float y) {
    char info[100];
    sprintf(info, "X: %8.3f, Y: %8.3f",cv->mouseW.x, cv->mouseW.y);
    cv->call_back(EV_MOUSEMOVE, info, cv->user_data);    
}

static char * canvas_tool_to_text(GuiCanvas * cv){
    switch(cv->current_tool)
    {
        case OBJ_POLYLINE       : return "Polyline creation"; 
        case OBJ_POLYGON        : return "Polygon creation"; 
        case OBJ_NODE           : return "Node creation"; 
        case SELECTION_POINT    : return "Selection Point"; 
        case ZOOM_WINDOW        : return "Zoom Window";   
    }
}

static void canvas_mode_info(GuiCanvas *cv) {
    cv->call_back(EV_MODE, canvas_tool_to_text(cv), cv->user_data);    
}

static void canvas_set_mode(GuiCanvas *cv, ObjType mode, bool interacting ){
    cv->current_tool = mode;
    cv->temp_obj.type = mode;
    cv->is_interacting = interacting;
    canvas_mode_info(cv);
}

static int canvas_delete_selection( GuiCanvas *cv) {
   return bim_db_delete_entity(&(cv->db));
}

static void canvas_handle_keyboard(GuiCanvas *cv, int key) {
    switch(key)
    { 
    case 'a' :
    case 'A' :
        canvas_set_mode(cv, OBJ_POLYGON, true);
        break;        
    case 'p' :
    case 'P' : 
        canvas_set_mode(cv, OBJ_POLYLINE, true);
        break;
    case 's' :
    case 'S' : 
        canvas_set_mode(cv, SELECTION_POINT, false);
        break; 
    case 'n' :
    case 'N' : 
        canvas_set_mode(cv, OBJ_NODE, false);
        break;                       
    case 'f' :
    case 'F' : 
        cv->refit = true;
        canvas_refresh(cv);
        break;
    case 'z' :
    case 'Z' : 
        canvas_set_mode(cv, ZOOM_WINDOW, false);
        cv->is_interacting = false;
        break;        
    case VK_ADD:      
    case VK_OEM_PLUS:
        canvas_zoom_plus(cv);
        canvas_refresh(cv);
        break;
    case VK_SUBTRACT: 
    case VK_OEM_MINUS: // Touche -/_                   
        canvas_zoom_minus(cv);
        canvas_refresh(cv);
        break;      
    case VK_ESCAPE :
        canvas_set_mode(cv, SELECTION_POINT, false);
        cv->is_interacting = false;
        cv->cache_valid = false;
        canvas_reset_temp_object(cv);
        if (cv->edit_active)
        {  
            cv->edit_active = 0;
            bim_db_edit_rollback(&(cv->db));
            cv->cache_valid = false;
        }        
        canvas_refresh(cv);
        break;
    case VK_BACK :
        if (cv->is_interacting) {
            if (cv->temp_obj.poly.count > 1 ){
                cv->temp_obj.poly.count--;
                canvas_refresh(cv);
            }
        }
        break;
    case VK_DELETE :
        canvas_delete_selection(cv);
        cv->cache_valid = false;
        canvas_refresh(cv);
        break;    
    }       
}

static void canvas_move(GuiCanvas * cv, int x, int y) {
    // Calcul du delta de mouvement
    cv->offset.x += (float)( x - cv->last_mouse.x);
    cv->offset.y += (float)( y - cv->last_mouse.y);           
    // Sauvegarde de la position actuelle pour le prochain mouvement
    cv->last_mouse.x = x;
    cv->last_mouse.y = y;
}

static void canvas_start_dragging(GuiCanvas *cv, int x, int y) {
    cv->is_dragging = true;
    cv->last_mouse.x = x;
    cv->last_mouse.y = y;
}

static bool canvas_pick_vertice(GuiCanvas *cv, BimPoint *pt){
    double x, y;
    double tol = 8.0f / cv->zoom;
    cv->picked = bim_db_get_snapped_coordinates(&(cv->db), pt->x, pt->y, tol, &x, &y);
    if (cv->picked)
    { pt->x = x;
      pt->y = y;
    }
    return cv->picked;
}

static bool canvas_pick_first(GuiCanvas *cv, BimPoint *pt){
    double tol = 8.0f / cv->zoom;
    if (cv->temp_obj.poly.count > 2)
    {
       cv->picked = false;
       return cv->picked;
    }   
    cv->picked = (bim_distance(pt, &cv->temp_obj.poly.vertices[0]) < tol);
    if (cv->picked)
    {
        cv->picked = true;
        *pt = cv->temp_obj.poly.vertices[0]; 
    }
    return cv->picked;
}

static void canvas_set_current_layer(GuiCanvas *cv, int layer,  char * layer_name){
    cv->current_layer = layer;
    cv->call_back(EV_CUR_LAYER, layer_name, cv->user_data);   
}

static int canvas_edit_lbuttondown(GuiCanvas *cv,double wx, double wy) {
    // Chercher le handle le plus proche
    double snap_dist = 10.0 / cv->zoom;  /* 10px écran */
    for (int i = 0; i < cv->edit_handle_count; i++) {
        double dx = cv->edit_handles[i].x - wx;
        double dy = cv->edit_handles[i].y - wy;
        if (sqrt(dx*dx + dy*dy) < snap_dist) {
            return i;
        }
    }
    return -1;
}

static void canvas_edit_mousemove(GuiCanvas *cv, double wx, double wy) {

    BimEditHandle *h = &cv->edit_handles[cv->dragging_handle];
    bim_db_edit_move_vertex(&cv->db, h->vertex_id, wx, wy);
    // Recharger les handles — le vertex a bougé
    bim_db_edit_load_handles(&cv->db, cv->edit_handles,
                            MAX_POLY_PTS,
                            &cv->edit_handle_count);
    h->x = wx; h->y = wy;
    canvas_refresh(cv);
}

//----------------------------------------------------------------------------------------------------------


/* ══════════════════════════════════════════════════════════════════
   GESTIONNAIRES D'ÉVÉNEMENTS — logique métier pure
   Appelés par CanvasWndProc après décodage des messages Win32.
   ══════════════════════════════════════════════════════════════════ */

static void canvas_on_resize(GuiCanvas *cv, int w, int h)
{
    cv->w = w;
    cv->h = h;
    canvas_cache_free(cv);
    canvas_refresh(cv);
}

static void canvas_on_paint(GuiCanvas *cv, HWND hwnd)
{
    canvas_zoom_extents(cv);
    canvas_paint(cv, hwnd);
}

static void canvas_on_wheel(GuiCanvas *cv, HWND hwnd, int x, int y, int delta)
{
    canvas_wheel_zoom(cv, hwnd, x, y, delta);
    canvas_refresh(cv);
}

static void canvas_on_mbutton_down(GuiCanvas *cv, HWND hwnd, int x, int y)
{
    canvas_start_dragging(cv, x, y);
    SetCapture(hwnd);
}

static void canvas_on_button_up(GuiCanvas *cv, int x, int y)
{
    (void)x; (void)y;
    ReleaseCapture();
    cv->is_dragging = FALSE;
    if ((cv->current_tool == OBJ_POLYLINE || cv->current_tool == OBJ_POLYGON)
        && cv->is_interacting) {
        canvas_commit_temp_object(cv);
        canvas_refresh(cv);
        canvas_set_mode(cv, SELECTION_POINT, false);
    }
}

static void canvas_on_mouse_move(GuiCanvas *cv, int x, int y)
{
    cv->mx = x;
    cv->my = y;
    stw(cv, cv->mx, cv->my, &(cv->mouseW.x), &(cv->mouseW.y));

    if (cv->current_tool != SELECTION_POINT) {
        if (!canvas_pick_vertice(cv, &cv->mouseW))
            canvas_apply_snap(cv, &cv->mouseW);
        else
            canvas_refresh(cv);
    } else {
        canvas_apply_snap(cv, &cv->mouseW);
    }
    canvas_mouse_info(cv, cv->mouseW.x, cv->mouseW.y);

    if (cv->is_interacting) {
        if (cv->current_tool == OBJ_POLYGON)
            canvas_pick_first(cv, &cv->mouseW);
        canvas_refresh(cv);
    }
    if (cv->is_dragging) {
        canvas_move(cv, cv->mx, cv->my);
        cv->cache_valid = false;
        canvas_refresh(cv);
    }
    if (cv->edit_active && cv->dragging_handle >= 0)
        canvas_edit_mousemove(cv, cv->mouseW.x, cv->mouseW.y);
}

static void canvas_on_rbutton_down(GuiCanvas *cv, HWND hwnd, int x, int y)
{
    if (cm_visible(cv->cairo_menu)) cm_hide(cv->cairo_menu);
    if (!cv->is_interacting) {
        POINT pt = { x, y };
        ClientToScreen(hwnd, &pt);
        cm_show(cv->cairo_menu, pt.x, pt.y);
    }
    if (cv->edit_active) {
        cv->edit_active = 0;
        bim_db_edit_rollback(&(cv->db));
        cv->cache_valid = false;
        canvas_refresh(cv);
        printf("rollback of edition\n");
    }
}

static void canvas_on_lbutton_down(GuiCanvas *cv, HWND hwnd, int x, int y, int shift)
{
    SetFocus(hwnd);
    stw(cv, x, y, &(cv->start.x), &(cv->start.y));
    if (cm_visible(cv->cairo_menu)) cm_hide(cv->cairo_menu);

    switch (cv->current_tool) {
        case OBJ_NODE:
            if (!canvas_pick_vertice(cv, &cv->start))
                canvas_apply_snap(cv, &cv->start);
            canvas_save_node(cv, cv->current_symbol, cv->start.x, cv->start.y, 0.0);
            canvas_set_mode(cv, SELECTION_POINT, false);
            canvas_refresh(cv);
            break;
        case OBJ_POLYGON:
        case OBJ_POLYLINE: {
            if (!canvas_pick_vertice(cv, &cv->start))
                canvas_apply_snap(cv, &cv->start);
            bool closed = canvas_pick_first(cv, &cv->start);
            canvas_add_to_temp(cv, cv->start);
            if (closed) {
                canvas_commit_temp_object(cv);
                canvas_set_mode(cv, SELECTION_POINT, false);
            }
            canvas_refresh(cv);
            break;
        }
        case SELECTION_POINT: {
            double tol = 8.0f / cv->zoom;
            bim_db_pick(&(cv->db), cv->start.x, cv->start.y, tol, shift);
            cv->cache_valid = false;
            canvas_refresh(cv);
            break;
        }
        case ZOOM_WINDOW:
            cv->is_interacting = true;
            canvas_refresh(cv);
            break;
    }
    if (cv->edit_active) {
        cv->dragging_handle = canvas_edit_lbuttondown(cv, cv->start.x, cv->start.y);
        if (cv->dragging_handle == -1) {
            cv->edit_active = 0;
            bim_db_edit_commit(&(cv->db));
            cv->cache_valid = false;
            canvas_refresh(cv);
            printf("end of edition\n");
        }
    }
}

static void canvas_on_lbutton_up(GuiCanvas *cv, int x, int y)
{
    if ((cv->current_tool == ZOOM_WINDOW) && cv->is_interacting) {
        cv->mx = x;
        cv->my = y;
        double wx, wy;
        stw(cv, x, y, &wx, &wy);
        canvas_zoom_window(cv, cv->start.x, cv->start.y, wx, wy);
        canvas_refresh(cv);
        canvas_set_mode(cv, SELECTION_POINT, false);
    }
    if (cv->edit_active && cv->dragging_handle >= 0) {
        cv->dragging_handle = -1;
        canvas_refresh(cv);
    }
}

static void canvas_on_lbutton_dbl(GuiCanvas *cv, int x, int y)
{
    stw(cv, x, y, &(cv->mouseW.x), &(cv->mouseW.y));
    printf("in double click\n");
    double tol = 8.0f / cv->zoom;
    if (bim_db_pick(&(cv->db), cv->mouseW.x, cv->mouseW.y, tol, 0) > 0) {
        printf("in edit\n");
        bim_db_edit_begin(&(cv->db));
        cv->edit_active = 1;
        bim_db_edit_load_handles(&cv->db,
                                 cv->edit_handles,
                                 MAX_POLY_PTS,
                                 &cv->edit_handle_count);
        printf("handles loaded: %d\n", cv->edit_handle_count);
        canvas_refresh(cv);
    }
}

static void canvas_on_drop_files(GuiCanvas *cv, HDROP hDrop)
{
    char filePath[MAX_PATH];
    UINT fileCount = DragQueryFileA(hDrop, 0xFFFFFFFF, NULL, 0);
    for (UINT i = 0; i < fileCount; i++) {
        DragQueryFileA(hDrop, i, filePath, MAX_PATH);
        char *ext = strrchr(filePath, '.');
        if (ext && _stricmp(ext, ".shp") == 0) {
            bim_db_import_blob_layer(&(cv->db), filePath, 0);
            cv->call_back(EV_LAYERS, NULL, cv);
            cv->refit = true;
            cv->cache_valid = false;
            canvas_refresh(cv);
        }
    }
    DragFinish(hDrop);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK CanvasWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    GuiCanvas* cv = (GuiCanvas*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    if (!cv && msg != WM_NCCREATE) return DefWindowProc(hwnd, msg, wParam, lParam);

    switch (msg) {
    case WM_CREATE:
        SetFocus(hwnd);
        return 0;
    case WM_SIZE:
        canvas_on_resize(cv, LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_PAINT:
        canvas_on_paint(cv, hwnd);
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_KILLFOCUS:
        return 0;
    case WM_DESTROY:
        canvas_cache_free(cv);
        break;
    case WM_NCDESTROY:
        bim_close(cv);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, 0);
        return 0;
    case WM_MOUSEWHEEL:
        canvas_on_wheel(cv, hwnd,
                        GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
                        GET_WHEEL_DELTA_WPARAM(wParam));
        return 0;
    case WM_MBUTTONDOWN:
        canvas_on_mbutton_down(cv, hwnd,
                               GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
        canvas_on_button_up(cv, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_MOUSEMOVE:
        canvas_on_mouse_move(cv, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_RBUTTONDOWN:
        canvas_on_rbutton_down(cv, hwnd,
                               GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONDOWN:
        canvas_on_lbutton_down(cv, hwnd,
                               GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam),
                               (GetAsyncKeyState(VK_SHIFT) & 0x8000) ? 1 : 0);
        return 0;
    case WM_LBUTTONUP:
        canvas_on_lbutton_up(cv, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_LBUTTONDBLCLK:
        canvas_on_lbutton_dbl(cv, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_KEYDOWN:
        canvas_handle_keyboard(cv, (int)wParam);
        return 0;
    case WM_DROPFILES:
        canvas_on_drop_files(cv, (HDROP)wParam);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

static ATOM RegisterCanvasClass(HINSTANCE hInstance) {
    WNDCLASSEXW wcex = {0};

    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS; // Redessiner si la taille change
    wcex.lpfnWndProc = CanvasWndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = sizeof(void*); // Pour stocker le pointeur GuiCanvas
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(NULL, IDC_CROSS); // Curseur en croix pour le dessin
    wcex.hbrBackground = NULL; // On gère le fond nous-mêmes (GDI+) pour éviter le scintillement
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = CANVAS_CLASS_NAME;

    return RegisterClassExW(&wcex);
}

static HWND CreateCanvasWindow(HINSTANCE hInstance, HWND hParent,fptr_t call_back, void *user_data ) {
    // 1. Enregistrement sécurisé
    RegisterCanvasClass(hInstance);
      
    // 2. Création de la fenêtre
    HWND hwnd = CreateWindowExW(
        0, 
        CANVAS_CLASS_NAME, 
        NULL, 
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        0, 0, 0, 0, 
        hParent, 
        NULL, 
        hInstance, 
        NULL
    );

    if (hwnd) {
        // 3. Initialisation INTERNE de la structure et démarrage du moteur db
        GuiCanvas *cv = bim_init(call_back, user_data);
        cv->hwnd = hwnd;    
        
        // 4. Liaison de la structure au HWND
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)cv);
        DragAcceptFiles(hwnd, TRUE);
    }
    
    return hwnd;
}

static inline GuiCanvas* gui_get_canvas_data(HWND hwndCanvas) {
    if (!hwndCanvas) return NULL;
    return (GuiCanvas*)GetWindowLongPtr(hwndCanvas, GWLP_USERDATA);
}

#ifdef __cplusplus
}
#endif
#endif
#endif