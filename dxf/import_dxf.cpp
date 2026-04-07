#include <iostream>
#include <vector>
#include <string>
#include "dl_dxf.h"
#include "dl_creationadapter.h"
#include "sqlite3.h"

// --- L'adaptateur pour dxflib ---
class MyDxfToSqlite : public DL_CreationAdapter {
private:
    sqlite3* db;
    int layer_id;
    std::vector<double> current_blob;

public:
    MyDxfToSqlite(sqlite3* database, int lid) : db(database), layer_id(lid) {}

    // Quand on croise une ligne (Exemple simple)
    virtual void addLine(const DL_LineData& d) override {
        // Format du BLOB : [X1, Y1, X2, Y2] en double
        double coords[] = {d.x1, d.y1, d.x2, d.y2};
        
        const char* sql = "INSERT INTO bim_objects (layer_id, geom_blob) VALUES (?, ?);";
        sqlite3_stmt* stmt;
        
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, layer_id);
            sqlite3_bind_blob(stmt, 2, coords, sizeof(coords), SQLITE_TRANSIENT);
            sqlite3_step(stmt);
        }
        sqlite3_finalize(stmt);
    }

    // Tu peux ajouter addLWPolyline, addCircle, etc.
};

// --- Main ---
int main(int argc, char* argv[]) {
    if (argc < 3) return -1; // Usage: importer.exe db_path task_id

    const char* db_path = argv[1];
    int task_id = std::stoi(argv[2]);

    sqlite3* db;
    if (sqlite3_open(db_path, &db) != SQLITE_OK) return -1;

    // 1. Lire la mission dans la DB
    std::string file_to_import;
    int target_layer = 0;
    
    std::string query = "SELECT file_path, layer_id FROM import_tasks WHERE id = " + std::to_string(task_id);
    sqlite3_stmt* stmt;
    
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            file_to_import = (const char*)sqlite3_column_text(stmt, 0);
            target_layer = sqlite3_column_int(stmt, 1);
        }
        sqlite3_finalize(stmt);
    }

    // 2. Vérifier si le fichier existe
    FILE* f = fopen(file_to_import.c_str(), "r");
    if (!f) {
        sqlite3_exec(db, ("UPDATE bim_import_tasks SET status = -1 WHERE id = " + std::to_string(task_id)).c_str(), nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return -2;
    }
    fclose(f);

    // 3. Marquer "En cours"
    sqlite3_exec(db, ("UPDATE bim_import_tasks SET status = 1 WHERE id = " + std::to_string(task_id)).c_str(), nullptr, nullptr, nullptr);

    // 4. Parser le DXF
    MyDxfToSqlite handler(db, target_layer);
    DL_Dxf dxf;
    
    // Début de la transaction pour la performance (CRUCIAL pour SQLite)
    sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);
    
    if (!dxf.in(file_to_import, &handler)) {
        sqlite3_exec(db, "ROLLBACK;", nullptr, nullptr, nullptr);
        sqlite3_exec(db, ("UPDATE import_tasks SET status = -1 WHERE id = " + std::to_string(task_id)).c_str(), nullptr, nullptr, nullptr);
    } else {
        sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
        // 5. Marquer "Fini"
        sqlite3_exec(db, ("UPDATE import_tasks SET status = 2 WHERE id = " + std::to_string(task_id)).c_str(), nullptr, nullptr, nullptr);
    }

    sqlite3_close(db);
    return 0;
}