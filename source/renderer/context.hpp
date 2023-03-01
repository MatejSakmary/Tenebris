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
            case SWAPCHAIN:         return "Swaphcain";
            case OFFSCREEN:         return "Offscreen";
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
    };

    struct Pipelines
    {
        std::shared_ptr<daxa::ComputePipeline> transmittance;
        std::shared_ptr<daxa::ComputePipeline> multiscattering;
        std::shared_ptr<daxa::ComputePipeline> skyview;
        std::shared_ptr<daxa::RasterPipeline> post_process;
    };

    struct MainTaskList
    {
        struct Buffers
        {
            daxa::TaskBufferId t_atmosphere_parameters;
        } task_buffers;

        std::array<daxa::TaskImageId, Images::IMAGE_COUNT> task_images;
        daxa::TaskList task_list;
    };

    daxa::Context daxa_context;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::PipelineManager pipeline_manager;

    std::array<daxa::ImageId, Images::IMAGE_COUNT> images;
    Buffers buffers;

    MainTaskList main_task_list;
    Pipelines pipelines;

    daxa::SamplerId linear_sampler;
    daxa::ImGuiRenderer imgui_renderer;
};