#include "application.hpp"

void Application::mouse_callback(f64 x, f64 y)
{

}

void Application::mouse_button_callback(i32 button, i32 action, i32 mods)
{

}

void Application::window_resize_callback(i32 width, i32 height)
{
    state.minimized = (width == 0 || height == 0);
    if(!state.minimized) { renderer.resize(); }
}

void Application::key_callback(i32 key, i32 code, i32 action, i32 mods)
{
}

Application::Application() : 
    window({1080, 720},
    WindowVTable {
        .mouse_pos_callback = [this](const f64 x, const f64 y)
            {this->mouse_callback(x, y);},
        .mouse_button_callback = [this](const i32 button, const i32 action, const i32 mods)
            {this->mouse_button_callback(button, action, mods);},
        .key_callback = [this](const i32 key, const i32 code, const i32 action, const i32 mods)
            {this->key_callback(key, code, action, mods);},
        .window_resized_callback = [this](const i32 width, const i32 height)
            {this->window_resize_callback(width, height);},
    }),
    state{ .minimized = false },
    renderer{window}
{
}

Application::~Application()
{
}

void Application::main_loop()
{
    while (!window.get_window_should_close())
    {
       glfwPollEvents();
       if (state.minimized) { std::cout << "minimized " << std::endl; continue; } 
       renderer.draw();
    }
}
