#pragma once

#include <unordered_map>

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>
#include <daxa/utils/pipeline_manager.hpp>

#include "../window.hpp"
#include "../utils.hpp"
#include "../gui_state.hpp"
#include "../camera.hpp"
#include "shared/shared.inl"
#include "context.hpp"
#include "texture_manager.hpp"

#include "../terrain_gen/planet_generator.hpp"

#include "tasks/upload_input_data.hpp"
#include "tasks/transmittance_LUT.hpp"
#include "tasks/multiscattering_LUT.hpp"
#include "tasks/skyview_LUT.hpp"
#include "tasks/draw_terrain.hpp"
#include "tasks/draw_far_sky.inl"
#include "tasks/post_process.hpp"
#include "tasks/imgui_task.hpp"

using namespace daxa::types;

struct Renderer
{
    explicit Renderer(const AppWindow & window);
    ~Renderer();

    void resize();
    void update(const GuiState & state);
    void draw(const Camera & camera);
    void upload_planet_geometry(const PlanetGeometry & geometry);
    void resize_LUT(Images::ID id, i32vec3 new_size);

    // TODO(msakmary) add get mapped pointer function
    void get_staging_memory_pointer();

    private:
        TextureManager manager;
        Context context;

        void create_main_tasklist();
        void create_resolution_independent_resources();
        void create_resolution_dependent_resources();
};