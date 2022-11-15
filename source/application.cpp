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
    if(key == GLFW_KEY_ENTER && action == GLFW_PRESS)
    {
        std::cout << "ENTER pressed" << std::endl;
    }
    return;
}

Application::Application() : 
    window({1080, 720},
    WindowVTable {
        .mouse_pos_callback = std::bind(
            &Application::mouse_callback, this,
            std::placeholders::_1,
            std::placeholders::_2),
        .mouse_button_callback = std::bind(
            &Application::mouse_button_callback, this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3),
        .key_callback = std::bind(
            &Application::key_callback, this,
            std::placeholders::_1,
            std::placeholders::_2,
            std::placeholders::_3,
            std::placeholders::_4),
        .window_resized_callback = std::bind(
            &Application::window_resize_callback, this,
            std::placeholders::_1,
            std::placeholders::_2)
        }
    ),
    state{ .minimized = false },
    renderer{window.get_native_handle()}
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
