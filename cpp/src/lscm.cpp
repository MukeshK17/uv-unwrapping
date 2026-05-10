/**
 * @file lscm.cpp
 * @brief LSCM (Least Squares Conformal Maps) parameterization
 *
 * Provided by Mixar.
 */

#include "lscm.h"
#include "math_utils.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include <map>
#include <vector>
#include <set>

#include <Eigen/Sparse>
#include <Eigen/SparseLU>

struct Vec3d {
    double x, y, z;
    Vec3d(double x = 0, double y = 0, double z = 0) : x(x), y(y), z(z) {}
    Vec3d operator-(const Vec3d& other) const {
        return Vec3d(x - other.x, y - other.y, z - other.z);
    }
};

static double dot(const Vec3d& a, Vec3d& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vec3d cross(const Vec3d& a, const Vec3d& b) {
    return Vec3d(
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    );
}

static double length(const Vec3d& v) {
    return std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static Vec3d normalize(const Vec3d& v) {
    double len = length(v);
    if (len < 1e-10) return Vec3d(0, 0, 0);
    return Vec3d(v.x / len, v.y / len, v.z / len);
}

int find_boundary_vertices(const Mesh* mesh,
                           const int* face_indices,
                           int num_faces,
                           int** boundary_out) {
    std::set<int> boundary_verts;
    std::map<std::pair<int, int>, int> edge_counts;
    const int* tris = mesh->triangles;

    for (int i = 0; i < num_faces; ++i) {
        int f = face_indices[i];
        int v0 = tris[3 * f + 0];
        int v1 = tris[3 * f + 1];
        int v2 = tris[3 * f + 2];

        int edges[3][2] = {{v0, v1}, {v1, v2}, {v2, v0}};

        for (int e = 0; e < 3; ++e) {
            int a = edges[e][0];
            int b = edges[e][1];
            if (a > b) std::swap(a, b);
            edge_counts[{a, b}]++;
        }
    }

    for (auto const& kv : edge_counts) {
        if (kv.second == 1) {
            boundary_verts.insert(kv.first.first);
            boundary_verts.insert(kv.first.second);
        }
    }

    int num_boundary = (int)boundary_verts.size();
    if (num_boundary > 0) {
        *boundary_out = (int*)malloc(num_boundary * sizeof(int));
        int idx = 0;
        for (int v : boundary_verts) {
            (*boundary_out)[idx++] = v;
        }
    } else {
        *boundary_out = NULL;
    }

    return num_boundary;
}

void normalize_uvs_to_unit_square(float* uvs, int num_verts) {
    if (!uvs || num_verts == 0) return;

    float min_u = FLT_MAX, max_u = -FLT_MAX;
    float min_v = FLT_MAX, max_v = -FLT_MAX;

    for (int i = 0; i < num_verts; i++) {
        float u = uvs[i * 2];
        float v = uvs[i * 2 + 1];

        min_u = min_float(min_u, u);
        max_u = max_float(max_u, u);
        min_v = min_float(min_v, v);
        max_v = max_float(max_v, v);
    }

    float u_range = max_u - min_u;
    float v_range = max_v - min_v;

    if (u_range < 1e-6f) u_range = 1.0f;
    if (v_range < 1e-6f) v_range = 1.0f;

    float aspect = u_range / v_range;
    bool is_extreme_shape = (aspect > 4.0f || aspect < 0.25f);

    if (is_extreme_shape) {
        float max_range = (u_range > v_range) ? u_range : v_range;
        for (int i = 0; i < num_verts; i++) {
            uvs[i * 2]     = (uvs[i * 2] - min_u) / max_range;
            uvs[i * 2 + 1] = (uvs[i * 2 + 1] - min_v) / max_range;
        }
    } else {
        for (int i = 0; i < num_verts; i++) {
            uvs[i * 2]     = (uvs[i * 2] - min_u) / u_range;
            uvs[i * 2 + 1] = (uvs[i * 2 + 1] - min_v) / v_range;
        }
    }
}

float* lscm_parameterize(const Mesh* mesh,
                         const int* face_indices,
                         int num_faces) {
    if (!mesh || !face_indices || num_faces == 0) return NULL;

    printf("LSCM parameterizing %d faces...\n", num_faces);

    std::map<int, int> global_to_local;
    std::vector<int> local_to_global;

    const float* vertices = mesh->vertices;
    const int* tris = mesh->triangles;

    for (int i = 0; i < num_faces; i++) {
        int f = face_indices[i];
        for (int j = 0; j < 3; j++) {
            int global_idx = tris[3 * f + j];

            if (global_to_local.find(global_idx) == global_to_local.end()) {
                global_to_local[global_idx] = (int)local_to_global.size();
                local_to_global.push_back(global_idx);
            }
        }
    }

    int n = (int)local_to_global.size();
    printf("  Island has %d vertices\n", n);

    if (n < 3) {
        fprintf(stderr, "LSCM: Island too small (%d vertices)\n", n);
        return NULL;
    }

    typedef Eigen::Triplet<double> T;
    std::vector<T> triplets;

    for (int i = 0; i < num_faces; i++) {
        int f = face_indices[i];
        int g0 = tris[3 * f + 0]; 
        int g1 = tris[3 * f + 1];
        int g2 = tris[3 * f + 2];

        int v0 = global_to_local[g0]; 
        int v1 = global_to_local[g1];
        int v2 = global_to_local[g2];

        Vec3d p0(vertices[3 * g0 + 0], vertices[3 * g0 + 1], vertices[3 * g0 + 2]); 
        Vec3d p1(vertices[3 * g1 + 0], vertices[3 * g1 + 1], vertices[3 * g1 + 2]);
        Vec3d p2(vertices[3 * g2 + 0], vertices[3 * g2 + 1], vertices[3 * g2 + 2]);

        Vec3d e1 = p1 - p0;  
        Vec3d e2 = p2 - p0;
        Vec3d normal = normalize(cross(e1, e2)); 
        Vec3d u_axis = normalize(e1);
        Vec3d v_axis = cross(normal, u_axis);

        double q0_x = 0.0, q0_y = 0.0;
        double q1_x = dot(e1, u_axis), q1_y = dot(e1, v_axis);
        double q2_x = dot(e2, u_axis), q2_y = dot(e2, v_axis);

        double area = 0.5 * std::abs(q1_x * q2_y - q1_y * q2_x);
        if (area < 1e-10) continue;

        double dx = q1_x - q0_x;
        double dy = q1_y - q0_y;

        triplets.push_back(T(2 * v0, 2 * v1, area * dx));
        triplets.push_back(T(2 * v0, 2 * v1 + 1, area * dy));
        triplets.push_back(T(2 * v0 + 1, 2 * v1, area * dy));
        triplets.push_back(T(2 * v0 + 1, 2 * v1 + 1, area * (-dx)));

        triplets.push_back(T(2 * v0, 2 * v0, -area * dx));
        triplets.push_back(T(2 * v0, 2 * v0 + 1, -area * dy));
        triplets.push_back(T(2 * v0 + 1, 2 * v0, -area * dy));
        triplets.push_back(T(2 * v0 + 1, 2 * v0 + 1, -area * (-dx)));

        dx = q2_x - q1_x;
        dy = q2_y - q1_y; 

        triplets.push_back(T(2 * v1, 2 * v2, area * dx));
        triplets.push_back(T(2 * v1, 2 * v2 + 1, area * dy));
        triplets.push_back(T(2 * v1 + 1, 2 * v2, area * dy));
        triplets.push_back(T(2 * v1 + 1, 2 * v2 + 1, area * (-dx)));

        triplets.push_back(T(2 * v1, 2 * v1, -area * dx));
        triplets.push_back(T(2 * v1, 2 * v1 + 1, -area * dy));
        triplets.push_back(T(2 * v1 + 1, 2 * v1, -area * dy));
        triplets.push_back(T(2 * v1 + 1, 2 * v1 + 1, -area * (-dx)));

        dx = q0_x - q2_x;
        dy = q0_y - q2_y;

        triplets.push_back(T(2 * v2, 2 * v0, area * dx));
        triplets.push_back(T(2 * v2, 2 * v0 + 1, area * dy));
        triplets.push_back(T(2 * v2 + 1, 2 * v0, area * dy));
        triplets.push_back(T(2 * v2 + 1, 2 * v0 + 1, area * (-dx)));

        triplets.push_back(T(2 * v2, 2 * v2, -area * dx));
        triplets.push_back(T(2 * v2, 2 * v2 + 1, -area * dy));
        triplets.push_back(T(2 * v2 + 1, 2 * v2, -area * dy));    
        triplets.push_back(T(2 * v2 + 1, 2 * v2 + 1, -area * (-dx)));
    }

    int pin1 = 0;
    int pin2 = 0;

    int* boundaries = NULL;
    int num_boundary = find_boundary_vertices(mesh, face_indices, num_faces, &boundaries);
    if (num_boundary >= 2) {
        int best_v1 = -1, best_v2 = -1;
        double max_dist_sq = -1.0;

        for (int i = 0; i < num_boundary; i++) {
            int g_i = boundaries[i];
            Vec3d p_i(vertices[3 * g_i + 0], vertices[3 * g_i + 1], vertices[3 * g_i + 2]);

            for (int j = i + 1; j < num_boundary; j++) {
                int g_j = boundaries[j];
                Vec3d p_j(vertices[3 * g_j + 0], vertices[3 * g_j + 1], vertices[3 * g_j + 2]);
                
                Vec3d diff = p_i - p_j;
                double d2 = dot(diff, diff);
                if (d2 > max_dist_sq) {
                    max_dist_sq = d2;
                    best_v1 = g_i;
                    best_v2 = g_j;
                }
            }
        }
        pin1 = global_to_local[best_v1];
        pin2 = global_to_local[best_v2];

    } else {
        pin1 = 0;
        if (pin2 == pin1) pin2 = (pin1 + 1) % n;
    }
    if (boundaries) free(boundaries);

    Eigen::SparseMatrix<double> A(2 * n, 2 * n);
    A.setFromTriplets(triplets.begin(), triplets.end());

    Eigen::VectorXd b = Eigen::VectorXd::Zero(2 * n);

    int pinned_indices[4] = {2 * pin1, 2 * pin1 + 1, 2 * pin2, 2 * pin2 + 1};
    double targets[4] = {0.0, 0.0, 1.0, 0.0};

    for (int i = 0; i < (int)A.outerSize(); ++i) {
        for (Eigen::SparseMatrix<double>::InnerIterator it(A, i); it; ++it) {
            int row = (int)it.row();
            for (int p = 0; p < 4; p++) {
                if (row == pinned_indices[p]) {
                    it.valueRef() = 0.0;
                }
            }
        }
    }

    for (int p = 0; p < 4; ++p) {
        int idx = pinned_indices[p]; 
        A.coeffRef(idx, idx) = 1.0;
        b[idx] = targets[p];
    }

    A.prune(0.0, 1e-12);
    
    Eigen::SparseLU<Eigen::SparseMatrix<double>> solver;
    solver.compute(A);
    if (solver.info() != Eigen::Success) {
        fprintf(stderr, "LSCM: SparseLU decomposition failed\n");
        return NULL;
    }

    Eigen::VectorXd x = solver.solve(b);
    if (solver.info() != Eigen::Success) {
        fprintf(stderr, "LSCM: SparseLU solving failed\n");
        return NULL;
    }

    float* uvs = (float*)malloc(n * 2 * sizeof(float));

    for (int i = 0; i < n; i++) {
        uvs[i * 2] = (float)x[2 * i];
        uvs[i * 2 + 1] = (float)x[2 * i + 1];
    }

    normalize_uvs_to_unit_square(uvs, n);

    printf("  LSCM completed\n");
    return uvs;
}