# BimEngine-C: High-Performance Spatial BIM/GIS Engine

A lightweight, data-centric geospatial engine written in **Pure C**. This project leverages **SQLite (R-Tree)** for ultra-fast spatial indexing and **Cairo Graphics** for professional-grade vector rendering.

Unlike traditional CAD software, BimEngine-C is built as a high-performance "Data Kernel" where the heavy lifting is done in C, while business logic and styling are delegated to a scripting layer.

## 🚀 Key Features

* **SQLite-Powered Spatial Core**: Deep integration with the R-Tree module for near-instant selection, collision detection, and spatial queries on massive datasets.
* **Universal Topology**: Native management of Nodes, Edges, and Faces using an optimized relational database schema.
* **Smart Pick Engine**: Advanced selection algorithms (Point-pick with tolerance and Rectangle-pick) executed directly via SQL `CASE` dispatching for topological vs. blob distance calculations.
* **Precision Rendering**: Sub-pixel accurate 2D rendering via Cairo, featuring interactive zoom, panning, and "Zoom to Selection" functionality.
* **Scriptable Architecture (WIP)**: Planned integration of the **Wren** programming language, allowing users to define custom business rules, dynamic labels, and conditional symbology without recompiling the engine.

## 🏗️ Technical Architecture

The project follows a "Technological Sandwich" approach to balance raw power with flexibility:

1.  **Core Layer (C / Win32)**: Handles window management, the message loop (mouse/keyboard inputs with SHIFT-support for multi-selection), and the high-frequency render cycle.
2.  **Data Layer (SQLite)**: The single "Source of Truth." The `bim_selection` table acts as a pivot for rendering and analysis tools.
3.  **Graphics Layer (Cairo)**: Professional 2D vector output with seamless World-to-Screen coordinate transformations.
4.  **Extensibility Layer (Wren)**: (Upcoming) An object-oriented scripting layer for dynamic label formatting and metadata-driven styling.

## 🛠️ Tech Stack

* **Language**: C (C11)
* **Database**: SQLite 3 (R-Tree enabled)
* **Graphics**: Cairo Graphics Library
* **Interface**: Win32 API
* **Scripting**: Wren (Embedded VM)

## 📋 Database Schema

The engine relies on a clean, relational structure to bridge the gap between CAD geometry and BIM metadata.

| Table | Purpose |
| :--- | :--- |
| `bim_entities` | Main registry for all Nodes and Lines |
| `bim_spatial_index` | Virtual R-Tree table for spatial queries |
| `bim_blobs` | Storage for complex DXF entities and geometry |
| `bim_selection` | Temporary registry for currently selected IDs |

![DB Schema](doc/db_struct.jpg)

## 📸 Screenshots

![BimEngine Main View](screenshots/main_view.png)
*Current build showing Cairo rendering, spatial selection, and dynamic labels.*

## 🛠 Compilation & Usage

1. Clone the repository: `git clone https://github.com/your-username/bim-engine-c.git`
2. Ensure `sqlite3` and `cairo` development headers are in your path.
3. Compile using your preferred C compiler (GCC, Clang, or MSVC).