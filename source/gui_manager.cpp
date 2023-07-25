#include "gui_manager.hpp"

#include <nlohmann/json.hpp>
#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>
#include <fstream>
#include <format>


GuiManager::GuiManager(GuiManagerInfo const & info) : 
    info{info},
    file_browser{
        ImGuiFileBrowserFlags_NoModal |
        ImGuiFileBrowserFlags_EnterNewFilename |
        ImGuiFileBrowserFlags_CloseOnEsc
    }
{
    file_browser.SetTypeFilters(std::vector<std::string>{".json"});
    file_browser.SetPwd("assets/gui_state");
    curr_path = "assets/gui_state/defaults.json";
    globals.lambda = 1.0f;
    load(curr_path, true);
    globals.use_debug_camera = false;
}

void GuiManager::on_update()
{
    Camera * camera = *info.camera;

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

    ImGui::Begin("General settings");
    auto camera_position = camera->get_camera_position();

    ImGui::InputFloat3("New camera pos: ", reinterpret_cast<f32*>(&new_camera_position));
    if(ImGui::Button("Set Camera Params", {150, 20})) { camera->set_position(new_camera_position);}
    bool use_debug = globals.use_debug_camera == 1;
    ImGui::Checkbox("Use debug camera", &use_debug);
    ImGui::SliderFloat("Lambda", &globals.lambda, 0.0, 1.0);
    globals.use_debug_camera = use_debug ? 1u : 0u;

    if(ImGui::Button("Save", {150, 20})) { save(curr_path); }
    ImGui::SameLine();
    if(ImGui::Button("Save as", {150, 20})) { file_browser.Open(); save_as_active = true; }
    ImGui::SameLine();
    if(ImGui::Button("Load", {150, 20})) { file_browser.Open(); load_active = true; }
    file_browser.Display();

    if(file_browser.HasSelected() && save_as_active)
    {
        save(file_browser.GetSelected().string());
        file_browser.ClearSelected();
        save_as_active = false;
    }

    if(file_browser.HasSelected() && load_active)
    {
        load(file_browser.GetSelected().string(), false);
        file_browser.ClearSelected();
        load_active = false;
    }

    ImGui::Text("Camera position is x: %f y: %f z: %f", camera_position.x, camera_position.y, camera_position.z);
    ImGui::Text("Camera offset it is x: %d y: %d z: %d", camera->offset.x, camera->offset.y, camera->offset.z);
    ImGui::End();

    ImGui::Begin("LUT sizes");
    ImGui::SliderInt2(
        "Transmittance LUT dimensions",
        reinterpret_cast<int*>(&trans_lut_dim), 1, 1024, "%d",
        ImGuiSliderFlags_::ImGuiSliderFlags_AlwaysClamp
    );
    if(ImGui::IsItemDeactivatedAfterEdit()) { globals.trans_lut_dim = trans_lut_dim; info.renderer->resize(); }

    ImGui::SliderInt2(
        "Multiscattering LUT dimensions",
        reinterpret_cast<int*>(&mult_lut_dim), 1, 1024, "%d",
        ImGuiSliderFlags_::ImGuiSliderFlags_AlwaysClamp
    );
    if(ImGui::IsItemDeactivatedAfterEdit()) { globals.mult_lut_dim = mult_lut_dim; info.renderer->resize(); }

    ImGui::SliderInt2(
        "Skyview LUT dimensions",
        reinterpret_cast<int*>(&sky_lut_dim), 1, 1024, "%d",
        ImGuiSliderFlags_::ImGuiSliderFlags_AlwaysClamp
    );
    if(ImGui::IsItemDeactivatedAfterEdit()) { globals.sky_lut_dim = sky_lut_dim; info.renderer->resize(); }
    ImGui::End();

    ImGui::Begin("Terrain params");
    ImGui::SliderFloat2("Terrain Scale", reinterpret_cast<f32*>(&globals.terrain_scale), 1.0f, 1000.0f);
    ImGui::SliderFloat("Terrain midpoint", &globals.terrain_midpoint, 0.0f, 1.0f);
    ImGui::SliderFloat("Terrain height scale", &globals.terrain_height_scale, 0.1f, 100.0f);
    ImGui::SliderFloat("Delta", &globals.terrain_delta, 1.0f, 10.0f);
    ImGui::SliderFloat("Min depth", &globals.terrain_min_depth, 1.0f, 10.0f);
    ImGui::SliderFloat("Max depth", &globals.terrain_max_depth, 1.0f, 1000.0f);
    ImGui::SliderInt("Minimum tesselation level", &globals.terrain_min_tess_level, 1, 20);
    ImGui::SliderInt("Maximum tesselation level", &globals.terrain_max_tess_level, 1, 40);
    if(ImGui::Button("Generate planet", {150, 20})) { info.renderer->upload_planet_geometry(generate_planet()); }
    ImGui::Checkbox("Wireframe terrain", &info.renderer->wireframe_terrain);
    ImGui::End();

    ImGui::Begin("Sun Angle");
    ImGui::SliderFloat("Horizontal angle", &sun_angle.x, 0.0f, 360.0f);
    ImGui::SliderFloat("Vertical angle", &sun_angle.y, 0.0f, 180.0f);

    globals.sun_direction =
    {
        f32(glm::cos(glm::radians(sun_angle.x)) * glm::sin(glm::radians(sun_angle.y))),
        f32(glm::sin(glm::radians(sun_angle.x)) * glm::sin(glm::radians(sun_angle.y))),
        f32(glm::cos(glm::radians(sun_angle.y)))
    };

    ImGui::SliderFloat("Atmosphere bottom", &globals.atmosphere_bottom, 1.0f, 20000.0f);
    globals.atmosphere_top = glm::max(globals.atmosphere_bottom + 10.0f, globals.atmosphere_top);
    ImGui::SliderFloat("Atmosphere top", &globals.atmosphere_top, globals.atmosphere_bottom + 10.0f, 20000.0f);
    ImGui::SliderFloat("mie scale height", &globals.mie_scale_height, 0.1f, 100.0f);
    globals.mie_density[1].exp_scale = -1.0f / globals.mie_scale_height;
    ImGui::SliderFloat("rayleigh scale height", &globals.rayleigh_scale_height, 0.1f, 100.0f);
    globals.rayleigh_density[1].exp_scale = -1.0f / globals.rayleigh_scale_height;
    ImGui::End();

    ImGui::Render();
}

void GuiManager::load(std::string path, bool constructor_load)
{
    auto json = nlohmann::json::parse(std::ifstream(path));

    auto read_val = [&json](auto const name, auto & val)
    {
        val = json[name];
    };

    auto read_vec = [&json](auto const name, auto & val)
    {
        val.x = json[name]["x"];
        val.y = json[name]["y"];
        if constexpr (requires(decltype(val) x){x.z;}) val.z = json[name]["z"];
        if constexpr (requires(decltype(val) x){x.w;}) val.w = json[name]["w"];
    };
    
    auto read_density_profile_layer = [&json](auto const name, auto const layer, DensityProfileLayer & val)
    {
        val.layer_width = json[name][layer]["layer_width"];
        val.exp_term = json[name][layer]["exp_term"];
        val.exp_scale = json[name][layer]["exp_scale"];
        val.lin_term = json[name][layer]["lin_term"];
        val.const_term = json[name][layer]["const_term"];
    };
    
    read_vec("trans_lut_dim", globals.trans_lut_dim);
    read_vec("mult_lut_dim", globals.mult_lut_dim);
    read_vec("sky_lut_dim", globals.sky_lut_dim);
    trans_lut_dim = globals.trans_lut_dim;
    mult_lut_dim = globals.mult_lut_dim;
    sky_lut_dim = globals.sky_lut_dim;

    read_vec("sun_angle", sun_angle);

    read_val("atmosphere_bottom", globals.atmosphere_bottom);
    read_val("atmosphere_top", globals.atmosphere_top);

    // Mie
    read_vec("mie_scattering", globals.mie_scattering);
    read_vec("mie_extinction", globals.mie_extinction);
    read_val("mie_scale_height", globals.mie_scale_height);
    read_val("mie_phase_function_g", globals.mie_phase_function_g);
    read_density_profile_layer("mie_density", 0, globals.mie_density[0]);
    read_density_profile_layer("mie_density", 1, globals.mie_density[1]);

    // Rayleigh
    read_vec("rayleigh_scattering", globals.rayleigh_scattering);
    read_val("rayleigh_scale_height", globals.rayleigh_scale_height);
    read_density_profile_layer("rayleigh_density", 0, globals.rayleigh_density[0]);
    read_density_profile_layer("rayleigh_density", 1, globals.rayleigh_density[1]);

    // Absorption
    read_vec("absorption_extinction", globals.absorption_extinction);
    read_density_profile_layer("absorption_density", 0, globals.absorption_density[0]);
    read_density_profile_layer("absorption_density", 1, globals.absorption_density[1]);

    // Camera
    read_vec("camera_position", globals.camera_position);
    read_vec("camera_offset", globals.offset);

    // Terrain
    read_vec("terrain_scale", globals.terrain_scale);
    read_val("terrain_midpoint", globals.terrain_midpoint);       
    read_val("terrain_height_scale", globals.terrain_height_scale);
    read_val("terrain_delta", globals.terrain_delta);             
    read_val("terrain_min_depth", globals.terrain_min_depth);
    read_val("terrain_max_depth", globals.terrain_max_depth);
    read_val("terrain_min_tess_level", globals.terrain_min_tess_level);
    read_val("terrain_max_tess_level", globals.terrain_max_tess_level);

    globals.sun_direction =
    {
        f32(glm::cos(glm::radians(sun_angle.x)) * glm::sin(glm::radians(sun_angle.y))),
        f32(glm::sin(glm::radians(sun_angle.x)) * glm::sin(glm::radians(sun_angle.y))),
        f32(glm::cos(glm::radians(sun_angle.y)))
    };
    
    (*info.camera)->set_position(globals.camera_position);
    (*info.camera)->offset = globals.offset;
    curr_path = path;
    if(!constructor_load) info.renderer->resize();
}

void GuiManager::save(std::string path)
{
    auto json = nlohmann::json {};
    auto write_val = [&json](auto const name, auto const & val)
    {
        json[name] = val;
    };

    auto write_vec = [&json](auto name, auto const & val)
    {
        json[name]["x"] = val.x;
        json[name]["y"] = val.y;
        if constexpr (requires(decltype(val) x){x.z;}) json[name]["z"] = val.z;
        if constexpr (requires(decltype(val) x){x.w;}) json[name]["w"] = val.w;
    };

    auto write_density_profile_layer = [&json](auto const name, auto const layer, DensityProfileLayer const & val)
    {
        json[name][layer]["layer_width"] = val.layer_width;
        json[name][layer]["exp_term"] = val.exp_term;
        json[name][layer]["exp_scale"] = val.exp_scale;
        json[name][layer]["lin_term"] = val.lin_term;
        json[name][layer]["const_term"] = val.const_term;
    };

    write_val("_version", 1);
    write_vec("trans_lut_dim", globals.trans_lut_dim);
    write_vec("mult_lut_dim", globals.mult_lut_dim);
    write_vec("sky_lut_dim", globals.sky_lut_dim);
    write_vec("sun_angle", sun_angle);

    write_val("atmosphere_bottom", globals.atmosphere_bottom);
    write_val("atmosphere_top", globals.atmosphere_top);

    // Mie
    write_vec("mie_scattering", globals.mie_scattering);
    write_vec("mie_extinction", globals.mie_extinction);
    write_val("mie_scale_height", globals.mie_scale_height);
    write_val("mie_phase_function_g", globals.mie_phase_function_g);
    write_density_profile_layer("mie_density", 0, globals.mie_density[0]);
    write_density_profile_layer("mie_density", 1, globals.mie_density[1]);

    // Rayleigh
    write_vec("rayleigh_scattering", globals.rayleigh_scattering);
    write_val("rayleigh_scale_height", globals.rayleigh_scale_height);
    write_density_profile_layer("rayleigh_density", 0, globals.rayleigh_density[0]);
    write_density_profile_layer("rayleigh_density", 1, globals.rayleigh_density[1]);

    // Absorption
    write_vec("absorption_extinction", globals.absorption_extinction);
    write_density_profile_layer("absorption_density", 0, globals.absorption_density[0]);
    write_density_profile_layer("absorption_density", 1, globals.absorption_density[1]);

    // Camera
    write_vec("camera_position", globals.camera_position);
    write_vec("camera_offset", globals.offset);

    // Terrain
    write_vec("terrain_scale", globals.terrain_scale);
    write_val("terrain_midpoint", globals.terrain_midpoint);       
    write_val("terrain_height_scale", globals.terrain_height_scale);
    write_val("terrain_delta", globals.terrain_delta);             
    write_val("terrain_min_depth", globals.terrain_min_depth);
    write_val("terrain_max_depth", globals.terrain_max_depth);
    write_val("terrain_min_tess_level", globals.terrain_min_tess_level);
    write_val("terrain_max_tess_level", globals.terrain_max_tess_level);
    
    auto f = std::ofstream(path);
    f << std::setw(4) << json;
}