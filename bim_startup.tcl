# bim_startup.tcl — chargé automatiquement au démarrage de BIM

bim::log "BIM scripting ready"

# ── Sélection graphique → listview ───────────────────────────────
# Quand bim_selection change → rafraîchir le tab Sélection
bim::subscribe EV_SELECT {
    if {[info exists bim::_sel_tab]} {
        bim::open_selection
    }
}

# ── Double-clic listview → sélection graphique ───────────────────
# Quand l'utilisateur double-clique une entité dans la listview
# → mettre à jour bim_selection → canvas se redessine
bim::subscribe EV_LIST_DBL {
    global _bim_dbl_id
    if {[info exists _bim_dbl_id]} {
        bim::eval "DELETE FROM bim_selection"
        bim::eval "INSERT INTO bim_selection (entity_id) VALUES ($_bim_dbl_id)"
        bim::refresh_graphic
    }
}

bim::log "subscribers enregistrés"
