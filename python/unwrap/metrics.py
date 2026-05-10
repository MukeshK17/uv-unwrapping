import numpy as np
import math

def compute_stretch(mesh, uvs):
    """
    Compute maximum stretch across all triangles

    Stretch measures how much the UV mapping distorts the mesh.

    Algorithm:
    For each triangle:
        1. Build Jacobian J (UV → 3D mapping)
        2. Compute singular values σ1, σ2 via SVD
        3. stretch = max(σ1/σ2, σ2/σ1)

    Args:
        mesh: Mesh object with vertices and triangles
        uvs: UV coordinates (N, 2) array

    Returns:
        float: Maximum stretch value
    """
    max_stretch = 1.0 
    verts = mesh.vertices
    tris = mesh.triangles

    for tri in tris:
        p0, p1, p2 = verts[tri[0]], verts[tri[1]], verts[tri[2]]
        uv0, uv1, uv2 = uvs[tri[0]], uvs[tri[1]], uvs[tri[2]]

        dp1 = p1 - p0
        dp2 = p2 - p0
        duv1 = uv1 - uv0
        duv2 = uv2 - uv0

        S = np.column_stack((duv1, duv2))
        Q = np.column_stack((dp1, dp2))
        
        try:
            if abs(np.linalg.det(S)) < 1e-9:
                continue  
            
            S_inv = np.linalg.inv(S)
            J = Q @ S_inv

            _, sigmas, _ = np.linalg.svd(J)
            sigma1, sigma2 = sigmas[0], sigmas[1]

            if sigma2 < 1e-9:
                if sigma1 < 1e-9:
                    current_stretch = 1.0 
                else:
                    current_stretch = 1e6
            else:
                current_stretch = sigma1 / sigma2
                if current_stretch < 1.0:
                    current_stretch = 1.0 / current_stretch
                    
            if current_stretch > max_stretch:
                max_stretch = current_stretch
                
        except np.linalg.LinAlgError:
            continue

    return float(max_stretch)


def compute_coverage(uvs, triangles, resolution=1024):
    """
    Compute UV coverage (percentage of [0,1]² covered)

    Algorithm:
    1. Rasterize UVs to resolution×resolution grid
    2. Mark pixels covered by triangles
    3. Return percentage of pixels covered

    Args:
        uvs: UV coordinates (N, 2) array
        triangles: Triangle indices (M, 3) array
        resolution: Grid resolution (default 1024)

    Returns:
        float: Coverage (0.0 to 1.0)
    """
    grid = np.zeros((resolution, resolution), dtype=bool)
    uv_grid = uvs * resolution 

    def rasterize_triangle(grid, t0, t1, t2):
        min_x = int(np.floor(min(t0[0], t1[0], t2[0])))
        max_x = int(np.ceil(max(t0[0], t1[0], t2[0])))
        min_y = int(np.floor(min(t0[1], t1[1], t2[1])))
        max_y = int(np.ceil(max(t0[1], t1[1], t2[1])))

        min_x = max(0, min_x)
        max_x = min(resolution, max_x)
        min_y = max(0, min_y)
        max_y = min(resolution, max_y)

        v0 = t1 - t0
        v1 = t2 - t0
        
        d00 = np.dot(v0, v0)
        d01 = np.dot(v0, v1)
        d11 = np.dot(v1, v1)
        denom = d00 * d11 - d01 * d01
        
        if abs(denom) < 1e-9: 
            return 

        invDenom = 1.0 / denom

        for y in range(min_y, max_y):
            for x in range(min_x, max_x):
                    p = np.array([x + 0.5, y + 0.5])
                    
                    v2 = p - t0
                    d20 = np.dot(v2, v0)
                    d21 = np.dot(v2, v1)
                    
                    v = (d11 * d20 - d01 * d21) * invDenom
                    w = (d00 * d21 - d01 * d20) * invDenom
                    u = 1.0 - v - w
                    
                    if v >= 0 and w >= 0 and u >= 0:
                        grid[y, x] = True

    for tri in triangles:
        t0 = uv_grid[tri[0]]
        t1 = uv_grid[tri[1]]
        t2 = uv_grid[tri[2]]
        rasterize_triangle(grid, t0, t1, t2)

    return np.sum(grid) / (resolution * resolution)


def compute_angle_distortion(mesh, uvs):
    """
    Compute maximum angle distortion
    
    Measures how much angles change between 3D and UV space.

    Algorithm:
    For each triangle:
        Compute 3 angles in 3D
        Compute 3 angles in UV
        Find max |angle_3d - angle_uv|

    Args:
        mesh: Mesh object
        uvs: UV coordinates (N, 2) array

    Returns:
        float: Max angle distortion (radians)
    """
    max_angle_distortion = 0.0
    verts = mesh.vertices
    tris = mesh.triangles
    
    def compute_triangle_angles_3d(p0, p1, p2):
        angles = []
        points = [p0, p1, p2]

        for i in range(3):
            a = points[i]
            b = points[(i + 1) % 3]
            c = points[(i - 1) % 3]

            v1 = b - a
            v2 = c - a

            n1 = np.linalg.norm(v1)
            n2 = np.linalg.norm(v2)

            if n1 < 1e-9 or n2 < 1e-9:
                angles.append(0.0)
                continue

            dot = np.dot(v1, v2) / (n1 * n2)
            dot = np.clip(dot, -1.0, 1.0)  
            angles.append(np.arccos(dot))
            
        return angles

    def compute_triangle_angles_2d(uv0, uv1, uv2):
        angles = []
        points = [uv0, uv1, uv2]

        for i in range(3): 
            a = points[i]
            b = points[(i + 1) % 3]
            c = points[(i - 1) % 3]

            v1 = b - a
            v2 = c - a

            n1 = np.linalg.norm(v1)
            n2 = np.linalg.norm(v2)

            if n1 < 1e-9 or n2 < 1e-9:
                angles.append(0.0)
                continue
                
            dot = np.dot(v1, v2) / (n1 * n2)
            dot = np.clip(dot, -1.0, 1.0)
            angles.append(np.arccos(dot))
            
        return angles

    for tri in tris:
        p0, p1, p2 = verts[tri[0]], verts[tri[1]], verts[tri[2]]
        uv0, uv1, uv2 = uvs[tri[0]], uvs[tri[1]], uvs[tri[2]]

        angles_3d = compute_triangle_angles_3d(p0, p1, p2)
        angles_uv = compute_triangle_angles_2d(uv0, uv1, uv2)

        for a3d, auv in zip(angles_3d, angles_uv):
            diff = abs(a3d - auv)
            if diff > max_angle_distortion:
                max_angle_distortion = diff

    return float(max_angle_distortion)

# Example usage
if __name__ == "__main__":
    vertices = np.array([
        [0, 0, 0],
        [1, 0, 0],
        [0.5, 0.866, 0],
    ], dtype=np.float32)

    triangles = np.array([[0, 1, 2]], dtype=np.int32)

    uvs = np.array([
        [0, 0],
        [1, 0],
        [0.5, 0.866],
    ], dtype=np.float32)

    class SimpleMesh:
        def __init__(self, vertices, triangles):
            self.vertices = vertices
            self.triangles = triangles

    mesh = SimpleMesh(vertices, triangles)

    print(f"Stretch: {compute_stretch(mesh, uvs)}")
    print(f"Coverage: {compute_coverage(uvs, triangles)}")
    print(f"Angle distortion: {compute_angle_distortion(mesh, uvs)}")