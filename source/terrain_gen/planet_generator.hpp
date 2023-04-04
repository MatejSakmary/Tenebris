#pragma once
#include <vector>

#include "poisson_generator.hpp"

#include <daxa/types.hpp>
using namespace daxa::types;

struct PlanetGeometry
{
    std::vector<f32vec2> vertices;
    std::vector<u32> indices;
};

auto generate_planet() -> PlanetGeometry;