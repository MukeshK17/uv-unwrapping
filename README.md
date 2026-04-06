# UV Unwrapping for 3D Meshes

This project implements an automatic UV unwrapping pipeline for 3D meshes, combining a C++ core engine with Python bindings and optional Blender integration.

## Overview

UV unwrapping maps a 3D surface onto a 2D plane for texture mapping.  
This implementation focuses on LSCM (Least Squares Conformal Maps) and mesh processing.

## Features

- C++ UV parameterization engine
- Mesh topology and seam handling
- Python bindings for scripting
- (Planned) Blender add-on integration
- UV visualization and analysis

## Project Structure
```
uv-unwrapping/
│
├── src/        # Core C++ implementation
├── python/     # Python bindings
├── blender/    # Blender add-on (WIP)
├── examples/   # Sample meshes (self-provided)
├── results/    # Outputs
```
## Build

mkdir build
cd build
cmake ..
make

## Notes

This project was inspired by a technical assignment provided by Mixar (2025).

No original assignment materials, datasets, or starter code are included.
All implementations are independent.

## Author

Mukesh Dewangan
