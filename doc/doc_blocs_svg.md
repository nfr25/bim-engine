# Blocs SVG — Assets connectables pour SIG métier

> Brainstorming architecture — blocs orientables avec ports de connexion  
> Stack : Cairo + librsvg + SQLite + Lua (optionnel)

---

## Concept général

Un **asset** est un bloc symbolique positionné sur le canvas :
- Dessin vectoriel SVG (vanne, interrupteur, transformateur...)
- Orientable librement ou par pas de 90°
- Points de connexion sur la bounding box
- Règles de connexion métier (optionnellement scriptées en Lua)

```
        N
        │
NW ─────┼───── NE
        │
W ──────+──────E      + = centre de l'asset
        │
SW ─────┼───── SE
        │
        S
```

---

## Architecture des fichiers

```
Cairo          → rendu, rotation, export PDF    (C, figé)
librsvg        → interprétation SVG             (bibliothèque)
SQLite         → stockage, topologie, RTree     (C, figé)
Lua (optionnel)→ règles métier, connectivité    (dynamique)
```

---

## Workflow de création d'un symbole

```
Inkscape / Boxy SVG
    │
    │  dessiner centré sur 0,0
    │  viewBox="-25 -25 50 50"
    ▼
svgo symbol.svg -o symbol_clean.svg    ← optionnel, nettoie le SVG
    │
    ▼
INSERT INTO symbols (name, svg_blob, width, height, ports_mask)
VALUES ('Vanne 2 voies', '<svg...>', 20.0, 20.0, 0x44);
```

**Règle de normalisation SVG :**
- Centré sur `0,0`
- `viewBox="-W -H W H"` avec W=demi-largeur, H=demi-hauteur
- Pas de couleurs hardcodées — utiliser `currentColor` pour que le style Cairo s'applique

---

## Ports de connexion

### Les 8 positions possibles

```c
typedef enum {
    PORT_N  = 0,   // Nord
    PORT_NE = 1,   // Nord-Est
    PORT_E  = 2,   // Est
    PORT_SE = 3,   // Sud-Est
    PORT_S  = 4,   // Sud
    PORT_SW = 5,   // Sud-Ouest
    PORT_W  = 6,   // Ouest
    PORT_NW = 7    // Nord-Ouest
} PortPos;

// Offsets locaux normalisés (×width ou ×height)
static const double PORT_OX[] = { 0,  1,  1,  1,  0, -1, -1, -1 };
static const double PORT_OY[] = {-1, -1,  0,  1,  1,  1,  0, -1 };
```

### Bitmask des ports actifs

```c
// Un entier suffit pour décrire les ports d'un symbole
// bit N = port N actif

#define PORT_BIT(p)  (1 << (p))

// Exemples
// Interrupteur  : E + W          = PORT_BIT(E) | PORT_BIT(W) = 0x44
// Vanne 2 voies : E + W          = 0x44
// Vanne 3 voies : N + E + W      = 0x45
// Nœud réseau   : N + E + S + W  = 0x55
// Transformateur: N + S          = 0x11
// Coin (angle)  : E + S          = 0x14
```

### Calcul des coords monde d'un port

```c
void asset_port_world(Asset *a, PortPos port,
                      double *wx, double *wy)
{
    double lx = PORT_OX[port] * a->sym->width;
    double ly = PORT_OY[port] * a->sym->height;

    // Appliquer la rotation de l'asset
    double cos_a = cos(a->angle);
    double sin_a = sin(a->angle);
    *wx = a->x + lx * cos_a - ly * sin_a;
    *wy = a->y + lx * sin_a + ly * cos_a;
}
```

> La rotation de l'asset fait **automatiquement** tourner tous ses ports —  
> les coords locales sont fixes, seule la transformation Cairo change.

---

## Schéma de base de données

```sql
-- Bibliothèque de symboles
CREATE TABLE symbols (
    id          INTEGER PRIMARY KEY,
    name        TEXT NOT NULL,
    svg_blob    TEXT,           -- SVG normalisé centré 0,0
    width       REAL,           -- demi-largeur en unités monde
    height      REAL,           -- demi-hauteur en unités monde
    ports_mask  INTEGER,        -- bitmask 8 bits des ports actifs
    port_max    INTEGER DEFAULT 1,  -- connexions max par port (global)
    lua_script  TEXT,           -- règles métier optionnelles (Lua)
    builtin_id  INTEGER         -- si symbole C hardcodé (NULL sinon)
);

-- Assets placés sur le canvas
CREATE TABLE assets (
    id          INTEGER PRIMARY KEY,
    layer_id    INTEGER REFERENCES layers(id),
    symbol_id   INTEGER REFERENCES symbols(id),
    x           REAL NOT NULL,
    y           REAL NOT NULL,
    angle       REAL DEFAULT 0,    -- radians
    -- Bounding box précalculée pour RTree
    bbox_minx   REAL,
    bbox_maxx   REAL,
    bbox_miny   REAL,
    bbox_maxy   REAL
);

-- Index spatial pour les assets
CREATE VIRTUAL TABLE assets_rtree USING rtree(
    id, bbox_minx, bbox_maxx, bbox_miny, bbox_maxy
);

-- Connexions port → extrémité de polyligne
CREATE TABLE connections (
    id          INTEGER PRIMARY KEY,
    asset_id    INTEGER REFERENCES assets(id),
    port_pos    INTEGER,            -- PortPos enum (0..7)
    object_id   INTEGER REFERENCES objects(id),
    end_point   INTEGER             -- 0=début polyligne, 1=fin
);
```

---

## Rendu Cairo

```c
void asset_draw(cairo_t *cr, GuiCanvas *cv, Asset *a)
{
    // Monde → écran
    double sx, sy;
    wts(cv, a->x, a->y, &sx, &sy);
    double scale = cv->zoom;

    cairo_save(cr);
    cairo_translate(cr, sx, sy);
    cairo_scale(cr, scale, scale);
    cairo_rotate(cr, a->angle);

    // Symbole SVG via librsvg
    rsvg_handle_render_cairo(a->sym->rsvg, cr);

    // Points de connexion
    for (int p = 0; p < 8; p++) {
        if (!(a->sym->ports_mask & (1 << p))) continue;

        double lx = PORT_OX[p] * a->sym->width;
        double ly = PORT_OY[p] * a->sym->height;

        int connected = asset_port_connection_count(a, p);
        int full      = (connected >= a->sym->port_max);

        // Vert = libre, Rouge = saturé, Bleu = survolé
        if (full)
            cairo_set_source_rgba(cr, 1.0, 0.2, 0.2, 0.9);
        else
            cairo_set_source_rgba(cr, 0.2, 1.0, 0.4, 0.9);

        cairo_arc(cr, lx, ly, 3.5 / scale, 0, 2*M_PI);
        cairo_fill(cr);
    }

    cairo_restore(cr);
}
```

---

## Intégration librsvg (MSYS2)

```bash
pacman -S mingw-w64-x86_64-librsvg
```

```c
#include <librsvg/rsvg.h>

// Charger un SVG depuis un BLOB en mémoire
RsvgHandle *rsvg_from_blob(const char *svg_text, gsize len)
{
    GError *err = NULL;
    RsvgHandle *h = rsvg_handle_new_from_data(
        (const guint8*)svg_text, len, &err);
    if (!h) {
        // gérer erreur
        return NULL;
    }
    return h;
}

// Libérer
g_object_unref(rsvg_handle);
```

```makefile
CFLAGS  += $(shell pkg-config --cflags librsvg-2.0)
LDFLAGS += $(shell pkg-config --libs   librsvg-2.0)
```

---

## Rotation — angle libre vs discret

| Mode          | Avantages                        | Inconvénients                    |
|---------------|----------------------------------|----------------------------------|
| **Discret**   | Snap automatique, ports alignés  | Moins flexible                   |
| 0/90/180/270° | Picking simple, DB propre        | Certains métiers le refusent     |
| **Libre**     | Flexible, tous les métiers       | Snap magnétisme plus complexe    |
| n'importe quel angle | Ports toujours corrects  | Bounding box dynamique           |

> Recommandation : commencer discret, prévoir le champ `angle REAL` dès le début.  
> Passer en libre est une évolution non-cassante côté DB.

---

## Extension Lua — règles métier dynamiques

### Pourquoi Lua

Les règles de connexion sont **métier-dépendantes** :
- Électricité : section de câble, polarité, norme
- Eau : diamètre, pression, sens de flux
- Gaz : pression max, type de fluide
- Télécom : nombre de fibres, atténuation

Un script Lua par symbole permet d'adapter sans recompiler.

### Format du script (stocké dans `symbols.lua_script`)

```lua
-- Exemple : vanne avec contrainte de pression
symbol = {
    name   = "Vanne 2 voies",
    width  = 20, height = 20,

    ports = {
        { pos = "E", max = 1, accepts = {"eau", "gaz"} },
        { pos = "W", max = 1, accepts = {"eau", "gaz"} },
    },

    -- Appelée avant chaque connexion
    on_connect = function(self, port, line)
        if line.pressure > 10.0 then
            return false, "Pression trop élevée (max 10 bar)"
        end
        if line.fluid ~= "eau" and line.fluid ~= "gaz" then
            return false, "Fluide non supporté"
        end
        return true
    end,

    -- Propriétés éditables dans le property editor
    properties = {
        { name = "diametre", type = "float", unit = "mm",  default = 25  },
        { name = "pression", type = "float", unit = "bar", default = 6.0 },
        { name = "normalisee", type = "bool",              default = true },
    }
}
```

### Intégration C minimale

```c
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

// MSYS2 : pacman -S mingw-w64-x86_64-lua
// Lua single-header alternatif : lua-amalg.h

int asset_can_connect(lua_State *L, Asset *a, int port, Line *line)
{
    lua_getglobal(L, "symbol");
    lua_getfield(L, -1, "on_connect");
    if (!lua_isfunction(L, -1)) return 1;  // pas de règle = OK

    lua_pushinteger(L, port);

    // Pousser les propriétés de la ligne comme table Lua
    lua_newtable(L);
    lua_pushnumber(L, line->pressure);
    lua_setfield(L, -2, "pressure");
    lua_pushstring(L, line->fluid);
    lua_setfield(L, -2, "fluid");

    if (lua_pcall(L, 2, 2, 0) != LUA_OK) return 1;  // erreur = permissif

    int ok = lua_toboolean(L, -2);
    // const char *msg = lua_tostring(L, -1);  // message d'erreur si !ok
    lua_pop(L, 2);
    return ok;
}
```

### Disponibilité

```bash
# Lua complet via MSYS2
pacman -S mingw-w64-x86_64-lua

# Lua single-header (amalgamation officielle)
# https://github.com/lua/lua  → make amalg
```

---

## Feuille de route suggérée

```
Phase 1 — Blocs statiques
  [ ] Structure Asset + Symbol en C
  [ ] Rendu SVG via librsvg
  [ ] Ports bitmask, calcul coords monde
  [ ] Picking assets (RTree SQLite)
  [ ] Placement + rotation 90°

Phase 2 — Connectivité
  [ ] Snap magnétisme sur les ports
  [ ] Table connections en DB
  [ ] Visualisation connexions (libre/saturé)
  [ ] Contraintes max par port

Phase 3 — Extensibilité
  [ ] Rotation libre
  [ ] Éditeur de symboles (SVG externe → import)
  [ ] Lua pour règles métier
  [ ] Property editor par asset (brancher cairo_style_editor)
```

---

## Notes

- `librsvg` est la solution la plus propre pour Cairo — même pipeline de rendu
- `svgo` (npm) nettoie les SVG Inkscape avant import en DB
- Les ports sur la bounding box couvrent 95% des besoins réels
- Le bitmask permet de changer les ports actifs sans modifier la structure
- Lua devient nécessaire quand un client a des règles différentes du voisin
