/**
 * @file unwrap.cpp
 * @brief Main UV unwrapping orchestrator
 *
 */

#include "unwrap.h"
#include "lscm.h"
#include <stdlib.h>
#include <stdio.h>
#include <vector>
#include <set>
#include <map>
#include <queue>

/**
 * @brief Extract UV islands after seam cuts
 *
 * Uses connected components algorithm on face graph after removing seam edges.
 *
 * @param mesh Input mesh
 * @param topo Topology info
 * @param seam_edges Array of seam edge indices
 * @param num_seams Number of seams
 * @param num_islands_out Output: number of islands
 * @return Array of island IDs per face
 */
static int* extract_islands(const Mesh* mesh,
                            const TopologyInfo* topo,
                            const int* seam_edges,
                            int num_seams,
                            int* num_islands_out) {
    int num_faces = mesh->num_triangles;
    int* face_island_ids = (int*)malloc(mesh->num_triangles * sizeof(int));

    // Initialize all to -1 (unvisited)
    for (int i = 0; i < mesh->num_triangles; i++) {
        face_island_ids[i] = -1;
    }

    std::set<int> seam_set;
    if (seam_edges) {
        for (int i = 0; i < num_seams; i++) {
            seam_set.insert(seam_edges[i]);
        }
    }

    std::vector<std::vector<int>> face_adj(num_faces);
    int E = topo->num_edges;
    const int* edge_faces = topo->edge_faces;
    
    for (int e = 0; e < E; e++) {
        if (seam_set.find(e) != seam_set.end()) {
            continue;
        }
        int f0 = edge_faces[2*e + 0];
        int f1 = edge_faces[2*e + 1];   

        if (f0 != -1 && f1 != -1) {
            face_adj[f0].push_back(f1);
            face_adj[f1].push_back(f0);
        }
    }

    int island_count = 0;
    for (int start_face = 0; start_face < num_faces; start_face++) {
        if (face_island_ids[start_face] != -1) continue;
        
        int current_island = island_count++;
        face_island_ids[start_face] = current_island;

        std::queue<int> q;
        q.push(start_face);

        while (!q.empty()) {
            int u = q.front();
            q.pop();

            for (int v : face_adj[u]) {
                if (face_island_ids[v] == -1 ) {
                    face_island_ids[v] = current_island;
                    q.push(v);
                }
            }
        }
    }
    
    *num_islands_out = island_count; 

    printf("Extracted %d UV islands\n", *num_islands_out);

    return face_island_ids;
}

/**
 * @brief Copy UVs from island parameterization to result mesh
 */
static void copy_island_uvs(Mesh* result,
                            const float* island_uvs,
                            const int* face_indices,
                            int num_faces,
                            const std::map<int, int>& global_to_local) {
    const int* tris = result->triangles;

    for (int i = 0; i < num_faces; i++) {
        int f = face_indices[i];
        
        for (int j = 0; j < 3; j++) {
            int global_idx = tris[3*f + j];
            auto it = global_to_local.find(global_idx);
            
            if (it != global_to_local.end()) {
                int local_idx = it->second;
                
                result->uvs[2*global_idx]     = island_uvs[2*local_idx];
                result->uvs[2*global_idx + 1] = island_uvs[2*local_idx + 1];
            }
        }
    }
}

Mesh* unwrap_mesh(const Mesh* mesh,
                  const UnwrapParams* params,
                  UnwrapResult** result_out) {
    if (!mesh || !params || !result_out) {
        fprintf(stderr, "unwrap_mesh: Invalid arguments\n");
        return NULL;
    }

    printf("\n=== UV Unwrapping ===\n");
    printf("Input: %d vertices, %d triangles\n",
           mesh->num_vertices, mesh->num_triangles);
    printf("Parameters:\n");
    printf("  Angle threshold: %.1f°\n", params->angle_threshold);
    printf("  Min island faces: %d\n", params->min_island_faces);
    printf("  Pack islands: %s\n", params->pack_islands ? "yes" : "no");
    printf("  Island margin: %.3f\n", params->island_margin);
    printf("\n");

    TopologyInfo* topo = build_topology(mesh);
    if (!topo) {
        fprintf(stderr, "Failed to build topology\n");
        return NULL;
    }
    validate_topology(mesh, topo);

    int num_seams;
    int* seam_edges = detect_seams(mesh, topo, params->angle_threshold, &num_seams);

    int num_islands;
    int* face_island_ids = extract_islands(mesh, topo, seam_edges, num_seams, &num_islands);

    Mesh* result = allocate_mesh_copy(mesh);
    
    if (!result) {
        fprintf(stderr, "Failed to allocate result mesh\n");
        free_topology(topo);
        free(seam_edges);
        free(face_island_ids);
        return NULL;
    }

    // Ensure UVs are allocated
    if (!result->uvs) {
        result->uvs = (float*)calloc(mesh->num_vertices * 2, sizeof(float));
    }
    
    for (int island_id = 0; island_id < num_islands; island_id++) {
        printf("\nProcessing island %d/%d...\n", island_id + 1, num_islands);

        std::vector<int> island_faces;
        for (int f = 0; f < mesh->num_triangles; f++) {
            if (face_island_ids[f] == island_id) {
                island_faces.push_back(f);
            }
        }

        printf("  %d faces in island\n", (int)island_faces.size());

        if ((int)island_faces.size() < params->min_island_faces) {
            printf("  Skipping (too small)\n");
            continue;
        }

        float* island_uvs = lscm_parameterize(mesh, island_faces.data(), (int)island_faces.size());
        
        if (island_uvs) {
            std::map<int, int> global_to_local;
            int local_idx = 0;
            const int* tris = mesh->triangles;
            
            for (int f : island_faces) {
                for (int j = 0; j < 3 ; j++) {
                    int g_idx = tris[3*f + j];
                    if (global_to_local.find(g_idx) == global_to_local.end()) {
                        global_to_local[g_idx] = local_idx++;
                    }
                }
            }
            
            copy_island_uvs(result, island_uvs, island_faces.data(), (int)island_faces.size(), global_to_local);
            free(island_uvs);
        }
    }

    UnwrapResult* result_data = (UnwrapResult*)malloc(sizeof(UnwrapResult));
    result_data->num_islands = num_islands;
    result_data->face_island_ids = face_island_ids; 
    
    result_data->avg_stretch = 0.0f;
    result_data->max_stretch = 0.0f;
    result_data->coverage = 0.0f;

    if (params->pack_islands) {
        pack_uv_islands(result, result_data, params->island_margin);
    }

    compute_quality_metrics(result, result_data);

    *result_out = result_data;

    free_topology(topo);
    free(seam_edges);

    printf("\n=== Unwrapping Complete ===\n");

    return result;
}

void free_unwrap_result(UnwrapResult* result) {
    if (!result) return;

    if (result->face_island_ids) {
        free(result->face_island_ids);
    }
    free(result);
}

// --------------

#ifdef _WIN32
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

extern "C" {
    /**
     * @brief Entry point for Python/Blender
     * Converts raw C-arrays into Mesh structs and calls the engine.
     */
    EXPORT int unwrap_mesh_data(
        const float* coords, int num_verts,
        const int* triangles, int num_tris,
        float* uvs_out,
        float angle_thresh, int min_island_faces, 
        int pack_islands, float island_margin
    ) {
        // Wrap raw data into Mesh struct
        Mesh input_mesh;
        input_mesh.num_vertices = num_verts;
        input_mesh.num_triangles = num_tris;
        input_mesh.vertices = (float*)coords; 
        input_mesh.triangles = (int*)triangles;
        input_mesh.uvs = NULL; 
        
        // Setup Params
        UnwrapParams params;
        params.angle_threshold = angle_thresh;
        params.min_island_faces = min_island_faces;
        params.pack_islands = pack_islands;
        params.island_margin = island_margin;

        // Call the C++ Engine
        UnwrapResult* result_meta = NULL;
        Mesh* result_mesh = unwrap_mesh(&input_mesh, &params, &result_meta);

        if (!result_mesh || !result_meta) {
            return 0; // Failure
        }

        // Copy Output UVs to Python's buffer
        if (result_mesh->uvs) {
            for (int i = 0; i < num_verts * 2; i++) {
                uvs_out[i] = result_mesh->uvs[i];
            }
        }

        // Cleanup
        if (result_mesh->uvs) free(result_mesh->uvs);
        if (result_mesh->vertices) free(result_mesh->vertices);
        if (result_mesh->triangles) free(result_mesh->triangles);
        free(result_mesh);
        
        free_unwrap_result(result_meta);

        return 1; // Success
    }
}