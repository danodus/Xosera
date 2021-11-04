# Makefile
# vim: set noet ts=8 sw=8

LDFLAGS		:= $(shell sdl2-config --libs) -lm
SDL_CFLAGS	:= $(shell sdl2-config --cflags)

#CFLAGS		:= -Os -std=c99 -Wall -Wextra -Werror $(SDL_CFLAGS)
#CFLAGS		:= -Os -std=c99 $(SDL_CFLAGS) -I../common
CFLAGS		:= -g -std=c99 $(SDL_CFLAGS) -I../common

all: draw_ref_impl

draw_ref_impl: Makefile draw_ref_impl.c ../common/draw.h ../common/cube.h ../common/teapot.h ../common/draw.c ../common/cube.c ../common/teapot.c ../common/sw_rasterizer.c
	$(CC) $(CFLAGS) draw_ref_impl.c ../common/draw.c ../common/cube.c ../common/teapot.c ../common/sw_rasterizer.c -o draw_ref_impl $(LDFLAGS) 

clean:
	rm -f draw_ref_impl

.PHONY: all clean
