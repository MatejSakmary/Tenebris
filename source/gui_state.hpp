#pragma once

#include <array>
#include <daxa/types.hpp>
#include "renderer/context.hpp"
using namespace daxa::types;

struct GuiState
{
    u32vec2 trans_lut_dim;
    u32vec2 mult_lut_dim;
    u32vec2 sky_lut_dim;
    f32vec2 sun_angle;
    f32vec3 new_camera_position;
    
};