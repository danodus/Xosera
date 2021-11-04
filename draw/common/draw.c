/*
 * vim: set et ts=4 sw=4
 *------------------------------------------------------------
 *  __ __
 * |  |  |___ ___ ___ ___ ___
 * |-   -| . |_ -| -_|  _| .'|
 * |__|__|___|___|___|_| |__,|
 *
 * Xark's Open Source Enhanced Retro Adapter
 *
 * - "Not as clumsy or random as a GPU, an embedded retro
 *    adapter for a more civilized age."
 *
 * ------------------------------------------------------------
 * Portions Copyright (c) 2021 Daniel Cliche
 * Portions Copyright (c) 2021 Xark
 * MIT License
 *
 * Draw
 * ------------------------------------------------------------
 */

#include "draw.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void xd_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color);
void xd_draw_filled_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color);

vec3d matrix_multiply_vector(mat4x4 * m, vec3d * i)
{
    vec3d r = {MUL(i->x, m->m[0][0]) + MUL(i->y, m->m[1][0]) + MUL(i->z, m->m[2][0]) + m->m[3][0],
               MUL(i->x, m->m[0][1]) + MUL(i->y, m->m[1][1]) + MUL(i->z, m->m[2][1]) + m->m[3][1],
               MUL(i->x, m->m[0][2]) + MUL(i->y, m->m[1][2]) + MUL(i->z, m->m[2][2]) + m->m[3][2],
               MUL(i->x, m->m[0][3]) + MUL(i->y, m->m[1][3]) + MUL(i->z, m->m[2][3]) + m->m[3][3]};

    return r;
}

vec3d vector_add(vec3d * v1, vec3d * v2)
{
    vec3d r = {v1->x + v2->x, v1->y + v2->y, v1->z + v2->z, FX(1.0f)};
    return r;
}

vec3d vector_sub(vec3d * v1, vec3d * v2)
{
    vec3d r = {v1->x - v2->x, v1->y - v2->y, v1->z - v2->z, FX(1.0f)};
    return r;
}

vec3d vector_mul(vec3d * v1, fx32 k)
{
    vec3d r = {MUL(v1->x, k), MUL(v1->y, k), MUL(v1->z, k), FX(1.0f)};
    return r;
}

vec3d vector_div(vec3d * v1, fx32 k)
{
    vec3d r = {DIV(v1->x, k), DIV(v1->y, k), DIV(v1->z, k), FX(1.0f)};
    return r;
}

fx32 vector_dot_product(vec3d * v1, vec3d * v2)
{
    return MUL(v1->x, v2->x) + MUL(v1->y, v2->y) + MUL(v1->z, v2->z);
}

fx32 vector_length(vec3d * v)
{
    return SQRT(vector_dot_product(v, v));
}

vec3d vector_normalize(vec3d * v)
{
    fx32 l = vector_length(v);
    if (l > FX(0.0f))
    {
        vec3d r = {DIV(v->x, l), DIV(v->y, l), DIV(v->z, l), FX(1.0f)};
        return r;
    }
    else
    {
        vec3d r = {FX(0.0f), FX(0.0f), FX(0.0f), FX(1.0f)};
        return r;
    }
}

vec3d vector_cross_product(vec3d * v1, vec3d * v2)
{
    vec3d r = {MUL(v1->y, v2->z) - MUL(v1->z, v2->y),
               MUL(v1->z, v2->x) - MUL(v1->x, v2->z),
               MUL(v1->x, v2->y) - MUL(v1->y, v2->x),
               FX(1.0f)};
    return r;
}

mat4x4 matrix_make_identity()
{
    mat4x4 mat;
    memset(&mat, 0, sizeof(mat4x4));
    mat.m[0][0] = 1.0f;
    mat.m[1][1] = 1.0f;
    mat.m[2][2] = 1.0f;
    mat.m[3][3] = 1.0f;
    return mat;
}

mat4x4 matrix_make_projection(int viewport_width, int viewport_height, float fov)
{
    mat4x4 mat_proj;

    // projection matrix
    fx32 near         = FX(0.1f);
    fx32 far          = FX(1000.0f);
    fx32 aspect_ratio = FX((float)viewport_height / (float)viewport_width);
    fx32 fov_rad      = FX(1.0f / tanf(fov * 0.5f / 180.0f * 3.14159f));

    memset(&mat_proj, 0, sizeof(mat4x4));
    mat_proj.m[0][0] = MUL(aspect_ratio, fov_rad);
    mat_proj.m[1][1] = fov_rad;
    mat_proj.m[2][2] = DIV(far, (far - near));
    mat_proj.m[3][2] = DIV(MUL(-far, near), (far - near));
    mat_proj.m[2][3] = FX(1.0f);
    mat_proj.m[3][3] = FX(0.0f);

    return mat_proj;
}

mat4x4 matrix_make_rotation_x(float theta)
{
    mat4x4 mat_rot_x;
    memset(&mat_rot_x, 0, sizeof(mat4x4));

    // rotation X
    mat_rot_x.m[0][0] = FX(1.0f);
    mat_rot_x.m[1][1] = FX(cosf(theta));
    mat_rot_x.m[1][2] = FX(sinf(theta));
    mat_rot_x.m[2][1] = FX(-sinf(theta));
    mat_rot_x.m[2][2] = FX(cosf(theta));
    mat_rot_x.m[3][3] = FX(1.0f);

    return mat_rot_x;
}

mat4x4 matrix_make_rotation_y(float theta)
{
    mat4x4 mat_rot_y;
    memset(&mat_rot_y, 0, sizeof(mat4x4));

    // rotation Y
    mat_rot_y.m[0][0] = FX(cos(theta));
    mat_rot_y.m[0][2] = FX(-sinf(theta));
    mat_rot_y.m[1][1] = FX(1.0f);
    mat_rot_y.m[2][0] = FX(sinf(theta));
    mat_rot_y.m[2][2] = FX(cosf(theta));

    return mat_rot_y;
}

mat4x4 matrix_make_rotation_z(float theta)
{
    mat4x4 mat_rot_z;
    memset(&mat_rot_z, 0, sizeof(mat4x4));

    // rotation Z
    mat_rot_z.m[0][0] = FX(cosf(theta));
    mat_rot_z.m[0][1] = FX(sinf(theta));
    mat_rot_z.m[1][0] = FX(-sinf(theta));
    mat_rot_z.m[1][1] = FX(cosf(theta));
    mat_rot_z.m[2][2] = FX(1.0f);
    mat_rot_z.m[3][3] = FX(1.0f);

    return mat_rot_z;
}

mat4x4 matrix_make_translation(fx32 x, fx32 y, fx32 z)
{
    mat4x4 mat;
    memset(&mat, 0, sizeof(mat4x4));

    mat.m[0][0] = FX(1.0f);
    mat.m[1][1] = FX(1.0f);
    mat.m[2][2] = FX(1.0f);
    mat.m[3][3] = FX(1.0f);
    mat.m[3][0] = x;
    mat.m[3][1] = y;
    mat.m[3][2] = z;

    return mat;
}

mat4x4 matrix_multiply_matrix(mat4x4 * m1, mat4x4 * m2)
{
    mat4x4 mat;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r)
            mat.m[c][r] = MUL(m1->m[c][0], m2->m[0][r]) + MUL(m1->m[c][1], m2->m[1][r]) +
                          MUL(m1->m[c][2], m2->m[2][r]) + MUL(m1->m[c][3], m2->m[3][r]);
    return mat;
}

void swap_triangle(triangle_t * a, triangle_t * b)
{
    triangle_t c = *a;
    *a           = *b;
    *b           = c;
}

fx32 triangle_depth(triangle_t * tri)
{
    return DIV(tri->p[0].z + tri->p[1].z + tri->p[2].z, FX(3.0f));
}

size_t sort_triangles_partition(triangle_t triangles[], size_t nb_triangles, size_t l, size_t h)
{
    size_t i, j;
    fx32   pivot;
    pivot = triangle_depth(&triangles[l]);
    i     = l;
    j     = h;

    while (i < j)
    {

        do
        {
            i++;
        } while (i < nb_triangles && triangle_depth(&triangles[i]) <= pivot);
        do
        {
            j--;
        } while (triangle_depth(&triangles[j]) > pivot);
        if (i < j)
        {
            swap_triangle(&triangles[i], &triangles[j]);
        }
    }
    swap_triangle(&triangles[l], &triangles[j]);
    return j;
}

void sort_triangles_lh(triangle_t triangles[], size_t nb_triangles, size_t l, size_t h)
{
    size_t j;

    if (l < h)
    {
        j = sort_triangles_partition(triangles, nb_triangles, l, h);
        sort_triangles_lh(triangles, nb_triangles, l, j);
        sort_triangles_lh(triangles, nb_triangles, j + 1, h);
    }
}

void sort_triangles(triangle_t triangles[], size_t nb_triangles)
{
    sort_triangles_lh(triangles, nb_triangles, 0, nb_triangles);
}

void draw_model(int       viewport_width,
                int       viewport_height,
                model_t * model,
                mat4x4 *  mat_proj,
                mat4x4 *  mat_rot_z,
                mat4x4 *  mat_rot_x,
                bool      is_lighting_ena,
                bool      is_wireframe)
{
    mat4x4 mat_trans = matrix_make_translation(FX(0.0f), FX(0.0f), FX(3.0f));
    mat4x4 mat_world;
    mat_world = matrix_make_identity();
    mat_world = matrix_multiply_matrix(mat_rot_z, mat_rot_x);
    mat_world = matrix_multiply_matrix(&mat_world, &mat_trans);


    vec3d vec_camera = {FX(0.0f), FX(0.0f), FX(0.0f)};

    size_t triangle_to_raster_index = 0;

    // draw faces
    for (size_t i = 0; i < model->mesh.nb_faces; ++i)
    {
        face_t *   face = &model->mesh.faces[i];
        triangle_t tri;
        tri.p[0] = model->mesh.vertices[face->indices[0]];
        tri.p[1] = model->mesh.vertices[face->indices[1]];
        tri.p[2] = model->mesh.vertices[face->indices[2]];
        tri.col  = face->col;

        triangle_t tri_projected, tri_transformed;

        tri_transformed.p[0] = matrix_multiply_vector(&mat_world, &tri.p[0]);
        tri_transformed.p[1] = matrix_multiply_vector(&mat_world, &tri.p[1]);
        tri_transformed.p[2] = matrix_multiply_vector(&mat_world, &tri.p[2]);

        // calculate the normal
        vec3d normal, line1, line2;
        line1.x = tri_transformed.p[1].x - tri_transformed.p[0].x;
        line1.y = tri_transformed.p[1].y - tri_transformed.p[0].y;
        line1.z = tri_transformed.p[1].z - tri_transformed.p[0].z;

        line2.x = tri_transformed.p[2].x - tri_transformed.p[0].x;
        line2.y = tri_transformed.p[2].y - tri_transformed.p[0].y;
        line2.z = tri_transformed.p[2].z - tri_transformed.p[0].z;

        // take the cross product of lines to get normal to triangle surface
        normal = vector_cross_product(&line1, &line2);

        if (is_lighting_ena)
        {
            normal = vector_normalize(&normal);
        }

        // get ray from triangle to camera
        vec3d vec_camera_ray = vector_sub(&tri_transformed.p[0], &vec_camera);

        // if ray is aligned with normal, then triangle is visible
        if (vector_dot_product(&normal, &vec_camera_ray) < FX(0.0f))
        {
            fx32 dp = FX(1.0f);
            if (is_lighting_ena)
            {
                // illumination
                vec3d light_direction = {FX(0.0f), FX(0.0f), FX(-1.0f)};
                light_direction       = vector_normalize(&light_direction);

                // how "aligned" are light direction and triangle surface normal?
                dp = vector_dot_product(&light_direction, &normal);

                if (dp < FX(0.0f))
                    dp = FX(0.0f);
            }
            tri_transformed.col.x = dp;
            tri_transformed.col.y = dp;
            tri_transformed.col.z = dp;

            // project triangles from 3D to 2D
            tri_projected.p[0] = matrix_multiply_vector(mat_proj, &tri_transformed.p[0]);
            tri_projected.p[1] = matrix_multiply_vector(mat_proj, &tri_transformed.p[1]);
            tri_projected.p[2] = matrix_multiply_vector(mat_proj, &tri_transformed.p[2]);
            tri_projected.col  = tri_transformed.col;

            // scale into view
            tri_projected.p[0] = vector_div(&tri_projected.p[0], tri_projected.p[0].w);
            tri_projected.p[1] = vector_div(&tri_projected.p[1], tri_projected.p[1].w);
            tri_projected.p[2] = vector_div(&tri_projected.p[2], tri_projected.p[2].w);

            // offset vertices into visible normalized space
            vec3d vec_offset_view = {FX(1.0f), FX(1.0f), FX(0.0f)};
            tri_projected.p[0]    = vector_add(&tri_projected.p[0], &vec_offset_view);
            tri_projected.p[1]    = vector_add(&tri_projected.p[1], &vec_offset_view);
            tri_projected.p[2]    = vector_add(&tri_projected.p[2], &vec_offset_view);

            fx32 w               = FX(0.5f * (float)viewport_width);
            fx32 h               = FX(0.5f * (float)viewport_height);
            tri_projected.p[0].x = MUL(tri_projected.p[0].x, w);
            tri_projected.p[0].y = MUL(tri_projected.p[0].y, h);
            tri_projected.p[1].x = MUL(tri_projected.p[1].x, w);
            tri_projected.p[1].y = MUL(tri_projected.p[1].y, h);
            tri_projected.p[2].x = MUL(tri_projected.p[2].x, w);
            tri_projected.p[2].y = MUL(tri_projected.p[2].y, h);

            // store triangle for sorting
            model->triangles_to_raster[triangle_to_raster_index] = tri_projected;
            triangle_to_raster_index++;
        }
    }

    // sort triangles from front to back
    sort_triangles(model->triangles_to_raster, triangle_to_raster_index);

    for (size_t i = 0; i < triangle_to_raster_index; ++i)
    {
        triangle_t tri_projected = model->triangles_to_raster[triangle_to_raster_index - i - 1];
        // rasterize triangle
        fx32 col = MUL(tri_projected.col.x, FX(255.0f));

        xd_draw_filled_triangle(INT(tri_projected.p[0].x),
                                INT(tri_projected.p[0].y),
                                INT(tri_projected.p[1].x),
                                INT(tri_projected.p[1].y),
                                INT(tri_projected.p[2].x),
                                INT(tri_projected.p[2].y),
                                INT(col));

        if (is_wireframe)
        {
            xd_draw_triangle(INT(tri_projected.p[0].x),
                             INT(tri_projected.p[0].y),
                             INT(tri_projected.p[1].x),
                             INT(tri_projected.p[1].y),
                             INT(tri_projected.p[2].x),
                             INT(tri_projected.p[2].y),
                             0);
        }
    }
}