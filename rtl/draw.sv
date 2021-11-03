// draw.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Daniel Cliche - https://github.com/dcliche
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

`include "xosera_pkg.sv"

module draw(
    input       logic            oe_i,                 // output enable

    input  wire logic            draw_reg_wr_i,        // strobe to write internal config register number
    input  wire logic  [5:0]     draw_reg_num_i,       // internal config register number
    input  wire logic [15:0]     draw_reg_data_i,      // data for internal config register

    output      logic            draw_vram_sel_o,      // draw VRAM select
    output      logic            draw_wr_o,            // draw VRAM write
    output      logic  [3:0]     draw_mask_o,          // draw VRAM nibble write masks
    output      logic [15:0]     draw_addr_o,          // draw VRAM addr
    output      logic [15:0]     draw_data_out_o,      // draw bus VRAM data write

    output      logic            busy_o,                // is busy?
    
    input  wire logic            reset_i,
    input  wire logic            clk
    );

logic start_line;
logic start_filled_triangle;
logic signed [11:0] x0, y0, x1, y1, x2, y2;
logic        [15:0] dest_addr;
logic        [11:0] dest_width;
logic        [11:0] dest_height;
/* verilator lint_off UNUSED */
logic         [1:0] bpp;
/* verilator lint_on UNUSED */
logic signed [11:0] x, y;
logic signed [11:0] x_line, y_line;
logic signed [11:0] x_filled_triangle, y_filled_triangle;
logic         [7:0] color;
logic drawing;
logic drawing_line;
logic drawing_filled_triangle;
logic busy_line;
logic busy_filled_triangle;
/* verilator lint_off UNUSED */
logic done;
logic done_line;
logic done_filled_triangle;
/* verilator lint_on UNUSED */

logic pipeline_valid[3];
logic signed [11:0] pipeline_x[3];
logic signed [11:0] pipeline_y[3];
logic signed [7:0] pipeline_color[3];

always_comb busy_o = busy_line || busy_filled_triangle || pipeline_valid[0] || pipeline_valid[1] || pipeline_valid[2];

draw_line #(.CORDW(12)) draw_line (    // framebuffer coord width in bits
    .clk(clk),                         // clock
    .reset_i(reset_i),                 // reset
    .start_i(start_line),              // start line rendering
    .oe_i(oe_i),                       // output enable
    .x0_i(x0),                         // point 0 - horizontal position
    .y0_i(y0),                         // point 0 - vertical position
    .x1_i(x1),                         // point 1 - horizontal position
    .y1_i(y1),                         // point 1 - vertical position
    .x_o(x_line),                      // horizontal drawing position
    .y_o(y_line),                      // vertical drawing position
    .drawing_o(drawing_line),          // line is drawing
    .busy_o(busy_line),                // line drawing request in progress
    .done_o(done_line)                 // line complete (high for one tick)
    );

draw_triangle_fill #(.CORDW(12)) draw_triangle_fill (     // framebuffer coord width in bits
    .clk(clk),                              // clock
    .reset_i(reset_i),                      // reset
    .start_i(start_filled_triangle),        // start triangle rendering
    .oe_i(oe_i),                            // output enable
    .x0_i(x0),                              // point 0 - horizontal position
    .y0_i(y0),                              // point 0 - vertical position
    .x1_i(x1),                              // point 1 - horizontal position
    .y1_i(y1),                              // point 1 - vertical position
    .x2_i(x2),                              // point 2 - horizontal position
    .y2_i(y2),                              // point 2 - vertical position
    .x_o(x_filled_triangle),                // horizontal drawing position
    .y_o(y_filled_triangle),                // vertical drawing position
    .drawing_o(drawing_filled_triangle),    // triangle is drawing
    .busy_o(busy_filled_triangle),          // triangle drawing request in progress
    .done_o(done_filled_triangle)           // triangle complete (high for one tick)
    );

always_comb begin
    drawing = drawing_line | drawing_filled_triangle;
    done = done_line | done_filled_triangle;
    if (drawing_line) begin
        x = x_line;
        y = y_line;
    end else begin
        x = x_filled_triangle;
        y = y_filled_triangle;
    end
end

always_ff @(posedge clk) begin

    if (draw_reg_wr_i) begin
        case(draw_reg_num_i[5:0])
            xv::XR_DRAW_COORDX0[5:0]      : x0          <= draw_reg_data_i[11:0];
            xv::XR_DRAW_COORDY0[5:0]      : y0          <= draw_reg_data_i[11:0];
            xv::XR_DRAW_COORDX1[5:0]      : x1          <= draw_reg_data_i[11:0];
            xv::XR_DRAW_COORDY1[5:0]      : y1          <= draw_reg_data_i[11:0];
            xv::XR_DRAW_COORDX2[5:0]      : x2          <= draw_reg_data_i[11:0];
            xv::XR_DRAW_COORDY2[5:0]      : y2          <= draw_reg_data_i[11:0];
            xv::XR_DRAW_COLOR[5:0]        : color       <= draw_reg_data_i[7:0];
            xv::XR_DRAW_EXECUTE[5:0]: begin
                case(draw_reg_data_i[3:0])
                    xv::DRAW_LINE             : start_line             <= 1;
                    xv::DRAW_FILLED_TRIANGLE  : start_filled_triangle  <= 1;
                    default: begin
                        // do nothing
                    end
                endcase
            end
            xv::XR_DRAW_DEST_ADDR[5:0]    : dest_addr   <= draw_reg_data_i[15:0];
            xv::XR_DRAW_DEST_WIDTH[5:0]   : dest_width  <= draw_reg_data_i[11:0];
            xv::XR_DRAW_DEST_HEIGHT[5:0]  : dest_height <= draw_reg_data_i[11:0];
            xv::XR_DRAW_GFX_CTRL[5:0]     : bpp         <= draw_reg_data_i[1:0];
            default: begin
                // Do nothing
            end
        endcase
    end

    if (oe_i) begin
        if (drawing) begin
            pipeline_x[0] <= x;
            pipeline_y[0] <= y;
            pipeline_color[0] <= color;
            pipeline_valid[0] <= 1;
        end else begin
            pipeline_valid[0] <= 0;
        end

        if (pipeline_valid[0] && pipeline_x[0] >= 0 && pipeline_y[0] >= 0) begin
            pipeline_x[1] <= pipeline_x[0];
            pipeline_y[1] <= pipeline_y[0];
            pipeline_color[1] <= pipeline_color[0];
            pipeline_valid[1] <= 1;
        end else begin
            pipeline_valid[1] <= 0;
        end

        if (pipeline_valid[1] && pipeline_x[1] < dest_width && pipeline_y[1] < dest_height) begin
            pipeline_x[2] <= pipeline_x[1];
            pipeline_y[2] <= pipeline_y[1];
            pipeline_color[2] <= pipeline_color[1];
            pipeline_valid[2] <= 1;
        end else begin
            pipeline_valid[2] <= 0;
        end

        if (pipeline_valid[2]) begin
            draw_vram_sel_o <= 1;
            draw_wr_o <= 1;
            draw_mask_o <= (pipeline_x[2] & 12'h001) != 12'h000 ? 4'b0011 : 4'b1100;
            draw_addr_o <= dest_addr + {4'b0, pipeline_y[2]} * ({4'b0, dest_width} / 2) + {4'b0, pipeline_x[2]} / 2;
            draw_data_out_o <= {pipeline_color[2], pipeline_color[2]};
        end else begin
            draw_vram_sel_o <= 0;
            draw_wr_o <= 0;
        end
    end

    if (!busy_o) begin
        draw_vram_sel_o <= 0;
        draw_wr_o <= 0;
    end

    if (start_line) start_line <= 0;
    if (start_filled_triangle) start_filled_triangle <= 0;

    if (reset_i) begin
        start_line <= 0;
        start_filled_triangle <= 0;
        draw_vram_sel_o <= 0;
        draw_wr_o <= 0;
        dest_addr <= 16'h0000;
        dest_width <= xv::VISIBLE_WIDTH / 2;
        dest_height <= xv::VISIBLE_HEIGHT / 2;
        bpp <= xv::BPP_8;
        pipeline_valid[0] <= 0;
        pipeline_valid[1] <= 0;
        pipeline_valid[2] <= 0;
    end
end

endmodule
