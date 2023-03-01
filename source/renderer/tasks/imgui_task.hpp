#pragma once

#include "../context.hpp"

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>
#include <daxa/utils/imgui.hpp>

inline void task_draw_imgui(Context & context)
{
    context.main_task_list.task_list.add_task({
        .used_images =
        {
            { 
                context.main_task_list.task_images.at(Images::SWAPCHAIN),
                daxa::TaskImageAccess::SHADER_WRITE_ONLY,
                daxa::ImageMipArraySlice{} 
            }
        },
        .task = [&](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();
            auto swapchain_image_dimensions = context.swapchain.get_surface_extent();
            auto swapchain_image = runtime.get_images(context.main_task_list.task_images.at(Images::SWAPCHAIN))[0];
            context.imgui_renderer.record_commands(
                ImGui::GetDrawData(), cmd_list, swapchain_image,
                swapchain_image_dimensions.x, swapchain_image_dimensions.y);
        },
        .debug_name = "Imgui Task",
    });
}