#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;

#include "renderer/renderer.hpp"
#include "window.hpp"

struct Application 
{
    struct AppState
    {
        b32 minimized;
    };

    public:
        Application();
        ~Application();

        void main_loop();

    private:
        AppWindow window;
        AppState state;
        Renderer renderer;

        void init_window();
        void mouse_callback(f64 x, f64 y);
        void mouse_button_callback(i32 button, i32 action, i32 mods);
        void key_callback(i32 key, i32 code, i32 action, i32 mods);
        void window_resize_callback(i32 width, i32 height);
};
