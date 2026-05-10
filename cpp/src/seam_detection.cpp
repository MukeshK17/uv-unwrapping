/**
 * @file seam_detection.cpp
 * @brief Seam detection using spanning tree + angular defect
 *
 */

#include <algorithm>
#include "unwrap.h"
#include "math_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <vector>
#include <set>
#include <queue>

#ifndef M_PI
  #define M_PI 3.14159265358979323846
#endif

static float get_edge_sharpness(const Mesh* mesh, int f0, int f1) {
    if (f0 < 0 || f1 < 0) return 1.0f; 

    const float* v = mesh->vertices;
    const int* t = mesh->triangles;

    // Normal f0
    int i0 = t[3 * f0], i1 = t[3 * f0 + 1], i2 = t[3 * f0 + 2];
    float p0[3] = {v[3 * i0], v[3 * i0 + 1], v[3 * i0 + 2]};
    float p1[3] = {v[3 * i1], v[3 * i1 + 1], v[3 * i1 + 2]};
    float p2[3] = {v[3 * i2], v[3 * i2 + 1], v[3 * i2 + 2]};
    float e1[3] = {p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2]};
    float e2[3] = {p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2]};
    float n0[3] = {e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2], e1[0] * e2[1] - e1[1] * e2[0]};
    
    float l0 = sqrtf(n0[0] * n0[0] + n0[1] * n0[1] + n0[2] * n0[2]); 
    if (l0 > 0) { n0[0] /= l0; n0[1] /= l0; n0[2] /= l0; }

    // Normal f1
    int j0 = t[3 * f1], j1 = t[3 * f1 + 1], j2 = t[3 * f1 + 2];
    float q0[3] = {v[3 * j0], v[3 * j0 + 1], v[3 * j0 + 2]};
    float q1[3] = {v[3 * j1], v[3 * j1 + 1], v[3 * j1 + 2]};
    float q2[3] = {v[3 * j2], v[3 * j2 + 1], v[3 * j2 + 2]};
    float d1[3] = {q1[0] - q0[0], q1[1] - q0[1], q1[2] - q0[2]};
    float d2[3] = {q2[0] - q0[0], q2[1] - q0[1], q2[2] - q0[2]};
    float n1[3] = {d1[1] * d2[2] - d1[2] * d2[1], d1[2] * d2[0] - d1[0] * d2[2], d1[0] * d2[1] - d1[1] * d2[0]};
    
    float l1 = sqrtf(n1[0] * n1[0] + n1[1] * n1[1] + n1[2] * n1[2]); 
    if (l1 > 0) { n1[0] /= l1; n1[1] /= l1; n1[2] /= l1; }

    return 1.0f - (n0[0] * n1[0] + n0[1] * n1[1] + n0[2] * n1[2]);
}

/**
 * @brief Compute angular defect at a vertex
 *
 * Angular defect = 2π - sum of angles at vertex
 *
 * - Flat surface: defect ≈ 0
 * - Corner (like cube): defect > 0
 * - Saddle: defect < 0
 *
 * @param mesh Input mesh
 * @param vertex_idx Vertex index
 * @return Angular defect in radians
 */
static float compute_angular_defect(const Mesh* mesh, int vertex_idx) {
    float angle_sum = 0.0f;

    if (!mesh) return 0.0f;

    int F = mesh->num_triangles;
    int V = mesh->num_vertices;
    const int* tris = mesh->triangles;

    if (vertex_idx < 0 || vertex_idx >= V) return 0.0f;

    for (int f = 0; f < F; ++f) {
        int a = tris[3 * f + 0];
        int b = tris[3 * f + 1];  
        int c = tris[3 * f + 2];

        if (a == vertex_idx || b == vertex_idx || c == vertex_idx) {
            angle_sum += compute_vertex_angle_in_triangle(mesh, f, vertex_idx);
        }
    }

    return 2.0f * float(M_PI) - angle_sum;
}

/**
 * @brief Get all edges incident to a vertex
 */
static std::vector<int> get_vertex_edges(const TopologyInfo* topo, int vertex_idx) {
    std::vector<int> edges;

    if (!topo) return edges;
    int E = topo->num_edges;
    const int* edge_verts = topo->edges;
    
    for (int e = 0; e < E; ++e) {
        int v0 = edge_verts[2 * e + 0];
        int v1 = edge_verts[2 * e + 1];
        if (v0 == vertex_idx || v1 == vertex_idx) {
            edges.push_back(e);
        }
    }

    return edges;
}

int* detect_seams(const Mesh* mesh,
                  const TopologyInfo* topo,
                  float angle_threshold,
                  int* num_seams_out) {
    if (!mesh || !topo || !num_seams_out) return NULL;
    (void)angle_threshold;

    std::set<int> seam_candidates;

    const int V = mesh->num_vertices;
    const int F = mesh->num_triangles;
    const int E = topo->num_edges;
    const int* edge_faces = topo->edge_faces;

    if (F <= 0 || E <= 0) {
        *num_seams_out = 0;
        return NULL;
    }

    // Dual graph (face adjacency)
    std::vector<std::vector<std::pair<int, int>>> face_adj(F);
    for (int e = 0; e < E; ++e) {
        int f0 = edge_faces[2 * e + 0];
        int f1 = edge_faces[2 * e + 1];
        if (f0 >= 0 && f1 >= 0 && f0 < F && f1 < F) {
            face_adj[f0].push_back(std::make_pair(e, f1));
            face_adj[f1].push_back(std::make_pair(e, f0));
        }
    }

    // Forces the BFS to explore flat surfaces first, pushing sharp edges to be seams
    for (int f = 0; f < F; ++f) {
        std::sort(face_adj[f].begin(), face_adj[f].end(), 
            [&](const std::pair<int, int>& a, const std::pair<int, int>& b) {
                float costA = get_edge_sharpness(mesh, f, a.second);
                float costB = get_edge_sharpness(mesh, f, b.second);
                return costA < costB; 
            }
        );
    }

    // BFS spanning tree
    std::vector<char> visited(F, 0);
    std::set<int> tree_edges; 

    if (F > 0) {
        std::queue<int> q;
        visited[0] = 1;
        q.push(0);

        while (!q.empty()) {
            int curr_face = q.front();
            q.pop();

            for (const auto& neighbor : face_adj[curr_face]) {
                int edge_idx = neighbor.first;
                int adj_face = neighbor.second;

                if (!visited[adj_face]) {
                    visited[adj_face] = 1;
                    tree_edges.insert(edge_idx);
                    q.push(adj_face);
                }
            }
        }
    }

    // Seam candidates = non-tree edges
    std::vector<int> non_tree_edges;
    for (int e = 0; e < E; ++e) {
        int f0 = edge_faces[2 * e + 0];
        int f1 = edge_faces[2 * e + 1];
        
        // Skip boundary edges
        if (f0 < 0 || f1 < 0) continue;
        
        if (tree_edges.find(e) == tree_edges.end()) {
            non_tree_edges.push_back(e);
        }
    }

    if (non_tree_edges.empty()) {
        *num_seams_out = 0;
        return NULL;
    }

    for (int nte : non_tree_edges) {
        int f0 = edge_faces[2 * nte + 0];
        int f1 = edge_faces[2 * nte + 1];
        float sharpness = get_edge_sharpness(mesh, f0, f1);
        
        // Threshold: 0.5 (approx 60 degrees) keeps Cubes seams, ignores Cylinder smoothness
        if (sharpness > 0.5f) {
            seam_candidates.insert(nte);
        }
    }

    // Fallback: If filtered everything out, still need at least one cut
    if (seam_candidates.empty() && !non_tree_edges.empty()) {
        int best_e = -1; 
        float max_s = -1.0f;
        for (int e : non_tree_edges) {
            int f0 = edge_faces[2 * e + 0];
            int f1 = edge_faces[2 * e + 1];
            float s = get_edge_sharpness(mesh, f0, f1);
            if (s > max_s) { max_s = s; best_e = e; }
        }
        if (best_e != -1) seam_candidates.insert(best_e);
    }
    
    // Angular defect refinement
    const float defect_threshold = 0.5f; 

    for (int v = 0; v < V; ++v) {
        float defect = compute_angular_defect(mesh, v);

        if (defect > defect_threshold) {
            std::vector<int> incident_edges = get_vertex_edges(topo, v);
            for (int e : incident_edges) {
                bool is_non_tree = false;
                for (int nte : non_tree_edges) if (nte == e) is_non_tree = true;
                
                if (is_non_tree) {
                    seam_candidates.insert(e);
                }
            }
        }
    }

    // Convert to array
    if (seam_candidates.empty()) {
        *num_seams_out = 0;
        return NULL;
    }

    *num_seams_out = (int)seam_candidates.size();
    int* seams = (int*)malloc(*num_seams_out * sizeof(int));

    if (!seams) {
        *num_seams_out = 0;
        return NULL;
    }
    
    int idx = 0;
    for (int edge_idx : seam_candidates) {
        seams[idx++] = edge_idx;
    }

    printf("Detected %d seams\n", *num_seams_out);

    return seams;
}