`timescale 1ns / 1ps
`default_nettype none

module top(
    input wire clk,
    input wire reset,
    input logic xosera_cs_n,
    input logic xosera_rd_nwr,
    input logic [3:0] xosera_reg_num,
    input logic [7:0] xosera_data_in,
    output logic [7:0] xosera_data_out,
    output logic xosera_intr,
    input logic xosera_bytesel,
    output logic vga_hsync,
    output logic vga_vsync,
    output logic [3:0] vga_r,
    output logic [3:0] vga_g,
    output logic [3:0] vga_b
    );

   logic clk_pix;
   logic clk_locked;

   assign clk_pix = clk;
   assign clk_locked = 1;

    //
    // VGA output
    //

    xosera_main xosera(
        .clk(clk_pix),
        .bus_cs_n_i(xosera_cs_n),
        .bus_rd_nwr_i(xosera_rd_nwr),
        .bus_reg_num_i(xosera_reg_num),
        .bus_bytesel_i(xosera_bytesel),
        .bus_data_i(xosera_data_in),
        .bus_data_o(xosera_data_out),
        .bus_intr_o(xosera_intr),
        .red_o(vga_r),
        .green_o(vga_g),
        .blue_o(vga_b),
        .hsync_o(vga_hsync),
        .vsync_o(vga_vsync),
        .dv_de_o(),
        .audio_l_o(),
        .audio_r_o(),
        .reconfig_o(),
        .boot_select_o(),
        .reset_i(reset)
    );

    // Print some stuff as an example
    initial begin
        if ($test$plusargs("trace") != 0) begin
            $display("[%0t] Tracing to logs/vlt_dump.vcd...\n", $time);
            $dumpfile("logs/vlt_dump.vcd");
            $dumpvars();
        end
        $display("[%0t] Model running...\n", $time);
    end

endmodule
