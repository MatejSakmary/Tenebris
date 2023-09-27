#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

DAXA_DECL_TASK_USES_BEGIN(ImGuiTaskBase, DAXA_UNIFORM_BUFFER_SLOT0)
DAXA_TASK_USE_IMAGE(_swapchain, REGULAR_2D, COLOR_ATTACHMENT)
DAXA_TASK_USE_IMAGE(_vsm_debug_page_table, REGULAR_2D, FRAGMENT_SHADER_SAMPLED)
DAXA_DECL_TASK_USES_END()

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