"""
Command-line interface for UV unwrapping

Commands:
- unwrap: Unwrap single mesh
- batch: Batch process directory
- optimize: Find optimal parameters
- analyze: Analyze mesh quality
"""

import argparse
import json
import os
import sys
import time
from pathlib import Path

import numpy as np

current_dir = os.path.dirname(os.path.abspath(__file__))
if current_dir not in sys.path:
    sys.path.append(current_dir)
    
try:
    from uvwrap.bindings import load_mesh, save_mesh, unwrap
    from uvwrap.metrics import compute_angle_distortion, compute_coverage, compute_stretch
    from uvwrap.optimizer import optimize_parameters
    from uvwrap.processor import UnwrapProcessor
except ImportError as e:
    print(f"Error importing modules: {e}")
    print("Ensure you are running this script from the 'part2_python' directory.")
    sys.exit(1)


def cmd_unwrap(args):
    """
    Unwrap single mesh
    """
    input_path = Path(args.input)
    output_path = Path(args.output)
    
    if not input_path.exists():
        print(f"Error: Input file '{input_path}' not found.")
        return 1

    print(f"Unwrapping: {input_path.name}")
    start_time = time.time()

    try:
        mesh = load_mesh(str(input_path))
        params = {
            'angle_threshold': args.angle_threshold,
            'min_island_faces': args.min_island,
            'pack_islands': args.pack,
            'island_margin': args.margin
        }
        
        result_mesh, result_stats = unwrap(mesh, params)
        save_mesh(result_mesh, str(output_path))
        duration = time.time() - start_time

        stretch = compute_stretch(result_mesh, result_mesh.uvs)
        coverage = compute_coverage(result_mesh.uvs, result_mesh.triangles)

        print(f"✓ Completed in {duration:.2f}s")
        print(f"  Islands:  {result_stats.get('num_islands', 0)}")
        print(f"  Stretch:  {stretch:.4f}")
        print(f"  Coverage: {coverage:.2%}")
        print(f"  Saved to: {output_path}")
    except Exception as e:
        print(f"Error during unwrap: {e}")
        return 1
        
    return 0


def cmd_batch(args):
    """
    Batch process directory
    """
    input_dir = Path(args.input_dir)
    output_dir = Path(args.output_dir)
    
    if not input_dir.exists():
        print(f"Error: input directory '{input_dir}' not found")
        return 1

    files = list(input_dir.glob("*.obj"))
    if not files:
        print(f"No .obj files found in {input_dir}")
        return 1

    print("UV Unwrapping Batch Processor")
    print("=" * 40)
    print(f"Found {len(files)} files.")
    print(f"Threads: {args.threads or 'Auto'}")
    print("-" * 40)

    processor = UnwrapProcessor(num_threads=args.threads)

    params = {
        'angle_threshold': args.angle_threshold,
        'min_island_faces': 10,
        'pack_islands': True,
        'island_margin': 0.02
    }

    def on_progress(current, total, filename):
        percent = int(100 * current / total)
        bar_len = 30
        filled_len = int(bar_len * current // total)
        bar = '=' * filled_len + '-' * (bar_len - filled_len)
        print(f"\r[{bar}] {percent}% - {filename}   ", end='', flush=True)

    try:
        results = processor.process_batch(
            input_files=[str(f) for f in files],
            output_dir=str(output_dir),
            params=params,
            on_progress=on_progress
        )
        print("\n" + "=" * 40)

        summary = results['summary']
        print("batch complete")
        print(f"Total:      {summary['total_files']}")
        print(f"Success:    {summary['successful']}")
        print(f"Failed:     {summary['failed']}")
        print(f"Avg Time:   {summary['avg_processing_time']:.3f}s")
        print(f"Avg Stretch:{summary['avg_stretch']:.4f}")

        if args.report:
            with open(args.report, 'w') as f:
                json.dump(results, f, indent=2)
            print(f"Report saved to {args.report}")
            
    except Exception as e:
        print(f"\nBatch processing failed: {e}")
        return 1
        
    return 0


def cmd_optimize(args):
    """
    Optimize parameters for a mesh
    """
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file '{input_path}' not found.")
        return 1

    print(f"Optimizing parameters for: {input_path.name}")
    print(f"Target metric: {args.metric}")

    try:
        best_params, best_score = optimize_parameters(
            str(input_path),
            target_metric=args.metric,
            verbose=True
        )

        print("\n" + "=" * 40)
        print("OPTIMAL CONFIGURATION FOUND")
        print(f"Score ({args.metric}): {best_score:.4f}")
        print("-" * 20)
        print(json.dumps(best_params, indent=2))
        print("=" * 40)

        if args.save_params:
            with open(args.save_params, 'w') as f:
                json.dump(best_params, f, indent=2)
            print(f"Parameters saved to {args.save_params}")

        if args.output:
            print(f"Saving optimized mesh to {args.output}...")
            mesh = load_mesh(str(input_path))
            res_mesh, _ = unwrap(mesh, best_params)
            save_mesh(res_mesh, args.output)
            print("Done.")

    except Exception as e:
        print(f"Optimization failed: {e}")
        return 1
        
    return 0


def cmd_analyze(args):
    """
    Analyze mesh quality
    """
    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file '{input_path}' not found.")
        return 1
        
    print(f"Analyzing: {input_path.name}")

    try:
        mesh = load_mesh(str(input_path))
        if mesh.uvs is None or len(mesh.uvs) == 0:
            print("Error: Mesh has no UV coordinates to analyze")
            return 1

        stretch = compute_stretch(mesh, mesh.uvs)
        coverage = compute_coverage(mesh.uvs, mesh.triangles)
        angle_dist = compute_angle_distortion(mesh, mesh.uvs)

        print("-" * 30)
        print(f"{'Metric':<20} | {'Value':<10}")
        print("-" * 30)
        print(f"{'Stretch (>=1.0)':<20} | {stretch:.4f}")
        print(f"{'Coverage (0-1)':<20} | {coverage:.2%}")
        print(f"{'Angle Dist (deg)':<20} | {np.degrees(angle_dist):.2f}°") 
        print("-" * 30)

        print("Assessment:")
        if stretch < 1.2 and angle_dist < np.radians(5):
            print("✓ Excellent quality (Low distortion)")
        elif stretch < 2.0:
            print("⚠ Acceptable quality (Moderate distortion)")
        else:
            print("✗ Poor quality (High distortion detected)")
            
    except Exception as e:
        print(f"Analysis failed: {e}")
        return 1

    return 0


def main():
    """
    Main CLI entry point
    """
    parser = argparse.ArgumentParser(
        description='UV Unwrapping Tool',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Unwrap single mesh
  python cli.py unwrap input.obj output.obj --angle-threshold 30

  # Unwrap without packing
  python cli.py unwrap input.obj output.obj --no-pack

  # Batch process
  python cli.py batch meshes/ output/ --threads 8

  # Optimize parameters
  python cli.py optimize mesh.obj --metric stretch --output best.obj

  # Analyze quality
  python cli.py analyze mesh.obj
        """
    )
    
    subparsers = parser.add_subparsers(dest='command', help='Commands')

    # Unwrap command
    unwrap_parser = subparsers.add_parser('unwrap', help='Unwrap single mesh')
    unwrap_parser.add_argument('input', help='Input OBJ file')
    unwrap_parser.add_argument('output', help='Output OBJ file')
    unwrap_parser.add_argument('--angle-threshold', type=float, default=30.0,
                              help='Angle threshold in degrees (default: 30)')
    unwrap_parser.add_argument('--min-island', type=int, default=10,
                              help='Minimum island size in faces (default: 10)')
    unwrap_parser.add_argument('--no-pack', action='store_false', dest='pack',
                              default=True,
                              help='Disable island packing (default: enabled)')
    unwrap_parser.add_argument('--margin', type=float, default=0.02,
                              help='Island margin (default: 0.02)')

    # Batch command
    batch_parser = subparsers.add_parser('batch', help='Batch process directory')
    batch_parser.add_argument('input_dir', help='Input directory')
    batch_parser.add_argument('output_dir', help='Output directory')
    batch_parser.add_argument('--threads', type=int, default=None,
                             help='Number of threads (default: CPU count)')
    batch_parser.add_argument('--angle-threshold', type=float, default=30.0,
                             help='Angle threshold in degrees')
    batch_parser.add_argument('--report', help='Save metrics to JSON file')

    # Optimize command
    opt_parser = subparsers.add_parser('optimize', help='Optimize parameters')
    opt_parser.add_argument('input', help='Input OBJ file')
    opt_parser.add_argument('--output', help='Output OBJ file with best params')
    opt_parser.add_argument('--metric', choices=['stretch', 'coverage', 'angle_distortion'],
                           default='stretch', help='Metric to optimize')
    opt_parser.add_argument('--save-params', help='Save best params to JSON')

    # Analyze command
    analyze_parser = subparsers.add_parser('analyze', help='Analyze mesh quality')
    analyze_parser.add_argument('input', help='Input OBJ file with UVs')

    args = parser.parse_args()

    if not args.command:
        parser.print_help()
        return 1

    # Route to command handlers
    if args.command == 'unwrap':
        return cmd_unwrap(args)
    elif args.command == 'batch':
        return cmd_batch(args)
    elif args.command == 'optimize':
        return cmd_optimize(args)
    elif args.command == 'analyze':
        return cmd_analyze(args)

    return 0


if __name__ == '__main__':
    sys.exit(main())