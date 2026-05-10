# UV Unwrapping for 3D Meshes

This project implements a complete, automatic UV unwrapping pipeline for 3D meshes. It combines a high-performance C++ core engine for complex geometric processing with Python bindings for scripting, optimization, and a fully integrated Blender add-on.

## Overview

UV unwrapping maps a 3D surface onto a 2D plane for texture mapping. This implementation focuses on minimizing distortion and maximizing texture space using **Least Squares Conformal Maps (LSCM)** alongside automated seam detection and island packing.

## Features

### Core Engine (C++)
* **LSCM Parameterization:** Solves sparse linear systems (via Eigen) to minimize angle distortion during unwrapping.
* **Automated Seam Detection:** Uses dual-graph spanning trees (BFS) and angular defect analysis to intelligently cut meshes at high-curvature edges.
* **Island Packing:** Implements a shelf-packing algorithm to tightly fit multiple UV islands into a normalized `[0,1]²` texture space.
* **Topology Building:** Automatically extracts unique edges and face adjacency data.

### Python API & CLI
* **Fast Bindings:** Uses `ctypes` to interface directly with the compiled C++ engine.
* **Batch Processing:** Multi-threaded unwrapping for processing entire directories of `.obj` files simultaneously.
* **Grid Search Optimizer:** Automatically tests combinations of angle thresholds and island sizes to find the mathematically optimal unwrap for a specific mesh.
* **Quality Metrics:** Evaluates unwrap quality based on UV Stretch, Coverage (%), and Angle Distortion.

### Blender Integration
* **Native UI Panel:** Accessible directly in the 3D Viewport.
* **Live Preview:** Automatically re-unwraps the mesh in real-time as you modify geometry.
* **Seam Tools:** Custom operators to quickly mark and clear seams.
* **Smart Caching:** Prevents redundant recalculations on unchanged meshes.

## Project Structure

```text
uv-unwrapping/
│
├── blender/                 # Blender add-on integration
│   ├── autouv/              # Add-on source code
│   │   ├── __init__.py
│   │   ├── live_preview.py
│   │   ├── operator.py
│   │   ├── panel.py
│   │   ├── seam_tools.py
│   │   └── wrapper.py
│   └── screenshots/         # UI previews and documentation images
│
├── cpp/                     # Core C++ implementation
│   ├── include/             # C++ Headers
│   │   ├── lscm.h
│   │   ├── math_utils.h
│   │   ├── mesh.h
│   │   ├── seam.h
│   │   ├── topology.h
│   │   └── unwrap.h
│   ├── src/                 # C++ Source files
│   ├── tests/               # C++ Test suite
│   │   └── test_unwrap.cpp
│   └── CMakeLists.txt       # C++ Build configuration
│
├── python/                  # Python CLI and bindings
│   ├── unwrap/              # Python module
│   │   ├── __init__.py
│   │   ├── bindings.py
│   │   ├── metrics.py
│   │   ├── optimizer.py
│   │   └── processor.py
│   ├── cli.py               # Command-line interface entry point
│   └── requirements.txt     # Python dependencies
│
├── test_data/               # Sample meshes for testing
│   ├── 01_cube.obj
│   ├── 02_cylinder.obj
│   ├── 03_sphere.obj
│   └── 04_torus.obj
│
├── .gitignore
├── CMakeLists.txt           # Root build configuration
├── pyproject.toml           # Python package configuration
├── README.md                # Project documentation
└── requirements.txt         # Root dependencies
```
## Build & Installation

### 1. Build the C++ Core
Ensure you have CMake (3.15+) and a C++17 compatible compiler installed.

```bash
mkdir build
cd build
cmake ..
make
```
(Note for Windows users: The CMake script is configured to automatically copy the compiled .dll to the Blender add-on folder upon a successful build).

### 2. Python Setup

Navigate to the root directory and install the required dependencies: 
```
pip install -r requirements.txt
```
3. Blender Add-on

    Zip the ```blender/autouv/``` folder.

    In Blender, go to Edit > Preferences > Add-ons.

    Click Install... and select the zip file.

    Enable "Auto UV Unwrap". The panel will appear in the 3D Viewport sidebar.

## Command Line Usage

The Python CLI offers powerful tools for headless processing. From the python/ directory, run:

Unwrap a single mesh:
```
python cli.py unwrap ../test_data/01_cube.obj output.obj --angle-threshold 30
```
Batch process a directory:
```
python cli.py batch ../test_data/ output_dir/ --threads 8
```
Find the optimal parameters for a specific mesh:
```
python cli.py optimize ../test_data/03_sphere.obj --metric stretch --output best.obj
```
Analyze an existing UV map's quality:
```
python cli.py analyze input.obj
```
## Acknowledgements & Notes

- C++ skeleton headers and initial API structures were provided by Mixar (2025).

- All algorithmic implementations (LSCM, Seam Detection, Packing, Bindings, Blender Add-on) are independent.

## Author

Mukesh Dewangan
