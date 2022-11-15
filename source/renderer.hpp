#pragma once

#include <unordered_map>

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

using namespace daxa::types;

struct Renderer
{
    Renderer(daxa::NativeWindowHandle window_handle);
    ~Renderer();

    void resize();
    void draw();

    private:

        struct ApplicationTasks
        {
            daxa::TaskList clear_present;
        };

        struct ApplicationTaskImages
        {
            daxa::TaskImageId swapchain_image;
        };

        struct ApplicationImages
        {
            daxa::ImageId swapchain_image;
        };

        struct ApplicationBuffers
        {
        };

        daxa::Context daxa_context;
        daxa::Device daxa_device;
        daxa::Swapchain daxa_swapchain;
        daxa::PipelineCompiler daxa_pipeline_compiler;

        ApplicationImages daxa_images;
        ApplicationTaskImages daxa_task_images;
        ApplicationTasks daxa_tasks;

        void record_tasks();
        void create_resources();
};