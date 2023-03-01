#pragma once

#include <unordered_map>

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>
#include <daxa/utils/pipeline_manager.hpp>

#include "../window.hpp"
#include "../utils.hpp"
#include "../gui_state.hpp"
#include "shared/shared.inl"
#include "context.hpp"

#include "tasks/upload_input_data.hpp"
#include "tasks/transmittance_LUT.hpp"
#include "tasks/multiscattering_LUT.hpp"
#include "tasks/skyview_LUT.hpp"
#include "tasks/post_process.hpp"
#include "tasks/imgui_task.hpp"

using namespace daxa::types;

struct Renderer
{
    explicit Renderer(const AppWindow & window);
    ~Renderer();

    void resize();
    void draw();
    void resize_LUT(Images::ID id, i32vec3 new_size);

    private:
        Context context;

        void create_main_tasklist();
        void create_resolution_independent_resources();
        void create_resolution_dependent_resources();
};