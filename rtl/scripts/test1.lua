-- Clear screen 4-bpp 320x240

XM_WR_INCR = 0x8
XM_WR_ADDR = 0x9
XM_DATA = 0xA

XR_PA_GFX_CTRL = 0x10
XR_PA_DISP_ADDR = 0x12
XR_PA_LINE_LEN = 0x13

xreg_setw(XR_PA_DISP_ADDR, 0)
xreg_setw(XR_PA_LINE_LEN, 320/4)
xreg_setw(XR_PA_GFX_CTRL, 0x0055)
xm_setw(XM_WR_INCR, 0x0001)
xm_setw(XM_WR_ADDR, 0)

for y=0,(240-1) do
    for x=0,(320/4-1) do
        xm_setw(XM_DATA, (x % 8 == 0 or y % 8 == 0) and 0xFFFF or 0x1111)
    end
end
