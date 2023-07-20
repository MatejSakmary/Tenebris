#include "application.hpp"

#include "renderer/context.hpp"

void Application::mouse_callback(f64 x, f64 y)
{
    f32 x_offset;
    f32 y_offset;
    if(!state.first_input)
    {
        x_offset = f32(x) - state.last_mouse_pos.x;
        y_offset = f32(y) - state.last_mouse_pos.y;
    } else {
        x_offset = 0.0f;
        y_offset = 0.0f;
        state.first_input = false;
    }

    if(state.fly_cam)
    {
        state.last_mouse_pos = {f32(x), f32(y)};
        active_camera->update_front_vector(x_offset, y_offset);
    }
}

void Application::mouse_button_callback(i32 button, i32 action, i32 mods)
{

}

void Application::window_resize_callback(i32 width, i32 height)
{
    state.minimized = (width == 0 || height == 0);
    if(!state.minimized)
    {
        renderer.resize(); 
        active_camera->aspect_ratio = f32(width) / f32(height);
    }
}

void Application::key_callback(i32 key, i32 code, i32 action, i32 mods)
{
    if(action == GLFW_PRESS || action == GLFW_RELEASE)
    {
        auto update_state = [](i32 action) -> unsigned int
        {
            if(action == GLFW_PRESS) return 1;
            return 0;
        };

        switch (key)
        {
            case GLFW_KEY_W: state.key_table.bits.W = update_state(action); return;
            case GLFW_KEY_A: state.key_table.bits.A = update_state(action); return;
            case GLFW_KEY_S: state.key_table.bits.S = update_state(action); return;
            case GLFW_KEY_D: state.key_table.bits.D = update_state(action); return;
            case GLFW_KEY_Q: state.key_table.bits.Q = update_state(action); return;
            case GLFW_KEY_E: state.key_table.bits.E = update_state(action); return;
            case GLFW_KEY_SPACE: state.key_table.bits.SPACE = update_state(action); return;
            case GLFW_KEY_LEFT_SHIFT: state.key_table.bits.LEFT_SHIFT = update_state(action); return;
            case GLFW_KEY_LEFT_CONTROL: state.key_table.bits.CTRL = update_state(action); return;
            default: break;
        }
    }

    if(key == GLFW_KEY_F && action == GLFW_PRESS)
    {
        state.fly_cam = !state.fly_cam;
        if(state.fly_cam)
        {
            window.set_input_mode(GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            state.first_input = true;
        } else {
            window.set_input_mode(GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void update_input()
{

}

Application::Application() : 
    window(INIT_WINDOW_DIMENSIONS,
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
    main_camera{{
        .front = {0.0, 1.0, 0.0},
        .up = {0.0, 0.0, 1.0}, 
        .aspect_ratio = f32(INIT_WINDOW_DIMENSIONS.x)/f32(INIT_WINDOW_DIMENSIONS.y),
        .fov = glm::radians(70.0f),
        .near_plane = 10.0f,
    }},
    debug_camera{{
        .position = {5000.0, 5000.0, 200.0},
        .front = {0.0, 1.0, 0.0},
        .up = {0.0, 0.0, 1.0}, 
        .aspect_ratio = f32(INIT_WINDOW_DIMENSIONS.x)/f32(INIT_WINDOW_DIMENSIONS.y),
        .fov = glm::radians(70.0f),
        .near_plane = 10.0f,
    }},
    active_camera{&main_camera},
    gui{{ &active_camera, &renderer }},
    renderer{window, &gui.globals},
    geometry{generate_planet()}
{
    renderer.upload_planet_geometry(geometry);
}

void Application::process_input()
{
    f64 this_frame_time = glfwGetTime();
    state.delta_time =  this_frame_time - state.last_frame_time;
    state.last_frame_time = this_frame_time;

    if(state.key_table.data > 0 && state.fly_cam)
    {
        bool camera_sped_up = state.key_table.bits.LEFT_SHIFT;
        if(state.key_table.bits.W)      { active_camera->move_camera(state.delta_time, Direction::FORWARD, camera_sped_up);    }
        if(state.key_table.bits.A)      { active_camera->move_camera(state.delta_time, Direction::LEFT, camera_sped_up);       }
        if(state.key_table.bits.S)      { active_camera->move_camera(state.delta_time, Direction::BACK, camera_sped_up);       }
        if(state.key_table.bits.D)      { active_camera->move_camera(state.delta_time, Direction::RIGHT, camera_sped_up);      }
        if(state.key_table.bits.Q)      { active_camera->move_camera(state.delta_time, Direction::ROLL_LEFT, camera_sped_up);  }
        if(state.key_table.bits.E)      { active_camera->move_camera(state.delta_time, Direction::ROLL_RIGHT, camera_sped_up); }
        if(state.key_table.bits.CTRL)   { active_camera->move_camera(state.delta_time, Direction::DOWN, camera_sped_up);       }
        if(state.key_table.bits.SPACE)  { active_camera->move_camera(state.delta_time, Direction::UP, camera_sped_up);         }
    }
}

void Application::main_loop()
{
    while (!window.get_window_should_close())
    {
        glfwPollEvents();
        process_input();
        gui.on_update();

        active_camera = gui.globals.use_debug_camera ? &debug_camera : &main_camera;
        if (state.minimized) { continue; } 
    
        renderer.draw({main_camera, debug_camera});
    }
}
