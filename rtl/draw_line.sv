// draw_line.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2021 Daniel Cliche - https://github.com/dcliche
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

// Based on Project F - Lines and Triangles (https://projectf.io/posts/lines-and-triangles/)

`default_nettype none               // mandatory for Verilog sanity
`timescale 1ns/1ps                  // mandatory to shut up Icarus Verilog

module draw_line #(parameter CORDW=10) (                // framebuffer coord width in bits
    input  wire logic clk,                              // clock
    input  wire logic reset_i,                          // reset
    input  wire logic start_i,                          // start line rendering
    input  wire logic oe_i,                             // output enable
    input  wire logic signed [CORDW-1:0] x0_i, y0_i,    // point 0
    input  wire logic signed [CORDW-1:0] x1_i, y1_i,    // point 1
    output      logic signed [CORDW-1:0] x_o, y_o,      // drawing position
    output      logic drawing_o,                        // actively drawing
    output      logic busy_o,                           // drawing request in progress
    output      logic done_o                            // line complete (high for one tick)
    );

    // line properties
    logic right;    // drawing direction;

    // error values
    logic signed [CORDW:0] err;         // a bit wider as signed
    logic signed [CORDW:0] dx, dy;
    logic movx, movy;   // horizontal/vertical move required
    logic is_drawing;
    
    // draw state machine
    enum {IDLE, INIT_0, INIT_1, DRAW_0, DRAW_1, DRAW_2} state;
    always_comb drawing_o = (is_drawing && oe_i);

    always_ff @(posedge clk) begin
        case (state)
            DRAW_0: begin
                if (oe_i) begin
                    if (x_o == x1_i && y_o == y1_i) begin
                        state <= IDLE;
                        is_drawing <= 0;
                        busy_o <= 0;
                        done_o <= 1;
                    end else begin
                        state <= DRAW_1;
                    end
                end
            end
            DRAW_1: begin
                movx <= (2*err >= dy);
                movy <= (2*err <= dx);
                state <= DRAW_2;
            end
            DRAW_2: begin
                if (movx) begin
                    x_o <= right ? x_o + 1 : x_o - 1;
                    err <= err + dy;
                end
                if (movy) begin
                    y_o <= y_o + 1; // always down
                    err <= err + dx;                            
                end
                if (movx && movy) begin
                    x_o <= right ? x_o + 1 : x_o - 1;
                    y_o <= y_o + 1;
                    err <= err + dy + dx;                            
                end
                state <= DRAW_0;
            end
            INIT_0: begin
                state <= INIT_1;
                dx <= right ? x1_i - x0_i : x0_i - x1_i;    // dx = abs(x1_i - x0_i)
                dy <= y0_i - y1_i;                          // dy = y0_i - y1_i
            end
            INIT_1: begin
                state <= DRAW_0;
                is_drawing <= 1;
                err <= dx + dy;
                x_o <= x0_i;
                y_o <= y0_i;
            end
            default: begin  // IDLE
                done_o <= 0;
                if (start_i) begin
                    state <= INIT_0;
                    right <= (x0_i < x1_i);     // draw right to left?
                    busy_o <= 1;
                end
            end
        endcase

        if (reset_i) begin
            state <= IDLE;
            busy_o <= 0;
            done_o <= 0;
            is_drawing <= 0;
        end
    end
endmodule
