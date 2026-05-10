import ctypes
import os
import platform
import time

import bpy
import numpy as np


def load_library():
    # Detect OS and locate the DLL inside the 'uvwrap' subfolder
    system = platform.system()
    lib_name = "uvunwrap.dll" if system == "Windows" else "libuvunwrap.so"
    
    # Path: part3_blender/autouv/uvwrap/uvunwrap.dll
    lib_path = os.path.join(os.path.dirname(__file__), "uvwrap", lib_name)
    
    if not os.path.exists(lib_path):
        print(f"[AutoUV] Error: DLL not found at {lib_path}")
        return None

    try:
        # Load the C++ Library
        lib = ctypes.CDLL(lib_path)
        
        # int unwrap_mesh_data(float* coords, int n_verts, int* tris, int n_tris, 
        #                      float* uvs_out, float angle, int min_faces, int pack, float margin)
        lib.unwrap_mesh_data.argtypes = [
            ctypes.POINTER(ctypes.c_float), ctypes.c_int,
            ctypes.POINTER(ctypes.c_int),   ctypes.c_int,
            ctypes.POINTER(ctypes.c_float),
            ctypes.c_float, ctypes.c_int, ctypes.c_int, ctypes.c_float
        ]
        lib.unwrap_mesh_data.restype = ctypes.c_int
        return lib
    except Exception as e:
        print(f"[AutoUV] Library Load Error: {e}")
        return None

def unwrap_object(obj, angle_limit=45.0, margin=0.02):
    """
    Main function called by the Operator.
    Prepares data, calls C++, and writes UVs back to Blender.
    """
    if obj.type != 'MESH':
        return {'CANCELLED'}
    
    mesh = obj.data
    
    # Ensure we are in Object Mode to read data safely
    if obj.mode != 'OBJECT':
        bpy.ops.object.mode_set(mode='OBJECT')

    print(f"[AutoUV] Extracting mesh data for {obj.name}...")
    start_time = time.time()
    
    # 1. Extract Data using Numpy (High Performance)
    # Vertices
    num_verts = len(mesh.vertices)
    coords = np.zeros(num_verts * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", coords)
    
    # Triangles (Loop Triangles are required for UV mapping)
    mesh.calc_loop_triangles()
    num_tris = len(mesh.loop_triangles)
    tris = np.zeros(num_tris * 3, dtype=np.int32)
    mesh.loop_triangles.foreach_get("vertices", tris)
    
    # Prepare output buffer for UVs (u, v per vertex)
    uvs_out = np.zeros(num_verts * 2, dtype=np.float32)

    # 2. Load and Call C++ Library
    lib = load_library()
    if not lib:
        return {'CANCELLED'}

    print(f"[AutoUV] Running C++ Engine on {num_verts} vertices...")
    
    # Call the C++ function
    res = lib.unwrap_mesh_data(
        coords.ctypes.data_as(ctypes.POINTER(ctypes.c_float)), num_verts,
        tris.ctypes.data_as(ctypes.POINTER(ctypes.c_int)), num_tris,
        uvs_out.ctypes.data_as(ctypes.POINTER(ctypes.c_float)),
        float(angle_limit), 
        5,    # min_island_faces
        1,    # pack_islands (True)
        float(margin)
    )
    
    print(f"[AutoUV] C++ finished in {time.time() - start_time:.4f}s")

    if res != 1:
        print("[AutoUV] Error: C++ unwrapping returned failure.")
        return {'CANCELLED'}

    # 3. Apply UVs back to Blender
    if not mesh.uv_layers:
        mesh.uv_layers.new(name="AutoUV")
    
    uv_layer = mesh.uv_layers.active.data
    
    # Blender stores UVs per loop (corner), but is calculated them per vertex.
    # Broadcasting vertex UVs to their corresponding loops.
    loop_vert_indices = np.zeros(len(mesh.loops), dtype=np.int32)
    mesh.loops.foreach_get("vertex_index", loop_vert_indices)
    
    # Reshape linear [u,v,u,v...] to [[u,v], [u,v]...]
    vertex_uvs = uvs_out.reshape((-1, 2))
    
    # Map vertex UVs to loops
    final_uvs = vertex_uvs[loop_vert_indices].flatten()
    
    # Bulk write to Blender (Instant)
    uv_layer.foreach_set("uv", final_uvs)
    
    mesh.update()
    
    return {'FINISHED'}