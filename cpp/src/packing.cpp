/**
 * @file packing.cpp
 * @brief UV island packing into [0,1]² texture space
 *
 */

#include "unwrap.h"
#include "math_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <vector>
#include <algorithm>

/**
 * @brief Island bounding box info
 */
struct Island {
    int id;
    float min_u, max_u, min_v, max_v;
    float width, height;
    float target_x, target_y;  // Packed position
    std::vector<int> vertex_indices;
};

void pack_uv_islands(Mesh* mesh,
                     const UnwrapResult* result,
                     float margin) {
    if (!mesh || !result || !mesh->uvs) return;

    if (result->num_islands <= 1) {
        return;
    }

    printf("Packing %d islands...\n", result->num_islands);

    std::vector<Island> islands(result->num_islands);

    for(int i = 0; i < result->num_islands; ++i) {
        islands[i].id = i;
        islands[i].min_u = FLT_MAX;
        islands[i].max_u = -FLT_MAX;
        islands[i].min_v = FLT_MAX;
        islands[i].max_v = -FLT_MAX;
        islands[i].target_x = 0;
        islands[i].target_y = 0;
    }

    const int* tris = mesh->triangles;
    const int* face_ids = result->face_island_ids;

    for (int f = 0; f < mesh->num_triangles; f++) {
        int island_id = face_ids[f];
        if (island_id < 0 || island_id >= result->num_islands) continue;

        for (int j = 0; j < 3; j++) {
            int v_idx = tris[3*f + j];
            float u = mesh->uvs[2*v_idx];
            float v = mesh->uvs[2*v_idx + 1];

            islands[island_id].min_u = min_float(islands[island_id].min_u, u);
            islands[island_id].max_u = max_float(islands[island_id].max_u, u);
            islands[island_id].min_v = min_float(islands[island_id].min_v, v);
            islands[island_id].max_v = max_float(islands[island_id].max_v, v);
        }
    }

    for(int i = 0; i < result->num_islands; ++i) {
        if (islands[i].min_u == FLT_MAX) {
            islands[i].width = 0;
            islands[i].height = 0;
        } else {
            islands[i].width = islands[i].max_u - islands[i].min_u;
            islands[i].height = islands[i].max_v - islands[i].min_v;
        }
    }

    float total_area = 0.0f;
    for(int i = 0; i < result->num_islands; ++i) {
        if (islands[i].min_u != FLT_MAX) {
            total_area += (islands[i].width + margin) * (islands[i].height + margin);
        }
    }

    std::vector<int> sorted_indices(result->num_islands);
    for(int i = 0; i < result->num_islands; ++i) sorted_indices[i] = i;

    std::sort(sorted_indices.begin(), sorted_indices.end(), 
        [&islands](int a, int b) {
            return islands[a].height > islands[b].height;
        }
    );

    float current_x = 0.0f;
    float current_y = 0.0f;
    float shelf_height = 0.0f;
    float max_packed_w = 0.0f;
    float max_packed_h = 0.0f;

    const float BIN_WIDTH = (total_area > 0.0f) ? sqrtf(total_area) : 1.0f;

    if (!sorted_indices.empty()) {
        shelf_height = islands[sorted_indices[0]].height;
    }

    for (int idx : sorted_indices) {
        Island& isl = islands[idx];
        if (isl.width == 0) continue;

        if (current_x + isl.width > BIN_WIDTH) {
            current_x = 0.0f;
            current_y += shelf_height + margin;
            shelf_height = isl.height; 
        }

        isl.target_x = current_x;
        isl.target_y = current_y;

        current_x += isl.width + margin;
        
        max_packed_w = max_float(max_packed_w, current_x);
        max_packed_h = max_float(max_packed_h, current_y + isl.height);
        
        if (isl.height > shelf_height) shelf_height = isl.height;
    }

    struct Offset { float x, y; };
    std::vector<Offset> vert_offsets(mesh->num_vertices, {0.0f, 0.0f});
    std::vector<bool> vert_seen(mesh->num_vertices, false);

    for (int f = 0; f < mesh->num_triangles; f++) {
        int isl_id = face_ids[f];
        if (isl_id < 0) continue;
        
        Island& isl = islands[isl_id];
        float off_x = isl.target_x - isl.min_u;
        float off_y = isl.target_y - isl.min_v;

        for(int j = 0; j < 3; j++) {
            int v = tris[3*f+j];
            if (!vert_seen[v]) {
                vert_offsets[v] = {off_x, off_y};
                vert_seen[v] = true;
            }
        }
    }

    for(int v = 0; v < mesh->num_vertices; v++) {
        if(vert_seen[v]) {
            mesh->uvs[2*v]     += vert_offsets[v].x;
            mesh->uvs[2*v+1] += vert_offsets[v].y;
        }
    }

    float scale = 1.0f;
    float final_w = max_packed_w;
    float final_h = max_packed_h;
    
    float max_dim = max_float(final_w, final_h);
    if (max_dim > 1e-6) {
        scale = 1.0f / max_dim;
    }

    for (int v = 0; v < mesh->num_vertices; v++) {
        mesh->uvs[2*v]     *= scale;
        mesh->uvs[2*v + 1] *= scale;
    }
    printf("  Packing completed\n");
}

void compute_quality_metrics(const Mesh* mesh, UnwrapResult* result) {
    if (!mesh || !result || !mesh->uvs) return;

    result->avg_stretch = 1.0f;
    result->max_stretch = 1.0f;

    double total_uv_area = 0.0;
    const int* tris = mesh->triangles;
    const float* uvs = mesh->uvs;

    for (int f = 0; f < mesh->num_triangles; f++) {
        int idx0 = tris[3*f + 0];
        int idx1 = tris[3*f + 1];
        int idx2 = tris[3*f + 2];

        float u0 = uvs[2*idx0], v0 = uvs[2*idx0+1];
        float u1 = uvs[2*idx1], v1 = uvs[2*idx1+1];
        float u2 = uvs[2*idx2], v2 = uvs[2*idx2+1];

        double area = 0.5 * std::abs((u1-u0)*(v2-v0) - (v1-v0)*(u2-u0));
        total_uv_area += area;
    }

    result->coverage = (float)total_uv_area;
    if (result->coverage > 1.0f) result->coverage = 1.0f; 

    printf("Quality metrics:\n");
    printf("  Avg stretch: %.2f (default)\n", result->avg_stretch);
    printf("  Max stretch: %.2f (default)\n", result->max_stretch);
    printf("  Coverage: %.1f%%\n", result->coverage * 100.0f);
}