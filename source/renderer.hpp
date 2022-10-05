#pragma once

#include <daxa/daxa.hpp>
using namespace daxa::types;

struct Renderer
{
    Renderer(daxa::NativeWindowHandle window_handle);
    ~Renderer();

    void draw();

    private:
        // daxa::Context daxa_ctx;
        // daxa::Device device;
        // daxa::Swapchain swapchain;
        // daxa::PipelineCompiler pipeline_compiler;
};