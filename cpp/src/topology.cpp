/**
 * @file topology.cpp
 * @brief Topology builder implementation
 *
 */

#include "topology.h"
#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <vector>
#include "mesh.h"

/**
 * @brief Edge structure for uniqueness
 */
struct Edge {
    int v0, v1;

    Edge(int a, int b) {
        // Always store smaller vertex first
        if (a < b) {
            v0 = a;
            v1 = b;
        } else {
            v0 = b;
            v1 = a;
        }
    }

    bool operator<(const Edge& other) const {
        if (v0 != other.v0) return v0 < other.v0;
        return v1 < other.v1;
    }
};

/**
 * @brief Edge information
 */
struct EdgeInfo {
    int face0;
    int face1;

    EdgeInfo() : face0(-1), face1(-1) {}
};

TopologyInfo* build_topology(const Mesh* mesh) {
    if (!mesh) return NULL;

    TopologyInfo* topo = (TopologyInfo*)malloc(sizeof(TopologyInfo));
    if (!topo) return NULL;

    // Initialize to safe defaults
    topo->edges = NULL;
    topo->num_edges = 0;
    topo->edge_faces = NULL;

    int V = mesh->num_vertices;
    int F = mesh->num_triangles;
    const int* tris = mesh->triangles;

    if (!tris || V <= 0 || F <= 0) {
        return topo;
    }

    std::map<Edge, EdgeInfo> edge_map;

    for (int f = 0; f < F; ++f) {
        int idx0 = tris[3 * f + 0];
        int idx1 = tris[3 * f + 1];
        int idx2 = tris[3 * f + 2];

        if (idx0 < 0 || idx0 >= V ||
            idx1 < 0 || idx1 >= V ||
            idx2 < 0 || idx2 >= V) {
            printf("Error: Triangle %d has invalid vertex indices\n", f); 
            continue;
        }
        
        struct { int a, b; } tri_edges[3] = {
            {idx0, idx1},
            {idx1, idx2},
            {idx2, idx0}
        };

        for (int e = 0; e < 3; ++e) {
            int a = tri_edges[e].a;
            int b = tri_edges[e].b;
            
            if (a == b) {
                continue; // Degenerate edge, skip
            }
            
            Edge key(a, b);
            EdgeInfo& info = edge_map[key];
            
            if (info.face0 == -1) {
                info.face0 = f;
            } else if (info.face1 == -1) {
                info.face1 = f;
            } else {
                printf("Warning: non-manifold edge (%d, %d) seen at face %d (already has faces %d and %d)\n",
                       key.v0, key.v1, f, info.face0, info.face1);
            }
        }
    }

    // Convert map to arrays
    size_t E = edge_map.size();
    if (E == 0) {
        topo->num_edges = 0;
        return topo; // no valid edges found
    }

    int* edges = (int*)malloc(sizeof(int) * 2 * E);
    int* edge_faces = (int*)malloc(sizeof(int) * 2 * E);
    
    if (!edges || !edge_faces) {
        printf("Error: malloc failed in build_topology\n");
        if (edges) free(edges);
        if (edge_faces) free(edge_faces);
        free(topo);
        return NULL;
    }

    // Fill arrays
    size_t i = 0;
    for (const auto& kv : edge_map) {
        const Edge& key = kv.first;
        const EdgeInfo& info = kv.second;

        edges[2 * i + 0] = key.v0;
        edges[2 * i + 1] = key.v1;

        edge_faces[2 * i + 0] = info.face0;
        edge_faces[2 * i + 1] = info.face1; // -1 if boundary

        ++i;
    }

    topo->edges = edges;
    topo->edge_faces = edge_faces;
    topo->num_edges = (int)E;

    return topo;
}

void free_topology(TopologyInfo* topo) {
    if (!topo) return;

    if (topo->edges) free(topo->edges);
    if (topo->edge_faces) free(topo->edge_faces);
    free(topo);
}

int validate_topology(const Mesh* mesh, const TopologyInfo* topo) {
    if (!mesh || !topo) return 0;

    int V = mesh->num_vertices;
    int E = topo->num_edges;
    int F = mesh->num_triangles;

    int euler = V - E + F;

    printf("Topology validation:\n");
    printf("  V=%d, E=%d, F=%d\n", V, E, F);
    printf("  Euler characteristic: %d (expected 2 for closed mesh)\n", euler);

    // Closed meshes should have Euler = 2
    // Open meshes or meshes with holes may differ
    if (euler != 2) {
        printf("  Warning: Non-standard Euler characteristic\n");
        printf("  (This may be OK for open meshes or meshes with boundaries)\n");
    }

    return 1;
}