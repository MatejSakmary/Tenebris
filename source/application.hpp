#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;

#include "terrain_gen/planet_generator.hpp"
#include "renderer/renderer.hpp"
#include "gui_manager.hpp"
#include "window.hpp"
#include "camera.hpp"

static constexpr i32vec2 INIT_WINDOW_DIMENSIONS = {1920, 1080};

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
        f32vec2 last_mouse_pos;

        KeyTable key_table;
    };

    public:
        Application();
        ~Application() = default;

        void main_loop();

    private:
        AppWindow window;
        AppState state;
        Camera main_camera;
        Camera debug_camera;
        Camera * active_camera;
        GuiManager gui;
        Renderer renderer;
        PlanetGeometry geometry;

        void ui_update();
        void process_input();

        void mouse_callback(f64 x, f64 y);
        void mouse_button_callback(i32 button, i32 action, i32 mods);
        void key_callback(i32 key, i32 code, i32 action, i32 mods);
        void window_resize_callback(i32 width, i32 height);
};
