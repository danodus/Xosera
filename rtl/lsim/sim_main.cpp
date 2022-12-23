#include <SDL.h>

#include <memory>
#include <chrono>
#include <deque>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>

#include <verilated.h>
#include <iostream>

// Include model header, generated from Verilating "top.v"
#include "Vtop.h"

// include Lua headers
extern "C" {
    #include "lua.h"
    #include "lualib.h"
    #include "lauxlib.h"
}

#include "../../xosera_m68k_api/xosera_m68k_defs.h"

typedef enum {
    SET_XOSERA_REG_NUM,
    SET_XOSERA_BYTESEL,
    SET_XOSERA_DATA_IN,
    SET_XOSERA_CS_N,
    SET_XOSERA_RD_NWR,
    SEND_DATA_VALUE,
    PULSE_CLK
} commands_t;


typedef struct {
    commands_t command;
    uint16_t value;
} command_t;

std::mutex commands_m;
std::deque<command_t> commands;

uint16_t data_value;
std::mutex data_m;
std::condition_variable data_cv;
bool data_ready = false;
bool data_processed = false;

// 640x480
const int vga_width = 800;
const int vga_height = 525;

// 848x480
//const int vga_width = 1088;
//const int vga_height = 517;

const int screen_width = vga_width;
const int screen_height = vga_height;

VerilatedContext *contextp = nullptr;
Vtop *top = nullptr;

double sc_time_stamp()
{
    return 0.0;
}

void xosera_write(uint8_t reg, uint16_t value)
{
    commands_m.lock();
    commands.emplace_back(command_t{SET_XOSERA_REG_NUM, reg});

    commands.emplace_back(command_t{SET_XOSERA_BYTESEL, 0}); // even
    commands.emplace_back(command_t{SET_XOSERA_DATA_IN, static_cast<uint16_t>(value >> 8)}); // even

    commands.emplace_back(command_t{SET_XOSERA_CS_N, 0});
    commands.emplace_back(command_t{PULSE_CLK, 0});

    commands.emplace_back(command_t{SET_XOSERA_RD_NWR, 0}); // write
    commands.emplace_back(command_t{PULSE_CLK, 0});

    commands.emplace_back(command_t{SET_XOSERA_CS_N, 1});
    commands.emplace_back(command_t{SET_XOSERA_RD_NWR, 1}); // read
    commands.emplace_back(command_t{PULSE_CLK, 0});

    commands.emplace_back(command_t{SET_XOSERA_BYTESEL, 1}); // odd
    commands.emplace_back(command_t{SET_XOSERA_DATA_IN, static_cast<uint16_t>(value & 0xff)}); // even

    commands.emplace_back(command_t{SET_XOSERA_CS_N, 0});
    commands.emplace_back(command_t{PULSE_CLK, 0});

    commands.emplace_back(command_t{SET_XOSERA_RD_NWR, 0}); // write
    commands.emplace_back(command_t{PULSE_CLK, 0});

    commands.emplace_back(command_t{SET_XOSERA_CS_N, 1});
    commands.emplace_back(command_t{SET_XOSERA_RD_NWR, 1}); // read
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands_m.unlock();
}

uint16_t xosera_read(uint8_t reg)
{
    uint16_t value;
    bool fetched;

    // Request MSB
    
    commands_m.lock();
    commands.emplace_back(command_t{SET_XOSERA_REG_NUM, reg});
    commands.emplace_back(command_t{SET_XOSERA_BYTESEL, 0}); // even
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands.emplace_back(command_t{SET_XOSERA_CS_N, 0});
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands.emplace_back(command_t{SEND_DATA_VALUE, 0});
    commands_m.unlock();

    {
        // Wait until main() sends data
        std::unique_lock<std::mutex> lk(data_m);
        data_cv.wait(lk, []{return data_ready;});
        data_ready = false;

        // after the wait, we own the lock.
        value = data_value << 8;
    
        // Send data back to main()
        data_processed = true;
    
        // Manual unlocking is done before notifying, to avoid waking up
        // the waiting thread only to block again (see notify_one for details)
        lk.unlock();
        data_cv.notify_one();        
    }

    commands_m.lock();
    commands.emplace_back(command_t{SET_XOSERA_CS_N, 1});
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands_m.unlock();

    // Request LSB

    commands_m.lock();
    commands.emplace_back(command_t{SET_XOSERA_REG_NUM, reg});
    commands.emplace_back(command_t{SET_XOSERA_BYTESEL, 1}); // odd
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands.emplace_back(command_t{SET_XOSERA_CS_N, 0});
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands.emplace_back(command_t{SEND_DATA_VALUE, 0});
    commands_m.unlock();

    {
        // Wait until main() sends data
        std::unique_lock<std::mutex> lk(data_m);
        data_cv.wait(lk, []{return data_ready;});
        data_ready = false;

        // after the wait, we own the lock.
        value |= data_value & 0xff;
    
        // Send data back to main()
        data_processed = true;
    
        // Manual unlocking is done before notifying, to avoid waking up
        // the waiting thread only to block again (see notify_one for details)
        lk.unlock();
        data_cv.notify_one();        
    }

    commands_m.lock();
    commands.emplace_back(command_t{SET_XOSERA_CS_N, 1});
    commands.emplace_back(command_t{PULSE_CLK, 0});
    commands_m.unlock();

    return value;
}


void xm_setw(uint16_t reg, uint16_t value)
{
    xosera_write(reg, value);
}

uint16_t xm_getw(uint16_t reg)
{
    return xosera_read(reg);
}

void xreg_setw(uint16_t reg, uint16_t value)
{
    xosera_write(XM_WR_XADDR >> 2, reg);
    xosera_write(XM_XDATA >> 2, value);
}

static int lua_xm_setw(lua_State* L)
{
    uint16_t reg = luaL_checkinteger(L, 1);
    uint16_t value = luaL_checkinteger(L, 2);
    xm_setw(reg, value);
    return 0;
}

static int lua_xm_getw(lua_State* L)
{
    uint16_t reg = luaL_checkinteger(L, 1);
    uint16_t value = xm_getw(reg);
    lua_pushinteger(L, value);
    return 1;
}

static int lua_xreg_setw(lua_State* L)
{
    uint16_t reg = luaL_checkinteger(L, 1);
    uint16_t value = luaL_checkinteger(L, 2);
    xreg_setw(reg, value);
    return 0;
}

void run_script()
{
    lua_State* L = luaL_newstate(); // create a new lua instance
    luaL_openlibs(L); // give lua access to basic libraries
    lua_register(L, "xm_setw", lua_xm_setw);
    lua_register(L, "xm_getw", lua_xm_getw);
    lua_register(L, "xreg_setw", lua_xreg_setw);
    int ret = luaL_dofile(L, "scripts/test1.lua"); // loads the Lua script
    if (ret) {
        std::cout << "Lua script error\n";
        return;
    }
}

int main(int argc, char **argv, char **env)
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow(
        "Lua Based Xosera Simulation",
        SDL_WINDOWPOS_UNDEFINED_DISPLAY(1),
        SDL_WINDOWPOS_UNDEFINED,
        screen_width,
        screen_height,
        0);

    SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    // Create logs/ directory in case we have traces to put under it
    Verilated::mkdir("logs");


    const size_t pixels_size = vga_width * vga_height * 4;
    unsigned char *pixels = new unsigned char[pixels_size];

    SDL_Texture *texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, vga_width, vga_height);


    bool restart_model;
    do {


        // Construct a VerilatedContext to hold simulation time, etc.
        // Multiple modules (made later below with Vtop) may share the same
        // context to share time, or modules may have different contexts if
        // they should be independent from each other.

        // Using unique_ptr is similar to
        // "VerilatedContext* contextp = new VerilatedContext" then deleting at end.
        contextp = new VerilatedContext;

        // Set debug level, 0 is off, 9 is highest presently used
        // May be overridden by commandArgs argument parsing
        contextp->debug(0);

        // Randomization reset policy
        // May be overridden by commandArgs argument parsing
        contextp->randReset(0);

        // Verilator must compute traced signals
        contextp->traceEverOn(false);

        // Pass arguments so Verilated code can see them, e.g. $value$plusargs
        // This needs to be called before you create any model
        contextp->commandArgs(argc, argv);

        restart_model = false;

        // Construct the Verilated model, from Vtop.h generated from Verilating "top.v".
        // Using unique_ptr is similar to "Vtop* top = new Vtop" then deleting at end.
        // "TOP" will be the hierarchical name of the module.
        top = new Vtop{contextp, "TOP"};

        // Set Vtop's input signals
        top->reset = 1;
        top->clk = 0;
        top->xosera_rd_nwr = 1;
        top->xosera_cs_n = 1;

        SDL_Event e;
        bool quit = false;

        auto tp_frame = std::chrono::high_resolution_clock::now();
        auto tp_clk = std::chrono::high_resolution_clock::now();
        auto tp_now = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration_clk;

        unsigned int frame_counter = 0;
        bool was_vsync = false;

        std::thread *lua_thread = nullptr;

        size_t pixel_index = 0;

        top->clk = 0;

        while (!contextp->gotFinish() && !quit)
        {
            top->clk = !top->clk;

            contextp->timeInc(1);
            top->eval();

            // if posedge clk
            if (top->clk) {
                
                if (contextp->time() > 1 && contextp->time() < 10)
                {
                    top->reset = 1; // Assert reset
                }
                else
                {
                    top->reset = 0; // Deassert reset
                }

                // Process commands
                if (!top->reset)
                {
                    commands_m.lock();
                    bool stop = false;
                    while (!commands.empty() && !stop) {
                        command_t cmd = commands.front();
                        commands.pop_front();

                        switch (cmd.command) {
                            case SET_XOSERA_REG_NUM:
                                //printf("SET_XOSERA_REG_NUM: %x\n", cmd.value);
                                top->xosera_reg_num = cmd.value;
                                break;
                            case SET_XOSERA_BYTESEL:
                                //printf("SET_XOSERA_BYTESEL: %x\n", cmd.value);
                                top->xosera_bytesel = cmd.value;
                                break;
                            case SET_XOSERA_DATA_IN:
                                //printf("SET_XOSERA_DATA_IN: %x\n", cmd.value);
                                top->xosera_data_in = cmd.value;
                                break;
                            case SET_XOSERA_CS_N:
                                //printf("SET_XOSERA_CS_N: %x\n", cmd.value);
                                top->xosera_cs_n = cmd.value;
                                break;
                            case SET_XOSERA_RD_NWR:
                                //printf("SET_XOSERA_RD_NWR: %x\n", cmd.value);
                                top->xosera_rd_nwr = cmd.value;
                                break;
                            case SEND_DATA_VALUE:
                                {
                                    data_value = top->xosera_data_out;
                                    //printf("SEND_DATA_VALUE: %x\n", data_value);
                                    // send data to the worker thread
                                    {
                                        std::lock_guard<std::mutex> lk(data_m);
                                        data_ready = true;
                                    }
                                    data_cv.notify_one();
                                
                                    // wait for the worker
                                    {
                                        std::unique_lock<std::mutex> lk(data_m);
                                        data_cv.wait(lk, []{return data_processed;});
                                        data_processed = false;
                                    }
                                }
                                break;
                            case PULSE_CLK:
                                //printf("PULSE_CLK\n");
                                stop = true;
                        }
                    }
                    commands_m.unlock();
                }

                // Update video display
                if (was_vsync && top->vga_vsync)
                {
                    pixel_index = 0;
                    was_vsync = false;
                }

                pixels[pixel_index] = top->vga_r << 4;
                pixels[pixel_index + 1] = top->vga_g << 4;
                pixels[pixel_index + 2] = top->vga_b << 4;
                pixels[pixel_index + 3] = 255;
                pixel_index = (pixel_index + 4) % (pixels_size);

                if (!top->vga_vsync && !was_vsync)
                {
                    was_vsync = true;
                    void *p;
                    int pitch;
                    SDL_LockTexture(texture, NULL, &p, &pitch);
                    assert(pitch == vga_width * 4);
                    memcpy(p, pixels, vga_width * vga_height * 4);
                    SDL_UnlockTexture(texture);
                }
            }

            tp_now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> duration_frame = tp_now - tp_frame;

            if (contextp->time() % 2000000 == 0)
            {
                duration_clk = tp_now - tp_clk;
                tp_clk = tp_now;
            }

            bool commands_empty;
            commands_m.lock();
            commands_empty = commands.empty();
            commands_m.unlock();

            if (!top->reset && commands_empty && duration_frame.count() >= 1.0 / 60.0)
            {
                while (SDL_PollEvent(&e))
                {
                    if (e.type == SDL_QUIT)
                    {
                        quit = true;
                    }
                    else if (e.type == SDL_KEYUP)
                    {
                        switch (e.key.keysym.sym)
                        {
                        case SDLK_F12:
                            quit = true;
                            restart_model = true;
                            break;
                        }
                    }
                    else if (e.type == SDL_KEYDOWN)
                    {
                        if (e.key.repeat == 0)
                        {
                            switch (e.key.keysym.sym)
                            {
                            case SDLK_F5:
                                if (lua_thread) {
                                    lua_thread->join();
                                    lua_thread = nullptr;
                                }
                                if (!lua_thread)
                                    lua_thread = new std::thread(run_script);
                                break;
                            case SDLK_F12:
                                std::cout << "Reset context\n";
                                break;
                            }
                        }
                    }
                }

                int draw_w, draw_h;
                SDL_GL_GetDrawableSize(window, &draw_w, &draw_h);

                int scale_x, scale_y;
                scale_x = draw_w / screen_width;
                scale_y = draw_h / screen_height;

                SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
                SDL_RenderClear(renderer);

                if (frame_counter % 100 == 0)
                {
                    std::cout << "Clk speed: " << 1.0 / (duration_clk.count()) << " MHz\n";
                }

                tp_frame = tp_now;
                frame_counter++;

                SDL_RenderCopy(renderer, texture, NULL, NULL);

                SDL_RenderPresent(renderer);
            }

        }

        if (lua_thread) {
            lua_thread->join();
            lua_thread = nullptr;
        }

        // Final model cleanup
        top->final();
    } while (restart_model);

    delete top;
    delete contextp;

    SDL_DestroyTexture(texture);

    delete[] pixels;

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
