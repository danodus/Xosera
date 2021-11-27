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
 * Draw API
 * ------------------------------------------------------------
 */

#include "draw_api.h"

#include "hw_rasterizer.h"
#include "sw_rasterizer.h"

#include <xosera_m68k_api.h>

#include <draw.h>

extern volatile uint32_t XFrameCount;

static uint8_t  g_disp_buffer;
static uint16_t g_start_line;
static uint16_t g_width;
static uint16_t g_height;
static uint8_t  g_bpp;
static uint16_t g_first_disp_buffer_addr, g_second_disp_buffer_addr;
static uint16_t g_cur_draw_buffer_addr;

static bool g_hw_rasterizer = false;

fx32 xd_hw_mult(fx32 x, fx32 y)
{
    xm_setl(MULT_MSW, (uint32_t)x);
    xm_setl(MULT_MSW, (uint32_t)y);

    return (fx32)(xm_getl(MULT_MSW));
}

void xd_wait_done()
{
    uint8_t sys_ctrl;
    do
    {
        sys_ctrl = xm_getbl(SYS_CTRL);
    } while (sys_ctrl & 0x80);
}

void xd_wait_frame()
{
    uint32_t f = XFrameCount;
    while (XFrameCount == f)
        ;
}

static void draw_pixel(int x, int y, int color)
{
    uint16_t ux = x;
    uint16_t uy = y;
    uint8_t  uc = color;
    if (ux < g_width && uy < g_height)
    {
        uint16_t addr = g_cur_draw_buffer_addr + (uy * (g_width / 2)) + (ux / 2);
        xm_setw(WR_ADDR, addr);
        xm_setw(SYS_CTRL, (ux & 1) ? 0x0308 : 0x0C08);

        xm_setbh(DATA, uc);
        xm_setbl(DATA, uc);
    }
}

void xd_init(bool hw_rasterizer, int start_line, int width, int height, int bpp)
{
    sw_init_rasterizer(draw_pixel);
    g_hw_rasterizer = hw_rasterizer;
    g_start_line    = start_line;
    g_width         = width;
    g_height        = height;
    g_bpp           = bpp;
    xd_wait_done();
    xreg_setw(DRAW_DEST_WIDTH, width);
    xreg_setw(DRAW_DEST_HEIGHT, height);
    xreg_setw(DRAW_GFX_CTRL, bpp == 4 ? 0x0001 : 0x0002);
}

void xd_init_swap()
{
    g_disp_buffer = 0;

    g_first_disp_buffer_addr  = g_start_line * g_width / 2;
    g_second_disp_buffer_addr = g_first_disp_buffer_addr + g_height * g_width / 2;

    xreg_setw(PA_DISP_ADDR, g_first_disp_buffer_addr);
    // xreg_setw(PA_LINE_ADDR, 0x0000);
    xd_wait_done();
    g_cur_draw_buffer_addr = g_second_disp_buffer_addr;
    xreg_setw(DRAW_DEST_ADDR, g_cur_draw_buffer_addr);
}

void xd_swap(bool is_vsync_enabled)
{
    xd_wait_done();
    if (is_vsync_enabled)
        xd_wait_frame();

    if (g_disp_buffer)
    {
        g_disp_buffer = 0;
        xreg_setw(PA_DISP_ADDR, g_first_disp_buffer_addr);
        g_cur_draw_buffer_addr = g_second_disp_buffer_addr;
    }
    else
    {
        g_disp_buffer = 1;
        xreg_setw(PA_DISP_ADDR, g_second_disp_buffer_addr);
        g_cur_draw_buffer_addr = g_first_disp_buffer_addr;
    }
    xreg_setw(DRAW_DEST_ADDR, g_cur_draw_buffer_addr);
}

uint16_t xd_swap_copper(bool is_vsync_enabled)
{
    xd_wait_done();
    if (is_vsync_enabled)
        xd_wait_frame();

    uint16_t addr;

    if (g_disp_buffer)
    {
        g_disp_buffer          = 0;
        addr                   = g_first_disp_buffer_addr;
        g_cur_draw_buffer_addr = g_second_disp_buffer_addr;
    }
    else
    {
        g_disp_buffer          = 1;
        addr                   = g_second_disp_buffer_addr;
        g_cur_draw_buffer_addr = g_first_disp_buffer_addr;
    }
    xreg_setw(DRAW_DEST_ADDR, g_cur_draw_buffer_addr);

    return addr;
}


void xd_clear()
{
    // temp hack, always accelerated
    hw_draw_filled_rectangle(0, 0, g_width - 1, g_height - 1, 1);
}

void xd_finish()
{
    xd_wait_done();
}

void xd_draw_line(int x0, int y0, int x1, int y1, int color)
{
    if (g_hw_rasterizer)
    {
        hw_draw_line(x0, y0, x1, y1, color);
    }
    else
    {
        sw_draw_line(x0, y0, x1, y1, color);
        xm_setw(SYS_CTRL, 0x0F08);
    }
}

void xd_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
    if (g_hw_rasterizer)
    {
        hw_draw_triangle(x0, y0, x1, y1, x2, y2, color);
    }
    else
    {
        sw_draw_triangle(x0, y0, x1, y1, x2, y2, color);
        xm_setw(SYS_CTRL, 0x0F08);
    }
}

void xd_draw_filled_triangle(int x0, int y0, int x1, int y1, int x2, int y2, int color)
{
    if (g_hw_rasterizer)
    {
        hw_draw_filled_triangle(x0, y0, x1, y1, x2, y2, color);
    }
    else
    {
        sw_draw_filled_triangle(x0, y0, x1, y1, x2, y2, color);
        xm_setw(SYS_CTRL, 0x0F08);
    }
}

void xd_draw_filled_rectangle(int x0, int y0, int x1, int y1, int color)
{
    if (g_hw_rasterizer)
    {
        hw_draw_filled_rectangle(x0, y0, x1, y1, color);
    }
    else
    {
        sw_draw_filled_rectangle(x0, y0, x1, y1, color);
        xm_setw(SYS_CTRL, 0x0F08);
    }
}

void xd_draw_textured_triangle(int         x0,
                               int         y0,
                               fx32        u0,
                               fx32        v0,
                               int         x1,
                               int         y1,
                               fx32        u1,
                               fx32        v1,
                               int         x2,
                               int         y2,
                               fx32        u2,
                               fx32        v2,
                               texture_t * tex)
{
    // Currently only available with the software rasterizer
    if (!g_hw_rasterizer)
    {
        sw_draw_textured_triangle(x0, y0, u0, v0, x1, y1, u1, v1, x2, y2, u2, v2, tex);
        xm_setw(SYS_CTRL, 0x0F08);
    }
}