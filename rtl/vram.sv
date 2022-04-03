// vram.sv
//
// vim: set et ts=4 sw=4
//
// Copyright (c) 2020 Xark - https://hackaday.io/Xark
//
// See top-level LICENSE file for license information. (Hint: MIT)
//

`default_nettype none             // mandatory for Verilog sanity
`timescale 1ns/1ps

`include "xosera_pkg.sv"

module vram(
           input  wire logic        clk,
           input  wire logic        sel,
           input  wire logic        wr_en,
           input  wire logic  [3:0] wr_mask,
           input  wire addr_t       address_in,
           input  wire word_t       data_in,
           output      word_t       data_out
       );


word_t memory[0: 65535] /* verilator public*/;

// synchronous write (keeps memory updated for easy simulator access)
always_ff @(posedge clk) begin
    if (sel) begin
        if (wr_en) begin
            if (wr_mask[0]) memory[address_in][ 3: 0]   <= data_in[ 3: 0];
            if (wr_mask[1]) memory[address_in][ 7: 4]   <= data_in[ 7: 4];
            if (wr_mask[2]) memory[address_in][11: 8]   <= data_in[11: 8];
            if (wr_mask[3]) memory[address_in][15:12]   <= data_in[15:12];
        end
        data_out <= memory[address_in];
    end
end

endmodule
