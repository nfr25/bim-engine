# BIM Scripting — Guide TCL (Jim 0.84)
> Interpréteur Jim TCL embarqué dans BIM-engine  
> Accessible depuis le transcript (zone basse du splitter)

---

## Démarrage rapide

Le transcript est une console TCL/SQL hybride :
- Si la commande est reconnue par Jim → exécutée comme TCL
- Sinon → transmise à SQLite comme requête SQL

```tcl
# TCL
set a 42
bim::log "Bonjour depuis TCL"

# SQL (fallback automatique)
SELECT * FROM bim_layers
```

Le fichier `bim_startup.tcl` à la racine du projet est chargé
automatiquement au démarrage.

---

## Commandes bim::*

### Logging

```tcl
bim::log   "message"    # → transcript en vert  (CT_INFO)
bim::warn  "message"    # → transcript en orange (CT_WARN)
bim::error "message"    # → transcript en rouge  (CT_ERR)
```

### Queries

```tcl
# Ouvre un tab listview avec le résultat de la query
bim::query "SELECT * FROM bim_layers" "Mon label"
bim::query "SELECT * FROM bim_entities"   # label par défaut : "Query"

# Retourne le nombre de lignes
set n [bim::query "SELECT * FROM bim_nodes" "Noeuds"]
bim::log "Trouvé $n entités"
```

### Sélection

```tcl
# Ouvre le tab Sélection (jointure bim_selection + bim_entities + bim_layers)
bim::open_selection
bim::selection          # alias court

# Le tab se rafraîchit automatiquement à chaque changement de sélection
# (via trigger SQLite → EV_SELECT → pub/sub)
```

### Exécution SQL sans résultat

```tcl
# INSERT, UPDATE, DELETE, CREATE...
bim::eval "UPDATE bim_layers SET is_visible = 1 WHERE id = 2"
bim::eval "DELETE FROM bim_selection"
```

### Layer courant

```tcl
set layer [bim::layer]   # retourne l'index du layer courant
```

---

## Système publish/subscribe

### S'abonner à un événement

```tcl
bim::subscribe EV_SELECT {
    bim::log "La sélection a changé"
    bim::open_selection
}

bim::subscribe EV_LAYERS {
    bim::log "Les layers ont changé"
}
```

### Se désabonner

```tcl
bim::revoke EV_SELECT {
    bim::log "La sélection a changé"
}
```

### Publier manuellement

```tcl
bim::publish EV_SELECT       # force la notification
bim::publish MON_EVENT data  # avec données optionnelles
```

### Événements disponibles

| Événement    | Déclencheur                              |
|-------------|------------------------------------------|
| `EV_SELECT` | INSERT ou DELETE dans `bim_selection`    |
| `EV_LAYERS` | à venir — changement de layer           |

---

## Procédures TCL

Jim supporte TCL complet — procédures, variables, boucles :

```tcl
# Procédure réutilisable
proc show_layer {id} {
    set sql "SELECT e.id, e.type FROM bim_entities e WHERE e.layer_id = $id"
    set n [bim::query $sql "Layer $id"]
    bim::log "Layer $id : $n entités"
}
show_layer 1

# Boucle
foreach table {bim_layers bim_entities bim_vertices} {
    bim::query "SELECT * FROM $table" $table
}
```

---

## Chemin de la sélection graphique vers la listview

Voici le chemin complet parcouru quand l'utilisateur sélectionne
des entités sur le canvas :

```
1. CANVAS (bim_canvas.h)
   L'utilisateur clique/dessine une sélection.
   Le canvas INSERT dans bim_selection :
     INSERT INTO bim_selection (entity_id) VALUES (?)

2. TRIGGER SQLite (bim_db_schema.h)
   trg_notify_select_insert se déclenche automatiquement :
     AFTER INSERT ON bim_selection BEGIN
       SELECT bim_notify('EV_SELECT');
     END

3. FONCTION C CUSTOM (bim_db_schema.h)
   bim__notify_func() est appelée par SQLite.
   Elle appelle le callback enregistré :
     bim__notify_cb("EV_SELECT", userdata)
   → La DB ne connaît pas Jim. Elle appelle juste un pointeur
     de fonction générique (NULL si personne ne s'est abonné).

4. CALLBACK bim.c (bim.c)
   on_bim_notify() reçoit l'événement :
     static void on_bim_notify(const char *event, void *ud) {
         if (g_bj) bj_publish(g_bj, event);
     }
   → bim.c fait le pont entre la DB et Jim.
     Il connaît les deux mais les garde séparés.

5. bj_publish (bim_jim.h)
   Évalue dans l'interpréteur Jim :
     bim::publish EV_SELECT
   → Entre dans le monde TCL.

6. bim::publish (TCL — bim_startup.tcl bootstrap)
   Parcourt la liste des subscribers de EV_SELECT
   et exécute chaque script enregistré.

7. SUBSCRIBER (bim_startup.tcl)
   Le script abonné est exécuté :
     bim::subscribe EV_SELECT {
         if {[info exists bim::_sel_tab]} {
             bim::open_selection
         }
     }

8. bim::open_selection (bim_jim.h)
   Si le tab Sélection est déjà ouvert (bim::_sel_tab existe) :
     → blv_refresh(lv)  — recharge la query SQLite
   Si non :
     → crée un BimListView avec BJ_SELECTION_SQL
     → cl_tabpane_add() — ouvre un nouveau tab
     → mémorise l'index dans bim::_sel_tab

9. LISTVIEW (bim_listview.h)
   blv_refresh() exécute :
     SELECT e.id, e.type, l.name AS layer,
            e.layer_id, e.style_id
     FROM bim_selection s
     JOIN bim_entities e ON e.id = s.entity_id
     JOIN bim_layers   l ON l.id = e.layer_id
   et met à jour l'affichage virtual.

10. RÉSULTAT
    Le tab "Sélection" affiche les entités sélectionnées,
    mis à jour automatiquement à chaque changement.
```

### Séparation des responsabilités

```
bim_db_schema.h   ← ne connaît pas Jim
                     publie via BimNotifyCb (pointeur générique)

bim.c             ← connaît Jim et la DB
                     fait le pont : on_bim_notify → bj_publish

bim_jim.h         ← ne connaît pas la DB directement
                     accède via gui_get_canvas_data(BJ_CANVAS)

bim_startup.tcl   ← logique métier en TCL
                     s'abonne aux événements, définit les réactions
```

---

## Exemple bim_startup.tcl complet

```tcl
# bim_startup.tcl

bim::log "BIM scripting ready"

# Rafraîchir la sélection automatiquement
bim::subscribe EV_SELECT {
    if {[info exists bim::_sel_tab]} {
        bim::open_selection
    }
}

# Raccourcis utiles
proc bim::selection {} { bim::open_selection }

proc bim::tables {} {
    bim::query "SELECT name, type FROM sqlite_master WHERE type='table'" "Tables"
}

proc bim::count {table} {
    bim::query "SELECT COUNT(*) as n FROM $table" "Count $table"
}
```

---

## Variables TCL réservées

| Variable          | Usage                                      |
|------------------|--------------------------------------------|
| `bim::version`   | Version du scripting BIM (`"0.1"`)         |
| `bim::_subs`     | Dict interne des subscribers (ne pas modifier) |
| `bim::_sel_tab`  | Index du tab Sélection ouvert (-1 si fermé)|

---

## Ajout d'un nouvel événement

**Côté C** — dans `bim.c`, appeler `bj_publish` quand l'événement se produit :
```c
bj_publish(g_bj, "EV_MON_EVENT");
```

Ou via un trigger SQLite dans `bim_db_schema.h` :
```sql
CREATE TRIGGER IF NOT EXISTS trg_mon_event
AFTER UPDATE ON ma_table BEGIN
  SELECT bim_notify('EV_MON_EVENT');
END;
```

**Côté TCL** — s'abonner dans `bim_startup.tcl` :
```tcl
bim::subscribe EV_MON_EVENT {
    bim::log "Mon événement !"
}
```

C'est tout — aucun autre fichier C à modifier.
