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
 * Draw reference implementation
 * ------------------------------------------------------------
 */

#include <SDL2/SDL.h>
#include <stdbool.h>

#include <cube.h>
#include <sw_rasterizer.h>
#include <teapot.h>

static int screen_width  = 1280;
static int screen_height = 800;

static SDL_Renderer * renderer;

void draw_pixel(int x, int y, int color)
{
    SDL_SetRenderDrawColor(renderer, color, color, color, SDL_ALPHA_OPAQUE);
    SDL_RenderDrawPoint(renderer, x, y);
}

void xd_draw_line(int x0, int y0, int x1, int y1, int color)
{
    sw_draw_line(x0, y0, x1, y1, color);
}

void xd_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
    sw_draw_triangle(x0, y0, x1, y1, x2, y2, color);
}

void xd_draw_filled_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
    // hack: limit the dynamic range to be like the hw
    sw_draw_filled_triangle(x0, y0, x1, y1, x2, y2, color & 0xF0);
}

void xd_draw_filled_rectangle(int x0, int y0, int x1, int y1, int color)
{
    sw_draw_filled_rectangle(x0, y0, x1, y1, color);
}

int main(int argc, char * argv[])
{
    sw_init_rasterizer(draw_pixel);

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window * window = SDL_CreateWindow("Xosera Draw Reference Implementation",
                                           SDL_WINDOWPOS_CENTERED_DISPLAY(1),
                                           SDL_WINDOWPOS_UNDEFINED,
                                           screen_width,
                                           screen_height,
                                           0);

    // SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    SDL_Event e;
    int       quit = 0;

    // Projection matrix
    mat4x4 mat_proj = matrix_make_projection(screen_width, screen_height, 60.0f);

    mat4x4 mat_rot_z, mat_rot_x;

    float theta = 0.0f;

    model_t * teapot_model = load_teapot();
    model_t * cube_model   = load_cube();

    vec3d vec_up     = {FX(0.0f), FX(1.0f), FX(0.0f), FX(1.0f)};
    vec3d vec_camera = {FX(0.0f), FX(0.0f), FX(0.0f), FX(1.0f)};

    unsigned int time = SDL_GetTicks();

    float yaw = 0.0f;

    while (!quit)
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        // Rotation Z
        mat_rot_z = matrix_make_rotation_z(theta);

        // Rotation X
        mat_rot_x = matrix_make_rotation_x(theta);

        vec3d  vec_target     = {FX(0.0f), FX(0.0f), FX(1.0f), FX(1.0f)};
        mat4x4 mat_camera_rot = matrix_make_rotation_y(yaw);
        vec3d  vec_look_dir   = matrix_multiply_vector(&mat_camera_rot, &vec_target);
        vec_target            = vector_add(&vec_camera, &vec_look_dir);

        mat4x4 mat_camera = matrix_point_at(&vec_camera, &vec_target, &vec_up);

        // make view matrix from camera
        mat4x4 mat_view = matrix_quick_inverse(&mat_camera);


        // Draw teapot

        draw_model(screen_width,
                   screen_height,
                   &vec_camera,
                   teapot_model,
                   &mat_proj,
                   &mat_view,
                   &mat_rot_z,
                   &mat_rot_x,
                   true,
                   true);
        // draw_model(screen_width,
        //            screen_height,
        //            &vec_camera,
        //            cube_model,
        //            &mat_proj,
        //            &mat_view,
        //            &mat_rot_z,
        //            &mat_rot_x,
        //            true,
        //            false);

        SDL_RenderPresent(renderer);

        // printf("%d ms\n", SDL_GetTicks() - time);
        float elapsed_time = (float)(SDL_GetTicks() - time) / 1000.0f;
        time               = SDL_GetTicks();

        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = 1;
            }
            else if (e.type == SDL_KEYDOWN)
            {
                vec3d vec_forward = vector_mul(&vec_look_dir, MUL(FX(2.0f), FX(elapsed_time)));
                switch (e.key.keysym.scancode)
                {
                    case SDL_SCANCODE_ESCAPE:
                        quit = 1;
                        break;

                    case SDL_SCANCODE_UP:
                        vec_camera.y += MUL(FX(8.0f), FX(elapsed_time));
                        break;
                    case SDL_SCANCODE_DOWN:
                        vec_camera.y -= MUL(FX(8.0f), FX(elapsed_time));
                        break;
                    case SDL_SCANCODE_LEFT:
                        vec_camera.x -= MUL(FX(8.0f), FX(elapsed_time));
                        break;
                    case SDL_SCANCODE_RIGHT:
                        vec_camera.x += MUL(FX(8.0f), FX(elapsed_time));
                        break;
                    case SDL_SCANCODE_W:
                        vec_camera = vector_add(&vec_camera, &vec_forward);
                        break;
                    case SDL_SCANCODE_S:
                        vec_camera = vector_sub(&vec_camera, &vec_forward);
                        break;
                    case SDL_SCANCODE_A:
                        yaw -= 2.0f * elapsed_time;
                        break;
                    case SDL_SCANCODE_D:
                        yaw += 2.0f * elapsed_time;
                        break;
                    default:
                        // do nothing
                        break;
                }
            }
        }

        theta += 0.001f;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
