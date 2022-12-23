VERILATOR = verilator

LDFLAGS := -LDFLAGS "$(shell sdl2-config --libs)"
CFLAGS := -CFLAGS "$(shell sdl2-config --cflags)"

SVSRC := lsim/top.sv $(wildcard *.sv)

SRC := lsim/sim_main.cpp $(wildcard lsim/*.c)

all: vlsim

vlsim: lsim/obj_dir/Vtop.mk
	@echo Completed building Verilator simulation, use \"make vlrun\" to run.

clean:
	rm -rf lsim/obj_dir

lsim/obj_dir/Vtop.mk: $(SVSRC) $(SRC)
	$(VERILATOR) --top-module top -cc --exe $(CFLAGS) $(LDFLAGS) $(SVSRC) $(notdir $(SRC)) -I./lsim -Mdir lsim/obj_dir -Wno-PINMISSING -Wno-WIDTH -Wno-CASEINCOMPLETE -Wno-TIMESCALEMOD -Wno-NULLPORT
	cd lsim/obj_dir && $(MAKE) -j 4 -f Vtop.mk

vlrun: lsim/obj_dir/Vtop.mk
	./lsim/obj_dir/Vtop

.PHONY: all clean vlsim vlrun
