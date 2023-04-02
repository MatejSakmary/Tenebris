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

    Context::TerrainParams terrain_params;

    f32vec2 sun_angle = {180.0f, 0.0f};
    f32 atmosphere_bottom = 6360.0f;
    f32 atmosphere_top = 6460.0f;
    f32 mie_scale_height = 1.2f;
    f32 rayleigh_scale_height = 8.0f;
    f32vec3 new_camera_position;
};