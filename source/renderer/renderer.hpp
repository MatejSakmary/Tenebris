#pragma once

#include <utility>

#include <daxa/daxa.hpp>
#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/pipeline_manager.hpp>

#include "../window.hpp"
#include "../utils.hpp"
#include "../camera.hpp"
#include "shared/shared.inl"
#include "context.hpp"
#include "texture_manager.hpp"

#include "../terrain_gen/planet_generator.hpp"

#include "tasks/bc6h_compress.inl"
#include "tasks/height_to_normal.inl"
#include "tasks/transmittance_LUT.inl"
#include "tasks/multiscattering_LUT.inl"
#include "tasks/skyview_LUT.inl"
#include "tasks/draw_terrain.inl"
#include "tasks/deferred_pass.inl"
#include "tasks/post_process.inl"
#include "tasks/imgui_task.inl"
#include "tasks/shadowmap.inl"
#include "tasks/ESM_pass.inl"

using namespace daxa::types;

struct Renderer
{
    bool wireframe_terrain = {};
    Globals *globals;
    explicit Renderer(const AppWindow & window, Globals * globals);
    ~Renderer();

    void resize();
    void draw(Camera & camera);
    void upload_planet_geometry(PlanetGeometry const & geometry);

    private:
        Context context;
        std::unique_ptr<TextureManager> manager;

        void initialize_main_tasklist();
        void create_persistent_resources();
        void load_textures();
};