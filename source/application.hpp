#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;

#include "renderer/renderer.hpp"
#include "gui_state.hpp"
#include "window.hpp"

union KeyTable
{
    unsigned int data;
    struct
    {
        unsigned int W : 1 = 0;
        unsigned int A : 1 = 0;
        unsigned int S : 1 = 0;
        unsigned int D : 1 = 0;
        unsigned int Q : 1 = 0;
        unsigned int E : 1 = 0;
        unsigned int CTRL : 1 = 0;
        unsigned int SPACE : 1 = 0;
        unsigned int LEFT_SHIFT : 1 = 0;
    } bits;
};
struct Application 
{
    struct AppState
    {
        f64 last_frame_time = 0.0;
        f64 delta_time = 0.0;

        bool minimized = false;
        bool fly_cam = false;
        bool first_input = true;

        KeyTable key_table;
        GuiState gui_state;
    };

    public:
        Application();
        ~Application();

        void main_loop();

    private:
        AppWindow window;
        AppState state;
        Renderer renderer;

        void ui_update();
        void process_input();

        void init_window();
        void mouse_callback(f64 x, f64 y);
        void mouse_button_callback(i32 button, i32 action, i32 mods);
        void key_callback(i32 key, i32 code, i32 action, i32 mods);
        void window_resize_callback(i32 width, i32 height);
};
