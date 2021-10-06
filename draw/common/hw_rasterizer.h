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

#ifndef HW_RASTERIZER_H
#define HW_RASTERIZER_H

void hw_draw_line(int x0, int y0, int x1, int y1, int color);
void hw_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color);
void hw_draw_filled_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color);
void hw_draw_filled_rectangle(int x0, int y0, int x1, int y1, int color);

#endif