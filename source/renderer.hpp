#pragma once

#include <unordered_map>

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

#include "shared/shared.inl"

using namespace daxa::types;

struct Renderer
{
    Renderer(daxa::NativeWindowHandle window_handle);
    ~Renderer();

    void resize();
    void draw();

    private:

        struct Tasks
        {
            daxa::TaskList clear_present;
            daxa::TaskList render_sky;
        };

        struct Buffers
        {
            template<typename T>
            struct ApplicationBuffer
            {
                T cpu_buffer;
                daxa::BufferId gpu_buffer;
            };

            ApplicationBuffer<AtmosphereParameters> atmosphere_parameters;
        };
        
        struct TaskBuffers
        {
            daxa::TaskBufferId atmosphere_parameters;
        };

        struct Images
        {
            daxa::ImageId swapchain_image;
            daxa::ImageId transmittance;
            daxa::ImageId multiscattering;
            daxa::ImageId skyview;
        };

        struct TaskImages
        {
            daxa::TaskImageId swapchain_image;
            daxa::TaskImageId transmittance;
            daxa::TaskImageId multiscattering;
            daxa::TaskImageId sampled_image;
        };

        struct Pipelines
        {
            daxa::ComputePipeline transmittance_pipeline;
            daxa::ComputePipeline multiscattering_pipeline;
            daxa::RasterPipeline finalpass_pipeline;
        };

        daxa::Context daxa_context;
        daxa::Device daxa_device;
        daxa::Swapchain daxa_swapchain;
        daxa::PipelineCompiler daxa_pipeline_compiler;

        daxa::SamplerId default_sampler;

        Images daxa_images;
        Buffers daxa_buffers;
        Tasks daxa_tasks;
        TaskImages daxa_task_images;
        TaskBuffers daxa_task_buffers;
        Pipelines pipelines;


        void record_tasks();
        void record_clear_present_task();
        void record_render_sky_task();
        void create_resources();
};