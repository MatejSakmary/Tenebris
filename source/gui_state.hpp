#pragma once

#include <daxa/types.hpp>
using namespace daxa::types;

struct GuiState
{
    u32vec2 transmittance_LUT_dimensions;
    u32vec2 multiscattering_LUT_dimensions;
    u32vec2 skyview_LUT_dimensions;
    u32vec3 aerial_perspective_LUT_dimensions;
};