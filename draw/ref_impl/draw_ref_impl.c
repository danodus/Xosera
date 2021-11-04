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

static int screen_width  = 320;
static int screen_height = 200;

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
                                           SDL_WINDOWPOS_UNDEFINED,
                                           SDL_WINDOWPOS_UNDEFINED,
                                           screen_width,
                                           screen_height,
                                           0);

    // SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);

    SDL_Event e;
    int       quit = 0;

    // Projection matrix
    mat4x4 mat_proj;
    get_projection_matrix(screen_width, screen_height, 60.0f, &mat_proj);

    mat4x4 mat_rot_z, mat_rot_x;

    float theta = 0.0f;

    model_t * teapot_model = load_teapot();

    while (!quit)
    {
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
        SDL_RenderClear(renderer);

        // Rotation Z
        get_rotation_z_matrix(theta, &mat_rot_z);

        // Rotation X
        get_rotation_x_matrix(theta, &mat_rot_x);

        // Draw teapot
        draw_model(screen_width, screen_height, teapot_model, &mat_proj, &mat_rot_z, &mat_rot_x, true, false);

        SDL_RenderPresent(renderer);

        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
            {
                quit = 1;
            }
            else if (e.type == SDL_KEYDOWN)
            {
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
                    quit = 1;
            }
        }

        theta += 0.001f;
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
