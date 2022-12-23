-- Clear screen 4-bpp 320x240

XM_RD_ADDR = 0x7
XM_WR_INCR = 0x8
XM_WR_ADDR = 0x9
XM_DATA = 0xA

XR_PA_GFX_CTRL = 0x10
XR_PA_DISP_ADDR = 0x12
XR_PA_LINE_LEN = 0x13

function grid(fg, bg)
    xreg_setw(XR_PA_DISP_ADDR, 0)
    xreg_setw(XR_PA_LINE_LEN, 320/4)
    xreg_setw(XR_PA_GFX_CTRL, 0x0055)
    xm_setw(XM_WR_INCR, 0x0001)
    xm_setw(XM_WR_ADDR, 0)

    for y=0,(240-1) do
        for x=0,(320/4-1) do
            xm_setw(XM_DATA, (x % 8 == 0 or y % 8 == 0) and fg or bg)
        end
    end
end

grid(0x1234, 0x5555)

xm_setw(XM_WR_ADDR, 0)
value = xm_setw(XM_DATA, 0x1234)

xm_setw(XM_RD_ADDR, 0)
value = xm_getw(XM_DATA)

print(string.format("Pixel at (0,0): %x", value))

