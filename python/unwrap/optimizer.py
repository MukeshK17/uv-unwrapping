import itertools
import os
import sys

try:
    from .bindings import load_mesh, unwrap
    from .metrics import compute_angle_distortion, compute_coverage, compute_stretch
except ImportError:
    sys.path.append(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    from uvwrap.bindings import load_mesh, unwrap
    from uvwrap.metrics import compute_angle_distortion, compute_coverage, compute_stretch

def optimize_parameters(mesh_path, target_metric='stretch', verbose=True):
    """
    Find optimal unwrapping parameters for a mesh

    Uses grid search over parameter space to minimize/maximize target metric.

    Args:
        mesh_path: Path to mesh file
        target_metric: Metric to optimize ('stretch', 'coverage', 'angle_distortion')
        verbose: Print progress

    Returns:
        tuple: (best_params, best_score)
            best_params: Dictionary of parameters
            best_score: Best metric value achieved
    """
    angle_thresholds = [20, 30, 40, 50]  # degrees
    min_island_sizes = [5, 10, 20, 50]   # faces

    maximize = (target_metric == 'coverage')
    best_score = float('-inf') if maximize else float('inf')
    best_params = None

    total_combinations = len(angle_thresholds) * len(min_island_sizes)
    current = 0

    if verbose:
        print(f"Testing {total_combinations} parameter combinations...")
        print(f"Target metric: {target_metric}\n")

    mesh = load_mesh(mesh_path)

    for angle, min_size in itertools.product(angle_thresholds, min_island_sizes):
        current += 1

        params = {
            'angle_threshold': float(angle),
            'min_island_faces': int(min_size),
            'pack_islands': True,
            'island_margin': 0.02
        }
        
        try:
            result_mesh, _ = unwrap(mesh, params)
            
            score = 0.0
            if target_metric == 'stretch':
                score = compute_stretch(result_mesh, result_mesh.uvs)
            elif target_metric == 'coverage':
                score = compute_coverage(result_mesh.uvs, result_mesh.triangles)
            elif target_metric == 'angle_distortion':
                score = compute_angle_distortion(result_mesh, result_mesh.uvs)
            else:
                raise ValueError(f"Unknown metric: {target_metric}")

            is_better = False
            if maximize:
                if score > best_score: is_better = True
            else:
                if score < best_score: is_better = True
            
            if is_better:
                best_score = score
                best_params = params.copy()
            
            if verbose:
                marker = "*" if is_better else ""
                print(f"{angle:<10} {min_size:<10} | {score:<10.4f} {marker}")

        except Exception as e:
            if verbose:
                print(f"{angle:<10} {min_size:<10} | FAILED: {str(e)}")

    if verbose:
        print("-" * 50)
        print("Optimization Complete.")
        print(f"Best Score: {best_score:.4f}")
        print(f"Best Params: {best_params}")
        
    return best_params, best_score


if __name__ == "__main__":
    test_file = "test_opt_cube.obj"
    
    if not os.path.exists(test_file):
        with open(test_file, 'w') as f:
            f.write("# Dummy Cube\n") 

    try:
        print("Running Optimizer Test (Mock Mode)...")
        best_p, best_s = optimize_parameters(
            test_file,
            target_metric='stretch',
            verbose=True
        )
    except Exception as e:
        print(f"Optimizer failed: {e}")
    finally:
        if os.path.exists(test_file):
            os.remove(test_file)