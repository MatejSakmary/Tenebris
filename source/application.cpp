#include "application.hpp"

void Application::mouse_callback(f64 x, f64 y)
{
    float xoffset;
    float yoffset;
    if(!app_state.first_input)
    {
        xoffset = app_state.mouse_last_pos.x - x;
        yoffset = app_state.mouse_last_pos.y - y;
    }else{
        xoffset = 0.0f;
        yoffset = 0.0f;
        app_state.first_input = false;
    }

    if(app_state.fly_mode)
    {
        app_state.mouse_last_pos.x = x;
        app_state.mouse_last_pos.y = y;
        // data->renderer->camera->updateFrontVec(xoffset, yoffset);
    }
}

void Application::window_resize_callback(i32 width, i32 height)
{
    // data->renderer->framebufferResized = true;
}

void Application::process_input()
{
    float current_frame = glfwGetTime();
    app_state.delta_time = current_frame - app_state.last_frame;
    app_state.last_frame = current_frame;
    app_state.fly_mode_toggle_timeout = app_state.fly_mode_toggle_timeout - app_state.delta_time < 0.0f ? 
        0.0f : app_state.fly_mode_toggle_timeout - app_state.delta_time;

    /* end program */
    if (window.get_key_state(GLFW_KEY_ESCAPE) == GLFW_PRESS){
        window.set_window_close();
    }
    if (window.get_key_state(GLFW_KEY_W) == GLFW_PRESS) {
        // data->renderer->camera->forward(data->deltaTime);
    }
    if (window.get_key_state(GLFW_KEY_S) == GLFW_PRESS) {
        // data->renderer->camera->back(data->deltaTime);
    }
    if (window.get_key_state(GLFW_KEY_A) == GLFW_PRESS) {
        // data->renderer->camera->left(data->deltaTime);
    }
    if (window.get_key_state(GLFW_KEY_D) == GLFW_PRESS) {
        // data->renderer-> camera->right(data->deltaTime);
    }
    if (window.get_key_state(GLFW_KEY_SPACE) == GLFW_PRESS && app_state.fly_mode) {
        // data->renderer->camera->up(data->deltaTime);
    }
    if (window.get_key_state(GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS && app_state.fly_mode) {
        // data->renderer->camera->down(data->deltaTime);
    }
    if (window.get_key_state(GLFW_KEY_F) == GLFW_PRESS && app_state.fly_mode_toggle_timeout == 0.0f) {
        app_state.fly_mode = !app_state.fly_mode;
        if(app_state.fly_mode)
        {
            window.set_input_mode(GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            app_state.first_input = true;
        }else{
            window.set_input_mode(GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
        app_state.fly_mode_toggle_timeout = 0.2f;
    }
}

Application::Application() : 
    window(1920, 1080,
           std::bind(&Application::mouse_callback, this, std::placeholders::_1, std::placeholders::_2),
           std::bind(&Application::window_resize_callback,this, std::placeholders::_1, std::placeholders::_2)
           ),
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
       process_input();
       renderer.draw();
    }

    /*  Wait for logical device to finish operations ->
     *  Because drawFrame operations are asynchornous when we exit the loop
     *  drawing and presentation operations may still be going on*/
    // vkDeviceWaitIdle(renderer->vDevice->device);
}
