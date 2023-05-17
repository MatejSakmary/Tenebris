#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_list.inl>

DAXA_INL_TASK_USE_BEGIN(ImGuiTaskBase, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_IMAGE(_swapchain, daxa_Image2Du32, COLOR_ATTACHMENT)
DAXA_INL_TASK_USE_END()

#if __cplusplus
#include "../context.hpp"
#include <daxa/utils/imgui.hpp>
struct ImGuiTask : ImGuiTaskBase
{
    Context *context = {};
    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();
        auto swapchain_image_dimensions = context->swapchain.get_surface_extent();
        context->imgui_renderer.record_commands(
            ImGui::GetDrawData(), cmd_list, uses._swapchain.image(),
            swapchain_image_dimensions.x, swapchain_image_dimensions.y);
    }
};
#endif