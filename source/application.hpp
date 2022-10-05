#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;

#include "renderer.hpp"
#include "window.hpp"

struct Application 
{

    public:
        Application();
        ~Application();

        void main_loop();

    private:

        struct AppState
        {
            f64 last_frame = 0.0f;
            f64vec2 mouse_last_pos = {0.0, 0.0};
            f64 delta_time = 0.0f;
            bool fly_mode = false;
            bool first_input = false;
            f32 fly_mode_toggle_timeout = 0.0f;
        };

        AppWindow window;
        AppState app_state;
        Renderer renderer;

        void init_window();
        void mouse_callback(f64 x, f64 y);
        void window_resize_callback(i32 width, i32 height);
        void process_input();
};
