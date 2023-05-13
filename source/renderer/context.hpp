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
struct Images
{
    // TODO(msakmary) I don't really like this that much - rethink
    enum ID
    {
        BEGIN = 0,

        TRANSMITTANCE = 0,
        MULTISCATTERING,
        SKYVIEW,

        SWAPCHAIN,
        OFFSCREEN,
        DEPTH,
        END,

        IMAGE_COUNT = END,
        LUT_COUNT = SKYVIEW + 1,
    };

    auto static inline constexpr get_image_name(ID id) -> std::string_view
    {
        switch(id)
        {
            case TRANSMITTANCE:     return "Transmittance";
            case MULTISCATTERING:   return "Multiscattering";
            case SKYVIEW:           return "Skyview";
            case SWAPCHAIN:         return "Swapchain";
            case OFFSCREEN:         return "Offscreen";
            case DEPTH:             return "Depth";
            case END:               throw std::runtime_error("[get_image_name()] Invalid enum");
        }
        return "Invalid image enum";
    };
};


struct Context
{
    struct Buffers
    {
        template<typename T>
        struct SharedBuffer
        {
            T cpu_buffer;
            daxa::BufferId gpu_buffer;
        };

        SharedBuffer<AtmosphereParameters> atmosphere_parameters;
        SharedBuffer<CameraParameters> camera_parameters;
        SharedBuffer<std::vector<TerrainVertex>> terrain_vertices;
        SharedBuffer<std::vector<TerrainIndex>> terrain_indices;
    };

    struct Pipelines
    {
        std::shared_ptr<daxa::ComputePipeline> transmittance;
        std::shared_ptr<daxa::ComputePipeline> multiscattering;
        std::shared_ptr<daxa::ComputePipeline> skyview;
        std::shared_ptr<daxa::RasterPipeline> post_process;
        std::shared_ptr<daxa::RasterPipeline> draw_far_sky;
        std::shared_ptr<daxa::RasterPipeline> draw_terrain;
    };

    struct MainTaskList
    {
        struct Buffers
        {
            daxa::TaskBuffer t_atmosphere_parameters;
            daxa::TaskBuffer t_camera_parameters;
            daxa::TaskBuffer t_terrain_vertices;
            daxa::TaskBuffer t_terrain_indices;
        } task_buffers;

        std::array<daxa::TaskImage, Images::IMAGE_COUNT> task_images;
        daxa::TaskList task_list;
    };

    struct TerrainParams
    {
        f32vec3 scale = {10.0, 10.0, 10.0};
        f32 delta = 1.0;
        f32 min_depth = 1.0;
        f32 max_depth = 10000.0;
        i32 min_tess_level = 1;
        i32 max_tess_level = 10;
    };

    enum Conditionals 
    {
        COPY_PLANET_GEOMETRY = 0,
        COUNT
    };

    daxa::Context daxa_context;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::PipelineManager pipeline_manager;

    std::array<daxa::ImageId, Images::IMAGE_COUNT> images;
    Buffers buffers;

    MainTaskList main_task_list;
    Pipelines pipelines;
    TerrainParams terrain_params;
    std::array<bool, Conditionals::COUNT> conditionals;

    daxa::SamplerId linear_sampler;
    daxa::ImGuiRenderer imgui_renderer;
};