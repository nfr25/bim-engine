/*
 * bim_jim.h  –  Intégration Jim TCL dans BIM-engine
 * ──────────────────────────────────────────────────────────────────
 *
 *  PHILOSOPHIE
 *  ───────────
 *  Un interpréteur Jim global, initialisé au démarrage.
 *  Les commandes bim::* exposent les fonctions métier BIM à TCL.
 *  Les scripts .tcl peuvent être chargés depuis le transcript ou
 *  automatiquement au démarrage (bim_startup.tcl).
 *
 *  COMMANDES TCL EXPOSÉES
 *  ───────────────────────
 *  bim::query  <sql> ?label?   → ouvre un tab listview avec le résultat
 *  bim::log    <message>       → CT_INFO dans le transcript
 *  bim::warn   <message>       → CT_WARN dans le transcript
 *  bim::error  <message>       → CT_ERR dans le transcript
 *  bim::eval   <sql>           → exécute SQL sans affichage (INSERT/UPDATE...)
 *  bim::layer  ?idx?           → retourne le layer courant (ou le change)
 *
 *  UTILISATION MINIMALE
 *  ─────────────────────
 *    #define BIM_JIM_IMPLEMENTATION
 *    #include "scripting/bim_jim.h"
 *
 *  INITIALISATION (dans WinMain, après cl_attach_tabpane)
 *    BimJim *bj = bj_create(g_layout, g_canvas, &cv->db);
 *    bj_load_startup(bj);   // charge bim_startup.tcl si présent
 *
 *  DEPUIS LE TRANSCRIPT (on_command)
 *    void on_command(const char *text, void *ud) {
 *        ct_add(g_layout->transcript, CT_CMD, text);
 *        bj_eval(g_bj, text);    // évalue le texte comme TCL
 *    }
 *
 *  DESTRUCTION
 *    bj_destroy(bj);   // dans WM_DESTROY, avant cl_destroy
 *
 * ──────────────────────────────────────────────────────────────────
 *  Dépendances : jim.h, cairo_layout.h, bim_listview.h
 *  Compilateur : GCC / MinGW-w64 (MSYS2)
 *  Sources     : bim.c + scripting/jimtcl/jim.c
 * ──────────────────────────────────────────────────────────────────
 */

#ifndef BIM_JIM_H
#define BIM_JIM_H

#include <jim.h>
#include <stdio.h>
/* sqlite3 utilisé comme void* pour éviter les conflits de headers
 * entre les unités de compilation bim.c et jim_embed.c           */
#include <stdlib.h>
#include <string.h>

/* ── Forward declarations — uniquement si pas encore définies ────── */
#ifndef CAIRO_LAYOUT_H
typedef struct CairoLayout_  CairoLayout;
#endif
#ifndef BIM_LISTVIEW_H
typedef struct BimListView_  BimListView;
#endif
#ifndef CAIRO_TABPANE_H
typedef struct CairoTabPane_ CairoTabPane;
#endif

/* Accès aux fonctions du layout et de la listview —
 * bim_jim.h doit être inclus APRÈS cairo_layout.h et bim_listview.h */
#ifdef CAIRO_LAYOUT_H
#  define BJ_HAS_LAYOUT 1
#else
#  define BJ_HAS_LAYOUT 0
#endif
#ifdef BIM_LISTVIEW_H
#  define BJ_HAS_LISTVIEW 1
#else
#  define BJ_HAS_LISTVIEW 0
#endif

/* ── Structure principale ───────────────────────────────────────── */
typedef struct BimJim_ BimJim;

/* ── API publique ───────────────────────────────────────────────── */
BimJim *bj_create      (CairoLayout *layout, HWND canvas, void *db);
void    bj_set_context (CairoLayout *layout, HWND canvas);
void    bj_destroy     (BimJim *bj);

/* Évaluer une chaîne TCL — affiche résultat/erreur dans le transcript */
int     bj_eval        (BimJim *bj, const char *script);

/* Charger un fichier .tcl */
int     bj_load_file   (BimJim *bj, const char *path);

/* Chercher et charger bim_startup.tcl dans le répertoire courant */
void    bj_load_startup(BimJim *bj);

/* Accès direct à l'interpréteur pour ajouter des commandes custom */
Jim_Interp *bj_interp  (BimJim *bj);

/* ══════════════════════════════════════════════════════════════════
   IMPLÉMENTATION
   ══════════════════════════════════════════════════════════════════ */
#ifdef BIM_JIM_IMPLEMENTATION

/* ── Structure interne ──────────────────────────────────────────── */
struct BimJim_ {
    Jim_Interp  *interp;
    /* layout, canvas et db sont des globaux de bim.c —
     * pas besoin de les dupliquer ici                  */
};

/* ── Accès au transcript via le layout ──────────────────────────── */
/* Pointeurs opaques vers le layout et le canvas —
 * initialisés par bj_set_context() depuis bim.c
 * après que les types sont définis.                */
static void *bj__g_layout = NULL;
static void *bj__g_canvas = NULL;

/* Macros d'accès avec cast */
#define BJ_LAYOUT ((CairoLayout*)bj__g_layout)
#define BJ_CANVAS ((HWND)bj__g_canvas)

static void bj__log(BimJim *bj, int type, const char *msg)
{
    (void)bj;
#if BJ_HAS_LAYOUT && CL_HAS_TRANSCRIPT
    if (BJ_LAYOUT && BJ_LAYOUT->transcript)
        ct_add(BJ_LAYOUT->transcript, type, msg);
    else
        fprintf(stderr, "[bim::log] %s\n", msg);
#else
    fprintf(stderr, "[bim::log] %s\n", msg);
#endif
}

/* ── Helpers résultat Jim ───────────────────────────────────────── */
static void bj__ok(Jim_Interp *i, const char *msg)
{
    Jim_SetResultString(i, msg ? msg : "", -1);
}

static void bj__err(Jim_Interp *i, const char *msg)
{
    Jim_SetResultString(i, msg ? msg : "error", -1);
}

/* ═══════════════════════════════════════════════════════════════════
   COMMANDES TCL bim::*
   ═══════════════════════════════════════════════════════════════════ */

/* ── bim::log <message> ─────────────────────────────────────────── */
static int bj__cmd_log(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2) { Jim_WrongNumArgs(interp, 1, argv, "message"); return JIM_ERR; }
    BimJim *bj = Jim_CmdPrivData(interp);
    bj__log(bj, CT_INFO, Jim_GetString(argv[1], NULL));
    bj__ok(interp, "");
    return JIM_OK;
}

/* ── bim::warn <message> ────────────────────────────────────────── */
static int bj__cmd_warn(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2) { Jim_WrongNumArgs(interp, 1, argv, "message"); return JIM_ERR; }
    BimJim *bj = Jim_CmdPrivData(interp);
    bj__log(bj, CT_WARN, Jim_GetString(argv[1], NULL));
    bj__ok(interp, "");
    return JIM_OK;
}

/* ── bim::error <message> ───────────────────────────────────────── */
static int bj__cmd_error(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2) { Jim_WrongNumArgs(interp, 1, argv, "message"); return JIM_ERR; }
    BimJim *bj = Jim_CmdPrivData(interp);
    bj__log(bj, CT_ERR, Jim_GetString(argv[1], NULL));
    bj__ok(interp, "");
    return JIM_OK;
}

/* ── bim::eval <sql> ────────────────────────────────────────────── */
static int bj__cmd_eval(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc != 2) { Jim_WrongNumArgs(interp, 1, argv, "sql"); return JIM_ERR; }
    BimJim *bj = Jim_CmdPrivData(interp);
    (void)bj;
    GuiCanvas *bj_cv = gui_get_canvas_data(BJ_CANVAS);
    if (!bj_cv || !bj_cv->db.handle) {
        bj__err(interp, "no database");
        return JIM_ERR;
    }
    const char *sql = Jim_GetString(argv[1], NULL);
    char *errmsg = NULL;
    int rc = sqlite3_exec(bj_cv->db.handle, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        char buf[512];
        snprintf(buf, sizeof buf, "SQL error: %s", errmsg ? errmsg : "?");
        sqlite3_free(errmsg);
        bj__log(bj, CT_ERR, buf);
        bj__err(interp, buf);
        return JIM_ERR;
    }
    bj__ok(interp, "");
    return JIM_OK;
}

/* ── bim::query <sql> ?label? ───────────────────────────────────── */
#if BJ_HAS_LAYOUT && BJ_HAS_LISTVIEW

static int bj__cmd_query(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    if (argc < 2 || argc > 3) {
        Jim_WrongNumArgs(interp, 1, argv, "sql ?label?");
        return JIM_ERR;
    }
    BimJim *bj = Jim_CmdPrivData(interp);
    (void)bj;

    if (!BJ_LAYOUT) {
        bj__err(interp, "no layout");
        return JIM_ERR;
    }
    GuiCanvas *bj_cv2 = gui_get_canvas_data(BJ_CANVAS);
    if (!bj_cv2 || !bj_cv2->db.handle) {
        bj__err(interp, "no database");
        return JIM_ERR;
    }

    const char *sql   = Jim_GetString(argv[1], NULL);
    const char *label = (argc == 3) ? Jim_GetString(argv[2], NULL) : "Query";

    CairoTabPane *tp = cl_tabpane_get(BJ_LAYOUT);
    if (!tp) {
        bj__err(interp, "no tabpane");
        return JIM_ERR;
    }

    /* Vérifier que le tabpane HWND est valide */
    if (!tp->hwnd || !IsWindow(tp->hwnd)) {
        bj__err(interp, "tabpane hwnd invalid");
        return JIM_ERR;
    }

    /* S'assurer que le splitter est ouvert */
    if (!BJ_LAYOUT->transcript_vis)
        cl_show_transcript(BJ_LAYOUT);

    /* Créer la listview dans le tabpane */
    bj__log(bj, CT_INFO, "creating listview...");
    BimListView *lv = blv_create(tp->hwnd, 0, 0, 1, 1, bj_cv2->db.handle);
    if (!lv) {
        bj__err(interp, "failed to create listview");
        return JIM_ERR;
    }

    bj__log(bj, CT_INFO, "setting query...");
    blv_set_query(lv, sql);

    /* Ouvrir le tab */
    bj__log(bj, CT_INFO, "adding tab...");
    int idx = cl_tabpane_add(BJ_LAYOUT, label, blv_get_hwnd(lv));
    if (idx < 0) {
        blv_destroy(lv);
        bj__err(interp, "failed to add tab");
        return JIM_ERR;
    }
    cl_tabpane_select(BJ_LAYOUT, idx);

    /* Log dans le transcript */
    char buf[256];
    snprintf(buf, sizeof buf, "%d lignes — %s", blv_row_count(lv), label);
    bj__log(bj, CT_OK, buf);

    /* Retourner le nombre de lignes */
    Jim_SetResultInt(interp, blv_row_count(lv));
    return JIM_OK;
}

#else

static int bj__cmd_query(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    (void)argc; (void)argv;
    bj__err(interp, "bim::query requires cairo_layout.h and bim_listview.h");
    return JIM_ERR;
}

#endif /* BJ_HAS_LAYOUT && BJ_HAS_LISTVIEW */

/* ── bim::layer ?idx? ───────────────────────────────────────────── */
static int bj__cmd_layer(Jim_Interp *interp, int argc, Jim_Obj *const *argv)
{
    BimJim *bj = Jim_CmdPrivData(interp);
    (void)bj;

    if (argc == 1) {
        /* retourner le layer courant — via canvas userdata */
        /* pour l'instant retourner 0, à connecter à gui_get_canvas_data */
        Jim_SetResultInt(interp, 0);
        return JIM_OK;
    }
    Jim_WrongNumArgs(interp, 1, argv, "?idx?");
    return JIM_ERR;
}

/* ═══════════════════════════════════════════════════════════════════
   ENREGISTREMENT DES COMMANDES
   ═══════════════════════════════════════════════════════════════════ */

static void bj__register_commands(BimJim *bj)
{
    Jim_Interp *i = bj->interp;

    /* namespace bim — créer via eval */
    Jim_Eval(i, "namespace eval bim {}");

    /* enregistrer les commandes dans le namespace bim:: */
    Jim_CreateCommand(i, "bim::log",   bj__cmd_log,   bj, NULL);
    Jim_CreateCommand(i, "bim::warn",  bj__cmd_warn,  bj, NULL);
    Jim_CreateCommand(i, "bim::error", bj__cmd_error, bj, NULL);
    Jim_CreateCommand(i, "bim::eval",  bj__cmd_eval,  bj, NULL);
    Jim_CreateCommand(i, "bim::query", bj__cmd_query, bj, NULL);
    Jim_CreateCommand(i, "bim::layer", bj__cmd_layer, bj, NULL);

    /* variable globale db_path accessible depuis les scripts */
    Jim_Eval(i, "set bim::version \"0.1\"");
}

/* ═══════════════════════════════════════════════════════════════════
   API PUBLIQUE — IMPLÉMENTATION
   ═══════════════════════════════════════════════════════════════════ */

void bj_set_context(CairoLayout *layout, HWND canvas)
{
    bj__g_layout = (void*)layout;
    bj__g_canvas = (void*)canvas;
}

BimJim *bj_create(CairoLayout *layout, HWND canvas, void *db)
{
    /* layout, canvas et db sont ignorés ici —
     * les commandes bim::* accèdent aux globaux de bim.c directement */
    bj_set_context(layout, canvas);
    (void)db;

    BimJim *bj = (BimJim*)calloc(1, sizeof *bj);
    if (!bj) return NULL;

    bj->interp = Jim_CreateInterp();
    if (!bj->interp) { free(bj); return NULL; }

    Jim_RegisterCoreCommands(bj->interp);
    Jim_InitStaticExtensions(bj->interp);

    bj__register_commands(bj);

    return bj;
}

void bj_destroy(BimJim *bj)
{
    if (!bj) return;
    if (bj->interp) Jim_FreeInterp(bj->interp);
    free(bj);
}

int bj_eval(BimJim *bj, const char *script)
{
    if (!bj || !script) return -1;

    int rc = Jim_Eval(bj->interp, script);

    const char *result = Jim_GetString(Jim_GetResult(bj->interp), NULL);

    if (rc == JIM_OK) {
        /* Afficher le résultat seulement s'il est non vide */
        if (result && result[0] != '\0')
            bj__log(bj, CT_OK, result);
    } else {
        /* Erreur — afficher le message d'erreur */
        if (result && result[0] != '\0')
            bj__log(bj, CT_ERR, result);
        else
            bj__log(bj, CT_ERR, "script error");
    }

    return rc;
}

int bj_load_file(BimJim *bj, const char *path)
{
    if (!bj || !path) return -1;

    int rc = Jim_EvalFile(bj->interp, path);

    if (rc != JIM_OK) {
        const char *err = Jim_GetString(Jim_GetResult(bj->interp), NULL);
        char buf[512];
        snprintf(buf, sizeof buf, "Error loading %s: %s", path, err ? err : "?");
        bj__log(bj, CT_ERR, buf);
    } else {
        char buf[256];
        snprintf(buf, sizeof buf, "Loaded: %s", path);
        bj__log(bj, CT_INFO, buf);
    }

    return rc;
}

void bj_load_startup(BimJim *bj)
{
    /* Chercher bim_startup.tcl dans le répertoire courant */
    static const char *candidates[] = {
        "bim_startup.tcl",
        "scripts/bim_startup.tcl",
        "scripting/bim_startup.tcl",
        NULL
    };

    for (int i = 0; candidates[i]; i++) {
        FILE *f = fopen(candidates[i], "r");
        if (f) {
            fclose(f);
            bj_load_file(bj, candidates[i]);
            return;
        }
    }
    /* Pas de startup — silencieux */
}

Jim_Interp *bj_interp(BimJim *bj)
{
    return bj ? bj->interp : NULL;
}

#endif /* BIM_JIM_IMPLEMENTATION */
#endif /* BIM_JIM_H */

/*
 * ════════════════════════════════════════════════════════════════════
 * GUIDE D'INTÉGRATION dans bim.c
 * ════════════════════════════════════════════════════════════════════
 *
 *  1. INCLUDES (ordre strict)
 *
 *     #define CAIRO_TRANSCRIPT_IMPLEMENTATION
 *     #include "ui/cairo_transcript.h"
 *     #define CAIRO_TABPANE_IMPLEMENTATION
 *     #include "ui/cairo_tabpane.h"
 *     #define CAIRO_LAYOUT_IMPLEMENTATION
 *     #include "ui/cairo_layout.h"
 *     #define BIM_LISTVIEW_IMPLEMENTATION
 *     #include "ui/bim_listview.h"
 *     #define BIM_JIM_IMPLEMENTATION
 *     #include "scripting/bim_jim.h"
 *
 *  2. GLOBAL
 *
 *     static BimJim *g_bj = NULL;
 *
 *  3. WinMain — après cl_attach_tabpane
 *
 *     GuiCanvas *cv = gui_get_canvas_data(BJ_CANVAS);
 *     g_bj = bj_create(g_layout, g_canvas, cv->db.handle);
 *     bj_load_startup(g_bj);
 *
 *  4. on_command — brancher Jim sur le transcript
 *
 *     void on_command(const char *text, void *userdata) {
 *         ct_add(g_layout->transcript, CT_CMD, text);
 *         bj_eval(g_bj, text);
 *     }
 *
 *  5. WM_DESTROY — avant cl_destroy
 *
 *     case WM_DESTROY:
 *         bj_destroy(g_bj);
 *         cl_destroy(g_layout);
 *         PostQuitMessage(0);
 *         return 0;
 *
 *  6. Makefile — ajouter jim.c dans SRCS
 *
 *     SRCS     = bim.c scripting/jimtcl/jim.c
 *     INCLUDES += -I ./scripting/jimtcl
 *
 * ════════════════════════════════════════════════════════════════════
 * SCRIPTS TCL D'EXEMPLE
 * ════════════════════════════════════════════════════════════════════
 *
 *  # bim_startup.tcl — chargé automatiquement au démarrage
 *  bim::log "BIM scripting ready (Jim $jim_version)"
 *
 *  # Ouvrir une query dans un tab
 *  bim::query "SELECT id, nom, x, y FROM bim_nodes" "Noeuds"
 *
 *  # Query avec filtre
 *  bim::query "SELECT * FROM bim_entities WHERE layer_id = 1" "Layer 1"
 *
 *  # Proc TCL réutilisable
 *  proc show_layer {id} {
 *      bim::query "SELECT * FROM bim_entities WHERE layer_id = $id" "Layer $id"
 *  }
 *  show_layer 2
 *
 *  # Modification de données
 *  bim::eval "UPDATE bim_nodes SET nom = 'Noeud_A' WHERE id = 1"
 *  bim::log  "Noeud renommé"
 *
 * ════════════════════════════════════════════════════════════════════
 */
