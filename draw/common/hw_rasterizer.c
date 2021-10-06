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

#include "hw_rasterizer.h"

#include <xosera_m68k_api.h>

void xd_wait_done();

static void swapi(int * a, int * b)
{
    int c = *a;
    *a    = *b;
    *b    = c;
}

void hw_draw_line(int x0, int y0, int x1, int y1, int color)
{
    if (y0 > y1)
    {
        swapi(&x0, &x1);
        swapi(&y0, &y1);
    }

    xd_wait_done();
    xm_setw(XR_ADDR, XR_DRAW_REGS);
    xm_setw(XR_DATA, x0);               // DRAW_COORD_X0
    xm_setw(XR_DATA, y0);               // DRAW_COORD_Y0
    xm_setw(XR_DATA, x1);               // DRAW_COORD_X1
    xm_setw(XR_DATA, y1);               // DRAW_COORD_Y1
    xm_setw(XR_DATA, 0);                // DRAW_COORD_X2 (unused)
    xm_setw(XR_DATA, 0);                // DRAW_COORD_Y2 (unused)
    xm_setw(XR_DATA, color);            // DRAW_COLOR
    xm_setw(XR_DATA, DRAW_LINE);        // DRAW_EXECUTE
}

void hw_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
    hw_draw_line(x0, y0, x1, y1, color);
    hw_draw_line(x1, y1, x2, y2, color);
    hw_draw_line(x2, y2, x0, y0, color);
}

void hw_draw_filled_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
    if (y0 > y2)
    {
        swapi(&x0, &x2);
        swapi(&y0, &y2);
    }

    if (y0 > y1)
    {
        swapi(&x0, &x1);
        swapi(&y0, &y1);
    }

    if (y1 > y2)
    {
        swapi(&x1, &x2);
        swapi(&y1, &y2);
    }

    xd_wait_done();
    xm_setw(XR_ADDR, XR_DRAW_REGS);
    xm_setw(XR_DATA, x0);                          // DRAW_COORD_X0
    xm_setw(XR_DATA, y0);                          // DRAW_COORD_Y0
    xm_setw(XR_DATA, x1);                          // DRAW_COORD_X1
    xm_setw(XR_DATA, y1);                          // DRAW_COORD_Y1
    xm_setw(XR_DATA, x2);                          // DRAW_COORD_X2
    xm_setw(XR_DATA, y2);                          // DRAW_COORD_Y2
    xm_setw(XR_DATA, color);                       // DRAW_COLOR
    xm_setw(XR_DATA, DRAW_FILLED_TRIANGLE);        // DRAW_EXECUTE
}

void hw_draw_filled_rectangle(int x0, int y0, int x1, int y1, int color)
{
    if (y0 > y1)
    {
        swapi(&x0, &x1);
        swapi(&y0, &y1);
    }

    xd_wait_done();
    xm_setw(XR_ADDR, XR_DRAW_REGS);
    xm_setw(XR_DATA, x0);                          // DRAW_COORD_X0
    xm_setw(XR_DATA, y0);                          // DRAW_COORD_Y0
    xm_setw(XR_DATA, x1);                          // DRAW_COORD_X1
    xm_setw(XR_DATA, y0);                          // DRAW_COORD_Y1
    xm_setw(XR_DATA, x0);                          // DRAW_COORD_X2 (unused)
    xm_setw(XR_DATA, y1);                          // DRAW_COORD_Y2 (unused)
    xm_setw(XR_DATA, color);                       // DRAW_COLOR
    xm_setw(XR_DATA, DRAW_FILLED_TRIANGLE);        // DRAW_EXECUTE

    xd_wait_done();
    xm_setw(XR_ADDR, XR_DRAW_REGS);
    xm_setw(XR_DATA, x1);                          // DRAW_COORD_X0
    xm_setw(XR_DATA, y0);                          // DRAW_COORD_Y0
    xm_setw(XR_DATA, x0);                          // DRAW_COORD_X1
    xm_setw(XR_DATA, y1);                          // DRAW_COORD_Y1
    xm_setw(XR_DATA, x1);                          // DRAW_COORD_X2 (unused)
    xm_setw(XR_DATA, y1);                          // DRAW_COORD_Y2 (unused)
    xm_setw(XR_DATA, color);                       // DRAW_COLOR
    xm_setw(XR_DATA, DRAW_FILLED_TRIANGLE);        // DRAW_EXECUTE
}
