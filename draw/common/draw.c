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

void multiply_matrix_vector(vec3d * i, vec3d * o, mat4x4 * m)
{
    o->x   = MUL(i->x, m->m[0][0]) + MUL(i->y, m->m[1][0]) + MUL(i->z, m->m[2][0]) + m->m[3][0];
    o->y   = MUL(i->x, m->m[0][1]) + MUL(i->y, m->m[1][1]) + MUL(i->z, m->m[2][1]) + m->m[3][1];
    o->z   = MUL(i->x, m->m[0][2]) + MUL(i->y, m->m[1][2]) + MUL(i->z, m->m[2][2]) + m->m[3][2];
    fx32 w = MUL(i->x, m->m[0][3]) + MUL(i->y, m->m[1][3]) + MUL(i->z, m->m[2][3]) + m->m[3][3];

    if (w != 0)
    {
        o->x = DIV(o->x, w);
        o->y = DIV(o->y, w);
        o->z = DIV(o->z, w);
    }
}

void get_projection_matrix(int viewport_width, int viewport_height, float fov, mat4x4 * mat_proj)
{
    // projection matrix
    fx32 near         = FX(0.1f);
    fx32 far          = FX(1000.0f);
    fx32 aspect_ratio = FX((float)viewport_height / (float)viewport_width);
    fx32 fov_rad      = FX(1.0f / tanf(fov * 0.5f / 180.0f * 3.14159f));

    memset(mat_proj, 0, sizeof(mat4x4));
    mat_proj->m[0][0] = MUL(aspect_ratio, fov_rad);
    mat_proj->m[1][1] = fov_rad;
    mat_proj->m[2][2] = DIV(far, (far - near));
    mat_proj->m[3][2] = DIV(MUL(-far, near), (far - near));
    mat_proj->m[2][3] = FX(1.0f);
    mat_proj->m[3][3] = FX(0.0f);
}

void get_rotation_z_matrix(float theta, mat4x4 * mat_rot_z)
{
    memset(mat_rot_z, 0, sizeof(mat4x4));

    // rotation Z
    mat_rot_z->m[0][0] = FX(cosf(theta));
    mat_rot_z->m[0][1] = FX(sinf(theta));
    mat_rot_z->m[1][0] = FX(-sinf(theta));
    mat_rot_z->m[1][1] = FX(cosf(theta));
    mat_rot_z->m[2][2] = FX(1.0f);
    mat_rot_z->m[3][3] = FX(1.0f);
}

void get_rotation_x_matrix(float theta, mat4x4 * mat_rot_x)
{
    memset(mat_rot_x, 0, sizeof(mat4x4));

    // rotation X
    mat_rot_x->m[0][0] = FX(1.0f);
    mat_rot_x->m[1][1] = FX(cosf(theta));
    mat_rot_x->m[1][2] = FX(sinf(theta));
    mat_rot_x->m[2][1] = FX(-sinf(theta));
    mat_rot_x->m[2][2] = FX(cosf(theta));
    mat_rot_x->m[3][3] = FX(1.0f);
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

        triangle_t tri_projected, tri_translated, tri_rotated_z, tri_rotated_zx;

        // rotate in Z-axis
        multiply_matrix_vector(&tri.p[0], &tri_rotated_z.p[0], mat_rot_z);
        multiply_matrix_vector(&tri.p[1], &tri_rotated_z.p[1], mat_rot_z);
        multiply_matrix_vector(&tri.p[2], &tri_rotated_z.p[2], mat_rot_z);

        // rotate in X-axis
        multiply_matrix_vector(&tri_rotated_z.p[0], &tri_rotated_zx.p[0], mat_rot_x);
        multiply_matrix_vector(&tri_rotated_z.p[1], &tri_rotated_zx.p[1], mat_rot_x);
        multiply_matrix_vector(&tri_rotated_z.p[2], &tri_rotated_zx.p[2], mat_rot_x);

        // offset into the screen
        tri_translated        = tri_rotated_zx;
        tri_translated.p[0].z = tri_rotated_zx.p[0].z + FX(3.0f);
        tri_translated.p[1].z = tri_rotated_zx.p[1].z + FX(3.0f);
        tri_translated.p[2].z = tri_rotated_zx.p[2].z + FX(3.0f);

        // calculate the normal
        vec3d normal, line1, line2;
        line1.x = tri_translated.p[1].x - tri_translated.p[0].x;
        line1.y = tri_translated.p[1].y - tri_translated.p[0].y;
        line1.z = tri_translated.p[1].z - tri_translated.p[0].z;

        line2.x = tri_translated.p[2].x - tri_translated.p[0].x;
        line2.y = tri_translated.p[2].y - tri_translated.p[0].y;
        line2.z = tri_translated.p[2].z - tri_translated.p[0].z;

        normal.x = MUL(line1.y, line2.z) - MUL(line1.z, line2.y);
        normal.y = MUL(line1.z, line2.x) - MUL(line1.x, line2.z);
        normal.z = MUL(line1.x, line2.y) - MUL(line1.y, line2.x);

        if (is_lighting_ena)
        {
            fx32 l = SQRT(MUL(normal.x, normal.x) + MUL(normal.y, normal.y) + MUL(normal.z, normal.z));
            if (l != 0)
            {
                normal.x = DIV(normal.x, l);
                normal.y = DIV(normal.y, l);
                normal.z = DIV(normal.z, l);
            }
        }

        if (MUL(normal.x, (tri_translated.p[0].x - vec_camera.x)) +
                MUL(normal.y, (tri_translated.p[0].y - vec_camera.y)) +
                MUL(normal.z, (tri_translated.p[0].z - vec_camera.z)) <
            FX(0.0f))
        {
            fx32 dp = FX(1.0f);
            if (is_lighting_ena)
            {
                // illumination
                vec3d light_direction = {FX(0.0f), FX(0.0f), FX(-1.0f)};
                fx32  l = SQRT(MUL(light_direction.x, light_direction.x) + MUL(light_direction.y, light_direction.y) +
                              MUL(light_direction.z, light_direction.z));
                if (l != 0)
                {
                    light_direction.x = DIV(light_direction.x, l);
                    light_direction.y = DIV(light_direction.y, l);
                    light_direction.z = DIV(light_direction.z, l);

                    dp = MUL(normal.x, light_direction.x) + MUL(normal.y, light_direction.y) +
                         MUL(normal.z, light_direction.z);

                    if (dp < FX(0.0f))
                        dp = FX(0.0f);
                }
            }
            tri_translated.col.x = dp;
            tri_translated.col.y = dp;
            tri_translated.col.z = dp;

            // project triangles from 3D to 2D
            multiply_matrix_vector(&tri_translated.p[0], &tri_projected.p[0], mat_proj);
            multiply_matrix_vector(&tri_translated.p[1], &tri_projected.p[1], mat_proj);
            multiply_matrix_vector(&tri_translated.p[2], &tri_projected.p[2], mat_proj);
            tri_projected.col = tri_translated.col;

            // scale into view
            tri_projected.p[0].x += FX(1.0f);
            tri_projected.p[0].y += FX(1.0f);
            tri_projected.p[1].x += FX(1.0f);
            tri_projected.p[1].y += FX(1.0f);
            tri_projected.p[2].x += FX(1.0f);
            tri_projected.p[2].y += FX(1.0f);

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