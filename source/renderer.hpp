#pragma once

#include <unordered_map>

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

using namespace daxa::types;

struct Renderer
{
    Renderer(daxa::NativeWindowHandle window_handle);
    ~Renderer();

    void draw();

    private:

        struct ApplicationImages
        {
            daxa::ImageId swapchain_image;
        };

        struct ApplicationBuffers
        {
        };

        struct ApplicationTask
        {
            daxa::TaskList task;
            std::unordered_map<std::string, daxa::TaskImageId> task_images;
            std::unordered_map<std::string, daxa::TaskBufferId> task_buffers;
        };


        daxa::Context daxa_ctx;
        daxa::Device device;
        daxa::Swapchain swapchain;
        daxa::PipelineCompiler pipeline_compiler;

        ApplicationImages images;
        ApplicationTask clear_present_task;

        void record_tasks();
        void create_resources();
};