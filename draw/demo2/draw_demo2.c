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
 * Draw demo 2
 * ------------------------------------------------------------
 */

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <basicio.h>
#include <machine.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846264338327950288
#endif

#include <draw_api.h>
#include <teapot.h>
#include <xosera_m68k_api.h>

// Note: Not enough memory for full 848x480
#define MODE_848_480 1

extern void install_intr(void);
extern void remove_intr(void);

const uint16_t defpal[16] = {
    0x0000,        // black
    0x000A,        // blue
    0x00A0,        // green
    0x00AA,        // cyan
    0x0A00,        // red
    0x0A0A,        // magenta
    0x0AA0,        // brown
    0x0AAA,        // light gray
    0x0555,        // dark gray
    0x055F,        // light blue
    0x05F5,        // light green
    0x05FF,        // light cyan
    0x0F55,        // light red
    0x0F5F,        // light magenta
    0x0FF5,        // yellow
    0x0FFF         // white
};

uint16_t screen_addr;
uint8_t  text_columns;
uint8_t  text_rows;
int8_t   text_h;
int8_t   text_v;
uint8_t  text_color = 0x02;        // dark green on black

model_t * teapot_model;

#if MODE_848_480
const int width = 848;
#else
const int width = 640;
#endif
const int height = 300;

const uint8_t  copper_list_len = 10;
const uint16_t copper_list[]   = {
    0x005A,
    0x0002,        // wait  0, 90                   ; Wait for line 90, H position ignored
    0x9010,
    0x0050,        // mover 0x0050, PA_GFX_CTRL     ; Set to 4-bpp
    0x0186,
    0x0002,        // wait  0, 390                  ; Wait for line 390, H position ignored
    0x9010,
    0x00D0,        // mover 0x00D0, PA_GFX_CTRL     ; Blank
    0x0000,
    0x0003        // nextf
};

static void get_textmode_settings()
{
    uint16_t vx          = (xreg_getw(PA_GFX_CTRL) & 3) + 1;
    uint16_t tile_height = (xreg_getw(PA_TILE_CTRL) & 0xf) + 1;
    screen_addr          = xreg_getw(PA_DISP_ADDR);
    text_columns         = (uint8_t)xreg_getw(PA_LINE_LEN);
    text_rows            = (uint8_t)(((xreg_getw(VID_VSIZE) / vx) + (tile_height - 1)) / tile_height);
}

static void xpos(uint8_t h, uint8_t v)
{
    text_h = h;
    text_v = v;
}

static void xcolor(uint8_t color)
{
    text_color = color;
}

static void xhome()
{
    get_textmode_settings();
    xpos(0, 0);
}

static void xcls()
{
    // clear screen
    xhome();
    xm_setw(WR_ADDR, screen_addr);
    xm_setw(WR_INCR, 1);
    xm_setbh(DATA, text_color);
    for (uint16_t i = 0; i < (text_columns * text_rows); i++)
    {
        xm_setbl(DATA, ' ');
    }
    xm_setw(WR_ADDR, screen_addr);
}

static void xprint(const char * str)
{
    xm_setw(WR_INCR, 1);
    xm_setw(WR_ADDR, screen_addr + (text_v * text_columns) + text_h);
    xm_setbh(DATA, text_color);

    char c;
    while ((c = *str++) != '\0')
    {
        if (c >= ' ')
        {
            xm_setbl(DATA, c);
            if (++text_h >= text_columns)
            {
                text_h = 0;
                if (++text_v >= text_rows)
                {
                    text_v = 0;
                }
            }
            continue;
        }
        switch (c)
        {
            case '\r':
                text_h = 0;
                xm_setw(WR_ADDR, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\n':
                text_h = 0;
                if (++text_v >= text_rows)
                {
                    text_v = text_rows - 1;
                }
                xm_setw(WR_ADDR, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\b':
                if (--text_h < 0)
                {
                    text_h = text_columns - 1;
                    if (--text_v < 0)
                    {
                        text_v = 0;
                    }
                }
                xm_setw(WR_ADDR, screen_addr + (text_v * text_columns) + text_h);
                break;
            case '\f':
                xcls();
                break;
            default:
                break;
        }
    }
}

static char xprint_buff[4096];
static void xprintf(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vsnprintf(xprint_buff, sizeof(xprint_buff), fmt, args);
    xprint(xprint_buff);
    va_end(args);
}

void demo_model(int nb_iterations)
{
    float  theta = 0.0f;
    mat4x4 mat_proj, mat_rot_z, mat_rot_x;

    mat_proj = matrix_make_projection(width, height, 60.0f);

    for (int i = 0; i < nb_iterations; ++i)
    {
        xd_clear();

        mat_rot_z = matrix_make_rotation_z(theta);
        mat_rot_x = matrix_make_rotation_x(theta, &mat_rot_x);

        draw_model(width, height, teapot_model, &mat_proj, &mat_rot_z, &mat_rot_x, false, true);
        delay(4000);

        theta += 0.1f;
    }
}

void xosera_demo()
{
    // allocations
    teapot_model = load_teapot();

#if MODE_848_480
    xosera_init(1);
#else
    xosera_init(0);
#endif

    install_intr();

    xreg_setw(PA_DISP_ADDR, 0x0000);
    xreg_setw(PA_LINE_ADDR, 0x0000);
    xreg_setw(PA_LINE_LEN, width / 4);

    // set black background
    xreg_setw(VID_CTRL, 0x0000);

    xm_setw(XR_ADDR, XR_COPPER_MEM);

    for (uint8_t i = 0; i < copper_list_len; i++)
    {
        xm_setw(XR_DATA, copper_list[i]);
    }

    xd_init(true, 0, width, height, 4);

    while (1)
    {

        xreg_setw(PA_GFX_CTRL, 0x0005);

        xcolor(0x02);
        xcls();

        xprintf("Xosera\nDraw\nDemo\n2\n");
        delay(2000);

        // enable Copper
        xreg_setw(COPP_CTRL, 0x8000);

        demo_model(100);

        // disable Copper
        xreg_setw(COPP_CTRL, 0x0000);
    }
}
