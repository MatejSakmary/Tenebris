#pragma once

#include <vector>
#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>
#include <daxa/utils/imgui.hpp>
#include <daxa/utils/pipeline_manager.hpp>

#include "shared/shared.inl"

struct Context
{

    struct Images
    {
        daxa::ImageId swapchain_image;
        daxa::ImageId transmittance;
        daxa::ImageId multiscattering;
        daxa::ImageId skyview;
    };

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
        struct Images
        {
            daxa::TaskImageId t_swapchain;
            daxa::TaskImageId t_transmittance;
            daxa::TaskImageId t_multiscattering;
            daxa::TaskImageId t_skyview;
            daxa::TaskImageId t_offscreen;
        } task_images;

        struct Buffers
        {
            daxa::TaskBufferId t_atmosphere_parameters;
        } task_buffers;

        daxa::TaskList task_list;
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
    daxa::ImGuiRenderer imgui_renderer;
};