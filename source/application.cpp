#include "application.hpp"

#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>
#include "renderer/context.hpp"

void Application::mouse_callback(f64 x, f64 y)
{
    f32 x_offset;
    f32 y_offset;
    if(!state.first_input)
    {
        x_offset = x - state.last_mouse_pos.x;
        y_offset = y - state.last_mouse_pos.y;
    } else {
        x_offset = 0.0f;
        y_offset = 0.0f;
        state.first_input = false;
    }

    if(state.fly_cam)
    {
        state.last_mouse_pos = {f32(x), f32(y)};
        camera.update_front_vector(x_offset, y_offset);
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
        camera.aspect_ratio = f32(width) / f32(height);
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
    renderer{window},
    camera{{
        .position = {0.0, 0.0, 6361.0},
        .front = {0.0, 1.0, 0.0},
        .up = {0.0, 0.0, 1.0}, 
        .aspect_ratio = f32(INIT_WINDOW_DIMENSIONS.x)/f32(INIT_WINDOW_DIMENSIONS.y),
        .fov = glm::radians(70.0f)
    }},
    geometry{generate_planet()}
{
    renderer.upload_planet_geometry(geometry);
    renderer.resize_LUT(Images::TRANSMITTANCE, state.gui_state.lut_dimensions.at(Images::TRANSMITTANCE)); 
    renderer.resize_LUT(Images::MULTISCATTERING, state.gui_state.lut_dimensions.at(Images::MULTISCATTERING)); 
    renderer.resize_LUT(Images::SKYVIEW, state.gui_state.lut_dimensions.at(Images::SKYVIEW)); 
}

void Application::ui_update()
{
    auto image_dimensions_slider = [&](Images::ID id)
    {
        ImGui::SliderInt2(
            std::string(Images::get_image_name(id)).append(" LUT dimensions").c_str(),
            reinterpret_cast<int*>(&state.gui_state.lut_dimensions.at(id)),
            1,
            1024,
            "%d",
            ImGuiSliderFlags_::ImGuiSliderFlags_AlwaysClamp
        );

        if(ImGui::IsItemDeactivatedAfterEdit()) 
        { 
            renderer.resize_LUT(id, state.gui_state.lut_dimensions.at(id));
        }
    };

    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGuiWindowFlags window_flags = 
        ImGuiWindowFlags_NoDocking  | ImGuiWindowFlags_NoBackground |
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse   |
        ImGuiWindowFlags_NoResize   | ImGuiWindowFlags_NoMove       |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBringToFrontOnFocus;
    const ImGuiViewport *viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");

    ImGui::Begin("DockSpace Demo", nullptr, window_flags);
    ImGui::PopStyleVar(3);
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
    ImGui::End();    

    ImGui::Begin("Camera info");
    auto camera_position = camera.get_camera_position();
    ImGui::InputFloat3("New camera pos: ", reinterpret_cast<f32*>(&state.gui_state.new_camera_position));
    if(ImGui::Button("Set Camera Params", {150, 20})) { camera.set_position(state.gui_state.new_camera_position);}

    ImGui::Text("Camera position is x: %f y: %f z: %f", camera_position.x, camera_position.y, camera_position.z);
    ImGui::End();

    ImGui::Begin("LUT sizes");
    image_dimensions_slider(Images::TRANSMITTANCE);
    image_dimensions_slider(Images::MULTISCATTERING);
    image_dimensions_slider(Images::SKYVIEW);
    ImGui::End();

    ImGui::Begin("Sun Angle");
    ImGui::SliderFloat("Horizontal angle", &state.gui_state.sun_angle.x, 0.0f, 360.0f);
    ImGui::SliderFloat("Vertical angle", &state.gui_state.sun_angle.y, 0.0f, 180.0f);
    ImGui::SliderFloat("Atmosphere bottom", &state.gui_state.atmosphere_bottom, 1.0f, 20000.0f);
    state.gui_state.atmosphere_top = glm::max(state.gui_state.atmosphere_bottom + 10.0f, state.gui_state.atmosphere_top);
    ImGui::SliderFloat("Atmosphere top", &state.gui_state.atmosphere_top, state.gui_state.atmosphere_bottom + 10.0f, 20000.0f);
    ImGui::SliderFloat("mie scale height", &state.gui_state.mie_scale_height, 0.1f, 100.0f);
    ImGui::SliderFloat("rayleigh scale height", &state.gui_state.rayleigh_scale_height, 0.1f, 100.0f);
    ImGui::SliderFloat3("Terrain Scale", reinterpret_cast<f32*>(&state.gui_state.terrain_scale), 1.0f, 1000.0f);
    if(ImGui::Button("Generate planet", {150, 20})) { renderer.upload_planet_geometry(generate_planet()); }
    ImGui::End();

    ImGui::Render();
}

void Application::process_input()
{
    f64 this_frame_time = glfwGetTime();
    state.delta_time =  this_frame_time - state.last_frame_time;
    state.last_frame_time = this_frame_time;

    if(state.key_table.data > 0 && state.fly_cam == true)
    {
        if(state.key_table.bits.W)      { camera.move_camera(state.delta_time, Direction::FORWARD);    }
        if(state.key_table.bits.A)      { camera.move_camera(state.delta_time, Direction::LEFT);       }
        if(state.key_table.bits.S)      { camera.move_camera(state.delta_time, Direction::BACK);       }
        if(state.key_table.bits.D)      { camera.move_camera(state.delta_time, Direction::RIGHT);      }
        if(state.key_table.bits.Q)      { camera.move_camera(state.delta_time, Direction::ROLL_LEFT);  }
        if(state.key_table.bits.E)      { camera.move_camera(state.delta_time, Direction::ROLL_RIGHT); }
        if(state.key_table.bits.CTRL)   { camera.move_camera(state.delta_time, Direction::DOWN);       }
        if(state.key_table.bits.SPACE)  { camera.move_camera(state.delta_time, Direction::UP);         }
    }
}

void Application::main_loop()
{
    while (!window.get_window_should_close())
    {
        glfwPollEvents();
        process_input();
        ui_update();

        if (state.minimized) { continue; } 
    
        renderer.update(state.gui_state);
        renderer.draw(camera);
    }
}

Application::~Application()
{
}