// xosera_main.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//
// This project would not be possible without learning from the following
// open projects (and many others, no doubt):
//
// YaGraphCon       - http://www.frank-buss.de/yagraphcon/
// yavga            - https://opencores.org/projects/yavga
// f32c             - https://github.com/f32c
// up5k_vga         - https://github.com/emeb/up5k_vga
// icestation-32    - https://github.com/dan-rodrigues/icestation-32Tanger
// ice40-playground - https://github.com/smunaut/ice40-playground
// Project-F        - https://github.com/projf/projf-explore
//
// Also the following web sites:
// Hamsterworks     - https://web.archive.org/web/20190119005744/http://hamsterworks.co.nz/mediawiki/index.php/Main_Page
//                    (Archived, but not forgotten - Thanks Mike Fields)
// John's FPGA Page - http://members.optushome.com.au/jekent/FPGA.htm
// FPGA4Fun         - https://www.fpga4fun.com/
// Nandland         - https://www.nandland.com/
// Project-F        - https://projectf.io/
// Alchrity         - https://alchitry.com/
//
// 1BitSquared Discord server has also been welcoming and helpful - https://1bitsquared.com/pages/chat
//
// Special thanks to everyone involved with the IceStorm/Yosys/NextPNR (etc.) open source FPGA projects.
// Consider supporting open source FPGA tool development: https://www.patreon.com/fpga_dave

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module xosera_main(
    input  wire logic         clk,                    // pixel clock
    input  wire logic         bus_cs_n_i,             // register select strobe (active low)
    input  wire logic         bus_rd_nwr_i,           // 0 = write, 1 = read
    input  wire logic [3:0]   bus_reg_num_i,          // register number
    input  wire logic         bus_bytesel_i,          // 0 = even byte, 1 = odd byte
    input  wire logic [7:0]   bus_data_i,             // 8-bit data bus input
    output logic      [7:0]   bus_data_o,             // 8-bit data bus output
    output logic              bus_intr_o,             // Xosera CPU interrupt strobe
    output logic      [3:0]   red_o, green_o, blue_o, // RGB 4-bit color outputs
    output logic              hsync_o, vsync_o,       // horizontal and vertical sync
    output logic              dv_de_o,                // pixel visible (aka display enable)
    output logic              audio_l_o, audio_r_o,   // left and right audio PWM output
    output logic              reconfig_o,             // reconfigure iCE40 from flash
    output logic      [1:0]   boot_select_o,          // reconfigure congigureation number (0-3)
    input  wire logic         reset_i                 // reset signal
);

// video generation
logic           vgen_vram_sel;      // video gen vram select (read only)
logic [15:0]    vgen_vram_addr;     // video gen vram addr

logic           dv_de;              // display enable
logic           hsync;              // hsync
logic           vsync;              // vsync

logic  [7:0]    colorA_index     /* verilator public */; // COLORMEM index
logic [15:0]    colorA_xrgb      /* verilator public */; // COLORMEM XRGB output

logic  [7:0]    colorB_index    /* verilator public */; // COLOR2MEM index          // TODO: playfield B
logic [15:0]    colorB_xrgb     /* verilator public */; // COLOR2MEM XRGB output    // TODO:

//  VRAM read output data (for vgen, regs, blit, draw)
logic [15:0]    vram_data_out   /* verilator public */;

// register interface vram/xr access
logic           regs_vram_sel   /* verilator public */;
logic           regs_vram_ack   /* verilator public */;
logic           regs_xr_sel     /* verilator public */;
logic           regs_xr_ack     /* verilator public */;
logic           regs_wr         /* verilator public */;
logic  [3:0]    regs_wr_mask    /* verilator public */;
logic [15:0]    regs_vram_addr  /* verilator public */;

// blit vram/xr access
logic           blit_vram_sel   /* verilator public */  = 1'b0;
logic           blit_vram_ack   /* verilator public */;
logic           blit_wr         /* verilator public */  = 1'b0;
logic  [3:0]    blit_wr_mask    /* verilator public */  = 4'b0;
logic [15:0]    blit_vram_addr  /* verilator public */  = 16'b0;
logic [15:0]    blit_vram_data  /* verilator public */  = 16'b0;

// draw vram/xr access
logic           draw_vram_sel   /* verilator public */  = 1'b0;
logic           draw_vram_ack   /* verilator public */;
logic           draw_xr_sel     /* verilator public */  = 1'b0;
logic           draw_xr_ack     /* verilator public */  = 1'b0;
logic           draw_wr         /* verilator public */  = 1'b0;
logic  [3:0]    draw_wr_mask    /* verilator public */  = 4'b0;
logic [15:0]    draw_vram_addr  /* verilator public */  = 16'b0;
logic [15:0]    draw_vram_data  /* verilator public */  = 16'b0;

// XR register bus access
logic           xr_regs_wr_en     /* verilator public */;
logic [15:0]    xr_regs_addr      /* verilator public */;
logic [15:0]    xr_regs_data_out  /* verilator public */;
logic [15:0]    xr_regs_data_in   /* verilator public */;

// XR register unit select signals
logic           vgen_reg_wr_en;   // vgen XR register 0x000X & 0x001X
/* verilator lint_off UNUSED */
logic           blit_reg_wr_en;   // blit XR register 0x002X    // TODO
logic           draw_reg_wr_en;   // draw XR register 0x003X    // TODO
/* verilator lint_on UNUSED */

assign vgen_reg_wr_en = xr_regs_wr_en && (xr_regs_addr[6:5] == xv::XR_CONFIG_REGS[6:5]);    // vgen reg write
assign blit_reg_wr_en = xr_regs_wr_en && (xr_regs_addr[6:4] == xv::XR_BLIT_REGS[6:4]);      // blit reg write
assign draw_reg_wr_en = xr_regs_wr_en && (xr_regs_addr[6:4] == xv::XR_DRAW_REGS[6:4]);      // draw reg write

// XM top-level register signals
logic [15:0]    xm_regs_addr      /* verilator public */;     // register interface VRAM/XR addr
logic [15:0]    xm_regs_data_out  /* verilator public */;     // register interface bus VRAM/XR data write
logic [15:0]    xm_regs_data_in   /* verilator public */;     // register interface bus VRAM/XR data read

// vgen tile memory read signals
logic                           vgen_tile_sel           /* verilator public */;
logic [xv::TILE_AWIDTH-1:0]     vgen_tile_addr          /* verilator public */;
logic [15:0]                    vgen_tile_data          /* verilator public */;

// copper bus signals
logic                           copp_prog_rd_en;
logic [xv::COPPER_AWIDTH-1:0]   copper_pc;
logic [31:0]                    copp_prog_data_out;
logic                           copp_xr_wr_en          /* verilator public */;
logic                           copp_xr_ack          /* verilator public */;
logic [15:0]                    copp_xr_addr         /* verilator public */;
logic [15:0]                    copp_xr_data_out        /* verilator public */;
logic                           copp_reg_wr;
logic [15:0]                    copp_reg_data;
logic [10:0]                    video_h_count;
logic [10:0]                    video_v_count;

// interrupt management signals
logic  [3:0]    intr_mask;          // true for each enabled interrupt
logic  [3:0]    intr_status;        // pending interrupt status
logic  [3:0]    intr_signal;        // interrupt signalled by Copper (or CPU)
logic  [3:0]    intr_clear;         // interrupt cleared by CPU

`ifdef BUS_DEBUG_SIGNALS
logic           dbug_cs_strobe;     // debug "ack" bus strobe
`endif

`ifdef BUS_DEBUG_SIGNALS
assign audio_l_o = dbug_cs_strobe;  // debug to see when CS noticed
assign audio_r_o = regs_xr_sel;     // debug to see when XR bus selected
`else
// TODO: audio generation
assign audio_l_o = 1'b0;
assign audio_r_o = 1'b0;
`endif

// draw
logic           draw_busy = 1'b0;       // is draw busy?

// register interface for CPU access
reg_interface reg_interface(
    // bus
    .bus_cs_n_i(bus_cs_n_i),            // bus chip select
    .bus_rd_nwr_i(bus_rd_nwr_i),        // 0=write, 1=read
    .bus_reg_num_i(bus_reg_num_i),      // register number (0-15)
    .bus_bytesel_i(bus_bytesel_i),      // 0=even byte, 1=odd byte
    .bus_data_i(bus_data_i),            // 8-bit data bus input
    .bus_data_o(bus_data_o),            // 8-bit data bus output
    // VRAM/XR
    .vram_ack_i(regs_vram_ack),         // register interface ack (after reg read/write cycle)
    .xr_ack_i(regs_xr_ack),             // register interface ack (after reg read/write cycle)
    .regs_vram_sel_o(regs_vram_sel),    // register interface vram select
    .regs_xr_sel_o(regs_xr_sel),        // register interface XR memory select
    .regs_wr_o(regs_wr),                // register interface write
    .regs_wrmask_o(regs_wr_mask),       // vram nibble masks
    .regs_addr_o(xm_regs_addr),            // vram/XR address
    .regs_data_o(xm_regs_data_out),        // 16-bit word write to XR/vram
    .regs_data_i(vram_data_out),        // 16-bit word read from vram
    .xr_data_i(xm_regs_data_in),           // 16-bit word read from XR
    //
    .busy_i(draw_busy),                 // TODO: blit engine busy
    // reconfig
    .reconfig_o(reconfig_o),
    .boot_select_o(boot_select_o),
    // interrupts
    .intr_mask_o(intr_mask),            // set with write to SYS_CTRL
    .intr_clear_o(intr_clear),          // strobe with write to TIMER
`ifdef BUS_DEBUG_SIGNALS
    .bus_ack_o(dbug_cs_strobe),         // debug "ack" bus strobe
`endif
    .reset_i(reset_i),
    .clk(clk)
);

`ifdef DRAW_ENABLE
// draw
draw draw(
    .oe_i(draw_vram_ack),                  // output enable

    .draw_reg_wr_i(),                      // DRAW_TODO
    .draw_reg_num_i(),                     // DRAW_TODO
    .draw_reg_data_i(),                    // DRAW_TODO

    .draw_vram_sel_o(draw_vram_sel),       // draw VRAM select
    .draw_wr_o(draw_wr),                   // draw VRAM write
    .draw_mask_o(draw_wr_mask),            // draw vram nibble masks
    .draw_addr_o(draw_vram_addr),          // draw VRAM addr
    .draw_data_out_o(draw_vram_data),      // draw bus VRAM data write

    .busy_o(draw_busy),                    // is busy?

    .clk(clk),                             // input clk
    .reset_i(reset_i)                      // reset
);
`endif    

//  video generation
video_gen video_gen(
    .vgen_reg_wr_en_i(vgen_reg_wr_en),
    .vgen_reg_num_i(xr_regs_addr[4:0]),
    .vgen_reg_data_i(xr_regs_data_in),
    .vgen_reg_data_o(xr_regs_data_out),
    .intr_status_i(intr_status),        // status read from VID_CTRL
    .intr_signal_o(intr_signal),        // signaled by write to VID_CTRL
    .vram_sel_o(vgen_vram_sel),
    .vram_addr_o(vgen_vram_addr),
    .vram_data_i(vram_data_out),
    .tilemem_sel_o(vgen_tile_sel),
    .tilemem_addr_o(vgen_tile_addr),
    .tilemem_data_i(vgen_tile_data),
    .colorA_index_o(colorA_index),
    .colorB_index_o(colorB_index),
    .hsync_o(hsync),
    .vsync_o(vsync),
    .dv_de_o(dv_de),
`ifndef COPPER_DISABLE
    .copp_reg_wr_o(copp_reg_wr),
    .copp_reg_data_o(copp_reg_data),
    .h_count_o(video_h_count),
    .v_count_o(video_v_count),
`endif
    .reset_i(reset_i),
    .clk(clk)
);

`ifndef COPPER_DISABLE
// Copper
copper copper(
    .xr_wr_en_o(copp_xr_wr_en),
    .xr_wr_ack_i(copp_xr_ack),
    .xr_wr_addr_o(copp_xr_addr),
    .xr_wr_data_o(copp_xr_data_out),
    .coppermem_rd_addr_o(copper_pc),
    .coppermem_rd_en_o(copp_prog_rd_en),
    .coppermem_rd_data_i(copp_prog_data_out),    // 32-bit
    .copp_reg_wr_i(copp_reg_wr),
    .copp_reg_data_i(copp_reg_data),
    .h_count_i(video_h_count),
    .v_count_i(video_v_count),
    .reset_i(reset_i),
    .clk(clk)
);
`endif

// VRAM memory arbitration
vram_arb vram_arb(
    // video gen
    .vram_data_o(vram_data_out),
    .vgen_sel_i(vgen_vram_sel),
    .vgen_addr_i(vgen_vram_addr),
    // register interface
    .regs_sel_i(regs_vram_sel),
    .regs_ack_o(regs_vram_ack),
    .regs_wr_i(regs_wr & regs_vram_sel),
    .regs_wr_mask_i(regs_wr_mask),
    .regs_addr_i(xm_regs_addr),
    .regs_data_i(xm_regs_data_out),
    // TODO: 2D blit
    .blit_sel_i(blit_vram_sel),
    .blit_ack_o(blit_vram_ack),
    .blit_wr_i(blit_wr & blit_vram_sel),
    .blit_wr_mask_i(blit_wr_mask),
    .blit_addr_i(blit_vram_addr),
    .blit_data_i(blit_vram_data),
    // TODO: polygon draw
    .draw_sel_i(draw_vram_sel),
    .draw_ack_o(draw_vram_ack),
    .draw_wr_i(draw_wr & regs_vram_sel),
    .draw_wr_mask_i(draw_wr_mask),
    .draw_addr_i(draw_vram_addr),
    .draw_data_i(draw_vram_data),

    .clk(clk)
);

// XR memory arbitration (conbines all other memory regions)
xrmem_arb xrmem_arb
(
    // regs XR register/memory interface (read/write)
    .xr_sel_i(regs_xr_sel),
    .xr_ack_o(regs_xr_ack),
    .xr_wr_i(regs_wr),
    .xr_addr_i(xm_regs_addr),
    .xr_data_i(xm_regs_data_out),
    .xr_data_o(xm_regs_data_in),

    // copper XR register/memory interface (write-only)
    .copp_xr_sel_i(copp_xr_wr_en),
    .copp_xr_ack_o(copp_xr_ack),
    .copp_xr_addr_i(copp_xr_addr),
    .copp_xr_data_i(copp_xr_data_out),

    // XR register bus (read/write)
    .xreg_wr_o(xr_regs_wr_en),
    .xreg_addr_o(xr_regs_addr),
    .xreg_data_i(xr_regs_data_out),
    .xreg_data_o(xr_regs_data_in),

    // color lookup colormem A+B 2 x 16-bit bus (read-only)
    .vgen_color_sel_i(dv_de),
    .vgen_colorA_addr_i(colorA_index),
    .vgen_colorA_data_o(colorA_xrgb),
`ifdef ENABLE_PB
    .vgen_colorB_data_o(colorB_xrgb),
    .vgen_colorB_addr_i(colorB_index),
`endif

    // video generation tilemem bus (read-only)
    .vgen_tile_sel_i(vgen_tile_sel),
    .vgen_tile_addr_i(vgen_tile_addr),
    .vgen_tile_data_o(vgen_tile_data),

    // copper program coppermem 32-bit bus (read-only)
    .copp_prog_sel_i(copp_prog_rd_en),
    .copp_prog_addr_i(copper_pc),
    .copp_prog_data_o(copp_prog_data_out),

    .clk(clk)
);

video_blend video_blend(
.vsync_i(vsync),
.hsync_i(hsync),
.dv_de_i(dv_de),
.colorA_xrgb_i(colorA_xrgb),
`ifdef ENABLE_PB
.colorB_xrgb_i(colorB_xrgb),
`endif
.red_o(red_o),
.green_o(green_o),
.blue_o(blue_o),
.hsync_o(hsync_o),
.vsync_o(vsync_o),
.dv_de_o(dv_de_o),
.clk(clk)
);


// interrupt handling
always_ff @(posedge clk) begin
    if (reset_i) begin
        bus_intr_o  <= 1'b0;
        intr_status <= 4'b0;
    end else begin
        // signal a bus interrupt if not masked and not set in status and
        if ((intr_signal & intr_mask & (~intr_status)) != 4'b0) begin
            bus_intr_o  <= 1'b1;
        end else begin
            bus_intr_o  <= 1'b0;
        end
        // remember interrupt signal and clear cleared interrupts
        intr_status <= (intr_status | intr_signal) & (~intr_clear);
    end
end

endmodule
`default_nettype wire               // restore default
