#pragma once

#include <array>
#include <daxa/types.hpp>
#include "camera.hpp"
#include "renderer/renderer.hpp"
#include "renderer/context.hpp"
#include "../external/imgui_custom/imgui_file_dialog.hpp"
using namespace daxa::types;

struct GuiManagerInfo
{
    Camera * camera;
    Renderer * renderer;
};

struct GuiManager
{
    GuiManager(GuiManagerInfo const & info);
    void on_update();

    Globals globals;

    private:
        void load(std::string path, bool constructor_load);
        void save(std::string path);

        ImGui::FileBrowser file_browser;

        bool save_as_active = {};
        bool load_active = {};

        GuiManagerInfo info;
        std::string curr_path;

        f32vec2 sun_angle;
        u32vec2 trans_lut_dim;
        u32vec2 mult_lut_dim;
        u32vec2 sky_lut_dim;
        f32vec3 new_camera_position{0.0f, 0.0f, 0.0f};
};