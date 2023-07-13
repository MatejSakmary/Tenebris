#pragma once

#include <utility>

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

#include "tasks/bc6h_compress.inl"
#include "tasks/transmittance_LUT.inl"
#include "tasks/multiscattering_LUT.inl"
#include "tasks/skyview_LUT.inl"
#include "tasks/draw_terrain.inl"
#include "tasks/draw_far_sky.inl"
#include "tasks/post_process.inl"
#include "tasks/imgui_task.inl"
#include "tasks/shadowmap.inl"

using namespace daxa::types;

struct Renderer
{
    bool wireframe_terrain;
    // TODO(msakmary) Should be per-fif array
    Globals *globals;
    explicit Renderer(const AppWindow & window);
    ~Renderer();

    void resize();
    void draw(const Camera & camera);
    void upload_planet_geometry(PlanetGeometry const & geometry);

    private:
        Context context;
        std::unique_ptr<TextureManager> manager;

        void initialize_main_tasklist();
        void create_persistent_resources();
        void create_astc_texture();
        void create_bc6h_texture(ManagedTextureHandle handle);
};