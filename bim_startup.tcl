# bim_startup.tcl — chargé automatiquement au démarrage de BIM
# ──────────────────────────────────────────────────────────────

bim::log "BIM scripting ready"

# ── Gestion de la sélection ───────────────────────────────────
# Quand bim_selection change (INSERT/DELETE via trigger SQLite),
# EV_SELECT est publié → on rafraîchit le tab Sélection.

bim::subscribe EV_SELECT {
    # Si le tab Sélection est déjà ouvert → refresh
    # Sinon → ne pas l'ouvrir automatiquement
    bim::log "selection"
    if {[info exists bim::_sel_tab]} {
        bim::open_selection
    }
}

# Proc utilitaire : ouvrir explicitement la sélection
proc bim::selection {} {
    bim::open_selection
}
