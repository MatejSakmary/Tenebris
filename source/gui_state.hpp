#pragma once

#include <array>
#include <daxa/types.hpp>
#include "renderer/context.hpp"
using namespace daxa::types;

struct GuiState
{
    std::array<i32vec3, Images::LUT_COUNT> lut_dimensions 
    {{
        {256, 64, 1},
        {32, 32, 1},
        {192, 128, 1}
    }};
    f32vec2 sun_angle = {180.0f, 0.0f};
};