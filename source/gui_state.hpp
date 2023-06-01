#pragma once

#include <array>
#include <daxa/types.hpp>
#include "renderer/context.hpp"
using namespace daxa::types;

struct GuiState
{
    f32vec2 sun_angle;
    f32vec3 new_camera_position;
};