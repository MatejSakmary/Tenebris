#pragma once

#include <vector>
#include <array>
#include <string_view>
#include <daxa/daxa.hpp>
#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/imgui.hpp>
#include <daxa/utils/pipeline_manager.hpp>

#include "shared/shared.inl"

using namespace std::literals;


struct Context
{
    static constexpr u32 frames_in_flight = 2;
    struct Buffers
    {
        daxa::TaskBuffer globals;
        daxa::TaskBuffer terrain_vertices;
        daxa::TaskBuffer terrain_indices;
        daxa::TaskBuffer frustum_indices;
    };

    struct Images
    {
        daxa::TaskImage swapchain;

        daxa::TaskImage diffuse_map;
        daxa::TaskImage height_map;
        daxa::TaskImage normal_map;
    };

    struct Pipelines
    {
        std::shared_ptr<daxa::ComputePipeline> BC6H_compress;
        std::shared_ptr<daxa::ComputePipeline> height_to_normal;
        std::shared_ptr<daxa::ComputePipeline> transmittance;
        std::shared_ptr<daxa::ComputePipeline> multiscattering;
        std::shared_ptr<daxa::ComputePipeline> skyview;
        std::shared_ptr<daxa::ComputePipeline> esm_pass;
        std::shared_ptr<daxa::ComputePipeline> analyze_depthbuffer_first_pass;
        std::shared_ptr<daxa::ComputePipeline> analyze_depthbuffer_subsequent_pass;
        std::shared_ptr<daxa::ComputePipeline> prepare_shadow_matrices;

        std::shared_ptr<daxa::RasterPipeline> post_process;
        std::shared_ptr<daxa::RasterPipeline> deferred_pass;
        std::shared_ptr<daxa::RasterPipeline> debug_draw_frustum;
        std::shared_ptr<daxa::RasterPipeline> draw_terrain_wireframe;
        std::shared_ptr<daxa::RasterPipeline> draw_terrain_solid;
        std::shared_ptr<daxa::RasterPipeline> draw_terrain_shadowmap;
    };

    struct MainTaskList
    {

        enum Conditionals 
        {
            COUNT
        };

        struct TransientBuffers
        {
            daxa::TaskBufferView depth_limits;
            daxa::TaskBufferView shadowmap_data;
            daxa::TaskBufferView frustum_vertices;
        };

        struct TransientImages
        {
            daxa::TaskImageView transmittance_lut;
            daxa::TaskImageView multiscattering_lut;
            daxa::TaskImageView skyview_lut;

            // GBuffer
            daxa::TaskImageView g_albedo;
            daxa::TaskImageView g_normals;

            daxa::TaskImageView offscreen;
            daxa::TaskImageView depth;

            daxa::TaskImageView shadowmap;
            daxa::TaskImageView esm;
        };

        std::array<bool, Conditionals::COUNT> conditionals;
        daxa::TaskGraph task_list;

        TransientImages images;
        TransientBuffers buffers;
    };

    daxa::Instance daxa_instance;
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