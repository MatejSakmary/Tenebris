#pragma once

#include <vector>
#include <array>
#include <string_view>
#include <daxa/daxa.hpp>
#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/imgui.hpp>
#include <daxa/utils/pipeline_manager.hpp>

#include "../camera.hpp"

#include "shared/shared.inl"

using namespace std::literals;


struct Context
{
    static constexpr daxa_u32 frames_in_flight = 2;
    struct Buffers
    {
        daxa::TaskBuffer globals;
        daxa::TaskBuffer terrain_vertices;
        daxa::TaskBuffer terrain_indices;
        daxa::TaskBuffer frustum_indices;
        daxa::TaskBuffer average_luminance;
        daxa::TaskBuffer histogram_readback;
    };

    struct Images
    {
        daxa::TaskImage swapchain;

        daxa::TaskImage diffuse_map;
        daxa::TaskImage height_map;
        daxa::TaskImage normal_map;
        daxa::TaskImage tonemapping_lut;

        daxa::TaskImage vsm_page_table;
        daxa::TaskImage vsm_page_height_offset;
        daxa::TaskImage vsm_debug_page_table;
        daxa::TaskImage vsm_memory;
        daxa::TaskImage vsm_meta_memory_table;
        daxa::TaskImage vsm_debug_meta_memory_table;
    };

    struct Pipelines
    {
        std::shared_ptr<daxa::ComputePipeline> transmittance;
        std::shared_ptr<daxa::ComputePipeline> multiscattering;
        std::shared_ptr<daxa::ComputePipeline> skyview;
        std::shared_ptr<daxa::ComputePipeline> first_esm_pass;
        std::shared_ptr<daxa::ComputePipeline> second_esm_pass;
        std::shared_ptr<daxa::ComputePipeline> analyze_depthbuffer_first_pass;
        std::shared_ptr<daxa::ComputePipeline> analyze_depthbuffer_subsequent_pass;
        std::shared_ptr<daxa::ComputePipeline> prepare_shadow_matrices;
        std::shared_ptr<daxa::ComputePipeline> luminance_histogram;
        std::shared_ptr<daxa::ComputePipeline> adapt_average_luminance;
        std::shared_ptr<daxa::ComputePipeline> vsm_free_wrapped_pages;
        std::shared_ptr<daxa::ComputePipeline> vsm_find_free_pages;
        std::shared_ptr<daxa::ComputePipeline> vsm_allocate_pages;
        std::shared_ptr<daxa::ComputePipeline> vsm_clear_pages;
        std::shared_ptr<daxa::RasterPipeline> vsm_draw_pages;
        std::shared_ptr<daxa::ComputePipeline> vsm_clear_dirty_bit;
        std::shared_ptr<daxa::ComputePipeline> vsm_debug_page_table;
        std::shared_ptr<daxa::ComputePipeline> vsm_debug_meta_memory_table;

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
            USE_DEBUG_CAMERA,
            COUNT
        };

        struct TransientBuffers
        {
            daxa::TaskBufferView depth_limits;
            daxa::TaskBufferView shadowmap_data;
            daxa::TaskBufferView frustum_vertices;
            daxa::TaskBufferView frustum_colors;
            daxa::TaskBufferView frustum_indirect;
            daxa::TaskBufferView luminance_histogram;

            daxa::TaskBufferView vsm_allocation_count;
            daxa::TaskBufferView vsm_allocation_requests;

            daxa::TaskBufferView vsm_allocate_indirect;
            daxa::TaskBufferView vsm_clear_indirect;
            daxa::TaskBufferView vsm_clear_dirty_bit_indirect;

            daxa::TaskBufferView vsm_free_wrapped_pages_info;
            daxa::TaskBufferView vsm_free_page_buffer;
            daxa::TaskBufferView vsm_not_visited_page_buffer;
            daxa::TaskBufferView vsm_find_free_pages_header;
            daxa::TaskBufferView vsm_sun_projection_matrices;
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

            daxa::TaskImageView shadowmap_cascades;
            daxa::TaskImageView esm_tmp_cascades;
            daxa::TaskImageView esm_cascades;

            daxa::TaskImageView vsm_debug_image;
        };

        std::array<bool, Conditionals::COUNT> conditionals;
        daxa::TaskGraph task_list;

        TransientImages images;
        TransientBuffers buffers;
    };

    Camera sun_camera;

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
    // linear min, linear max, clamp edge
    daxa::SamplerId llce_sampler;
    daxa::ImGuiRenderer imgui_renderer;

    daxa_u32 terrain_index_size;

    daxa_u32 debug_frustum_cpu_count;
    std::array<Histogram, HISTOGRAM_BIN_COUNT> cpu_histogram;
    std::array<FrustumVertex, 8 * MAX_FRUSTUM_COUNT> frustum_vertices;
    std::array<FrustumColor, MAX_FRUSTUM_COUNT> frustum_colors;
    std::array<VSMClipProjection, VSM_CLIP_LEVELS> vsm_sun_projections;
    std::array<FreeWrappedPagesInfo, VSM_CLIP_LEVELS> vsm_free_wrapped_pages_info;
    std::array<daxa_i32vec2, VSM_CLIP_LEVELS> vsm_last_frame_offset;
};

using MainConditionals = Context::MainTaskList::Conditionals;