#pragma once

#include <vector>
#include <array>
#include <string_view>
#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>
#include <daxa/utils/imgui.hpp>
#include <daxa/utils/pipeline_manager.hpp>

#include "shared/shared.inl"

using namespace std::literals;


struct Context
{
    struct Buffers
    {
        daxa::TaskBuffer globals;
        daxa::TaskBuffer terrain_vertices;
        daxa::TaskBuffer terrain_indices;
    };

    struct Images
    {
        daxa::TaskImage swapchain;

        daxa::TaskImage diffuse_map;
        daxa::TaskImage height_map;

    };

    struct Pipelines
    {
        std::shared_ptr<daxa::ComputePipeline> BC6H_compress;
        std::shared_ptr<daxa::ComputePipeline> transmittance;
        std::shared_ptr<daxa::ComputePipeline> multiscattering;
        std::shared_ptr<daxa::ComputePipeline> skyview;
        std::shared_ptr<daxa::RasterPipeline> post_process;
        std::shared_ptr<daxa::RasterPipeline> draw_far_sky;
        std::shared_ptr<daxa::RasterPipeline> draw_terrain_wireframe;
        std::shared_ptr<daxa::RasterPipeline> draw_terrain_solid;
    };

    struct MainTaskList
    {

        enum Conditionals 
        {
            COUNT
        };

        struct TransientBuffers
        {
        };

        struct TransientImages
        {
            daxa::TaskImageHandle transmittance_lut;
            daxa::TaskImageHandle multiscattering_lut;
            daxa::TaskImageHandle skyview_lut;
            daxa::TaskImageHandle offscreen;
            daxa::TaskImageHandle depth;
        };

        std::array<bool, Conditionals::COUNT> conditionals;
        daxa::TaskList task_list;

        TransientImages images;
        TransientBuffers buffers;
    };

    daxa::Context daxa_context;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::PipelineManager pipeline_manager;

    Images images;
    Buffers buffers;

    MainTaskList main_task_list;
    Pipelines pipelines;

    daxa::SamplerId linear_sampler;
    daxa::SamplerId nearest_sampler;
    daxa::ImGuiRenderer imgui_renderer;

    u32 terrain_index_size;
};