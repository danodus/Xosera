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
// RetroRamblings   - https://retroramblings.net/
// Alchrity         - https://alchitry.com/
//
// 1BitSquared Discord server has also been welcoming and helpful - https://1bitsquared.com/pages/chat
//
// Special thanks to everyone involved with the IceStorm/Yosys/NextPNR (etc.) open source FPGA projects.
// Consider supporting open source FPGA tool development: https://www.patreon.com/fpga_dave

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module xosera_main#(
    parameter   EN_VID_PF_B             = 1,        // enable playfield B
    parameter   EN_BLEND                = 1,        // enable pf B blending (else overlay only)
    parameter   EN_BLEND_ADDCLAMP       = 1,        // enable pf B clamped RGB blending
    parameter   EN_BLIT                 = 1,        // enable blit unit
    parameter   EN_BLIT_DECR_MODE       = 1,        // enable blit pointer decrementing
    parameter   EN_BLIT_DECR_LSHIFT     = 1,        // enable blit left shift when decrementing
    parameter   EN_AUDIO                = 1         // enable audio output
)
(
    input  wire logic         bus_cs_n_i,           // register select strobe (active low)
    input  wire logic         bus_rd_nwr_i,         // 0 = write, 1 = read
    input  wire logic [3:0]   bus_reg_num_i,        // register number
    input  wire logic         bus_bytesel_i,        // 0 = even byte, 1 = odd byte
    input  wire logic [7:0]   bus_data_i,           // 8-bit data bus input
    output logic      [7:0]   bus_data_o,           // 8-bit data bus output
    output logic              bus_intr_o,           // Xosera CPU interrupt strobe
    output logic      [3:0]   red_o,                // red color gun output
    output logic      [3:0]   green_o,              // green color gun output
    output logic      [3:0]   blue_o,               // blue color gun output
    output logic              hsync_o, vsync_o,     // horizontal and vertical sync
    output logic              dv_de_o,              // pixel visible (aka display enable)
    output logic              audio_l_o,            // left channel audio PWM output
    output logic              audio_r_o,            // right channel audio PWM output
    output logic              reconfig_o,           // reconfigure iCE40 from flash
    output logic      [1:0]   boot_select_o,        // reconfigure congigureation number (0-3)
    input  wire logic         reset_i,              // reset signal
    input  wire logic         clk                   // pixel clock
);

// video generation
logic                   vgen_vram_sel;      // video gen vram select (read only)
addr_t                  vgen_vram_addr;     // video gen vram addr

logic                   dv_de;              // display enable
logic                   hsync;              // hsync
logic                   vsync;              // vsync

color_t                 colorA_index;       // pf A color index
argb_t                  colorA_xrgb;        // pf A ARGB output

color_t                 colorB_index;       // pf B color index
argb_t                  colorB_xrgb;        // pf B ARGB output

//  VRAM read output data (for vgen, regs, blit)
word_t                  vram_data_out;

// register interface vram/xr access
logic                   regs_vram_sel;
logic                   regs_vram_ack;
logic                   regs_xr_sel;
logic                   regs_xr_ack;
logic                   regs_wr;
logic  [3:0]            regs_wr_mask;
//addr_t                  regs_vram_addr;

// blit vram/xr access
logic                   blit_vram_sel;
logic                   blit_vram_ack;
logic                   blit_wr;
logic  [3:0]            blit_wr_mask;
addr_t                  blit_vram_addr;
word_t                  blit_vram_data;
logic                   blit_busy;
logic                   blit_full;

`ifdef ENABLE_COPP
// copper bus signals
/* verilator lint_off UNUSED */
logic                   copp_prog_rd_en;
logic [xv::COPP_W-1:0]  copper_pc;
logic [31:0]            copp_prog_data_out;
logic                   copp_xr_wr_en;
logic                   copp_xr_ack;
addr_t                  copp_xr_addr;
word_t                  copp_xr_data_out;
logic                   copp_reg_wr;
word_t                  copp_reg_data;
hres_t                  video_h_count;
vres_t                  video_v_count;
`endif

// XR register bus access
logic                   xr_regs_wr_en;
logic  [6:0]            xr_regs_addr;
word_t                  xr_regs_data_out;
word_t                  xr_regs_data_in;

// XR register unit select signals
logic                   vgen_reg_wr_en;     // vgen XR register 0x000X & 0x001X
logic                   blit_reg_wr_en;     // blit XR register 0x002X    // TODO

// XM top-level register signals
addr_t                  xm_regs_addr;       // register interface VRAM/XR addr
word_t                  xm_regs_data_out;   // register interface bus VRAM/XR data write
word_t                  xm_regs_data_in;    // register interface bus VRAM/XR data read

// vgen tile memory read signals
logic                   vgen_tile_sel;
tile_addr_t             vgen_tile_addr;
word_t                  vgen_tile_data;

// interrupt management signals
logic  [3:0]            intr_mask;          // true for each enabled interrupt
logic  [3:0]            intr_status;        // pending interrupt status
logic  [3:0]            vid_intr_signal;    // any interrupt signalled VID_CTRL
logic  [3:0]            intr_clear;         // interrupt cleared by CPU
logic                   blit_intr;          // blit done interrupt

`ifdef BUS_DEBUG_SIGNALS
logic                   dbug_cs_strobe;     // debug "ack" bus strobe
`endif

`ifdef BUS_DEBUG_SIGNALS
assign audio_l_o    =   dbug_cs_strobe;     // debug to see when CS noticed
assign audio_r_o    =   regs_xr_sel;        // debug to see when XR bus selected
`endif

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
    .regs_addr_o(xm_regs_addr),         // vram/XR address
    .regs_data_o(xm_regs_data_out),     // 16-bit word write to XR/vram
    .regs_data_i(vram_data_out),        // 16-bit word read from vram
    .xr_data_i(xm_regs_data_in),        // 16-bit word read from XR
    //
    .blit_busy_i(blit_busy),            // blit engine busy
    .blit_full_i(blit_full),            // blit engine queue full
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

//  video generation
video_gen#(
    .EN_VID_PF_B(EN_VID_PF_B),
    .EN_AUDIO(EN_AUDIO)
) video_gen(
    .vgen_reg_wr_en_i(vgen_reg_wr_en),
    .vgen_reg_num_i(xr_regs_addr[4:0]),
    .vgen_reg_data_i(xr_regs_data_in),
    .vgen_reg_data_o(xr_regs_data_out),
    .intr_status_i(intr_status),            // status read from VID_CTRL
    .intr_signal_o(vid_intr_signal),        // signaled by write to VID_CTRL
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
`ifdef ENABLE_COPP
    .copp_reg_wr_o(copp_reg_wr),
    .copp_reg_data_o(copp_reg_data),
    .h_count_o(video_h_count),
    .v_count_o(video_v_count),
`endif
    .audio_pdm_l_o(audio_l_o),
    .audio_pdm_r_o(audio_r_o),
    .reset_i(reset_i),
    .clk(clk)
);

`ifdef ENABLE_COPP
// copper - video synchronized co-processor
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

// blitter - blit block transfer unit
generate
    if (EN_BLIT) begin : opt_EN_BLIT
        blitter2#(
            .EN_BLIT_DECR_MODE(EN_BLIT_DECR_MODE),
            .EN_BLIT_DECR_LSHIFT(EN_BLIT_DECR_LSHIFT)
        ) blitter(
            .xreg_wr_en_i(blit_reg_wr_en),
            .xreg_num_i(xr_regs_addr[3:0]),
            .xreg_data_i(xr_regs_data_in),
            .blit_busy_o(blit_busy),
            .blit_full_o(blit_full),
            .blit_done_intr_o(blit_intr),
            .blit_vram_sel_o(blit_vram_sel),
            .blit_vram_ack_i(blit_vram_ack),
            .blit_wr_o(blit_wr),
            .blit_wr_mask_o(blit_wr_mask),
            .blit_addr_o(blit_vram_addr),
            .blit_data_i(vram_data_out),
            .blit_data_o(blit_vram_data),
            .reset_i(reset_i),
            .clk(clk)
        );
    end else begin

        logic unused_blit;
        assign unused_blit = &{1'b0, blit_vram_ack };
        assign blit_vram_sel    = '0;
        assign blit_wr          = '0;
        assign blit_wr_mask     = '0;
        assign blit_vram_addr   = '0;
        assign blit_vram_data   = '0;
        assign blit_busy        = '0;
        assign blit_full        = '0;
        assign blit_intr        = '0;
    end
endgenerate

// VRAM memory arbitration
vram_arb #(
    .EN_BLIT(EN_BLIT)
) vram_arb(
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

    .clk(clk)
);

// XR memory arbitration (conbines all other memory regions)
assign vgen_reg_wr_en = xr_regs_wr_en && (xr_regs_addr[6:5] == xv::XR_CONFIG_REGS[6:5]);    // vgen reg write
assign blit_reg_wr_en = xr_regs_wr_en && (xr_regs_addr[6:4] == xv::XR_BLIT_REGS[6:4]);      // blit reg write
xrmem_arb#(
    .EN_VID_PF_B(EN_VID_PF_B)
) xrmem_arb(
    // regs XR register/memory interface (read/write)
    .xr_sel_i(regs_xr_sel),
    .xr_ack_o(regs_xr_ack),
    .xr_wr_i(regs_wr),
    .xr_addr_i(xm_regs_addr),
    .xr_data_i(xm_regs_data_out),
    .xr_data_o(xm_regs_data_in),

`ifdef ENABLE_COPP
    // copper XR register/memory interface (write-only)
    .copp_xr_sel_i(copp_xr_wr_en),
    .copp_xr_ack_o(copp_xr_ack),
    .copp_xr_addr_i(copp_xr_addr),
    .copp_xr_data_i(copp_xr_data_out),
`endif

    // XR register bus (read/write)
    .xreg_wr_o(xr_regs_wr_en),
    .xreg_addr_o(xr_regs_addr),
    .xreg_data_i(xr_regs_data_out),
    .xreg_data_o(xr_regs_data_in),

    // color lookup colormem A+B 2 x 16-bit bus (read-only)
    .vgen_color_sel_i(dv_de),
    .vgen_colorA_addr_i(colorA_index),
    .vgen_colorA_data_o(colorA_xrgb),
    .vgen_colorB_data_o(colorB_xrgb),
    .vgen_colorB_addr_i(colorB_index),

    // video generation tilemem bus (read-only)
    .vgen_tile_sel_i(vgen_tile_sel),
    .vgen_tile_addr_i(vgen_tile_addr),
    .vgen_tile_data_o(vgen_tile_data),

`ifdef ENABLE_COPP
    // copper program coppermem 32-bit bus (read-only)
    .copp_prog_sel_i(copp_prog_rd_en),
    .copp_prog_addr_i(copper_pc),
    .copp_prog_data_o(copp_prog_data_out),
`endif

    .clk(clk)
);

// video blending - alpha and other color belding between playfield A and B
generate
    if (EN_VID_PF_B) begin : opt_PF_B_BLEND
        video_blend#(
            .EN_BLEND(EN_BLEND),
            .EN_BLEND_ADDCLAMP(EN_BLEND_ADDCLAMP)
        ) video_blend(
            .vsync_i(vsync),
            .hsync_i(hsync),
            .dv_de_i(dv_de),
            .colorA_xrgb_i(colorA_xrgb),
            .colorB_xrgb_i(colorB_xrgb),
            .blend_rgb_o({ red_o, green_o, blue_o }),
            .hsync_o(hsync_o),
            .vsync_o(vsync_o),
            .dv_de_o(dv_de_o),
            .clk(clk)
        );
    end
endgenerate

// interrupt handling
always_ff @(posedge clk) begin
    if (reset_i) begin
        bus_intr_o  <= 1'b0;
        intr_status <= 4'b0;
    end else begin
        // generate bus interrupt if signal bit set, not masked and not already set
        if (((vid_intr_signal | { 2'b0, blit_intr, 1'b0 } ) & intr_mask & (~intr_status)) != 4'b0) begin
            bus_intr_o  <= 1'b1;
        end else begin
            bus_intr_o  <= 1'b0;
        end
        // remember interrupt signal and clear cleared interrupts
        intr_status <= (intr_status | vid_intr_signal | { 2'b0, blit_intr, 1'b0 } ) & (~intr_clear);
    end
end

endmodule
`default_nettype wire               // restore default
