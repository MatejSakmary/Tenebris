#pragma once
#include <vector>

#include "poisson_generator.hpp"

#include <daxa/types.hpp>
using namespace daxa::types;

struct PlanetGeometry
{
    std::vector<daxa_f32vec2> vertices;
    std::vector<daxa_u32> indices;
};

auto generate_planet() -> PlanetGeometry;