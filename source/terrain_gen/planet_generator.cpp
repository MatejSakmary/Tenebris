#include "planet_generator.hpp"
#include <CDT.h>

auto generate_planet() -> PlanetGeometry
{
    static u32 seed = 0;
    auto poisson_points = generate_poisson_points({ .num_points = 10000, .seed = seed++});
    CDT::Triangulation<float> cdt;

    cdt.insertVertices(
        poisson_points.begin(),
        poisson_points.end(),
        [](const f32vec2 & p){ return p.x; },
        [](const f32vec2 & p){ return p.y; }
    );

    cdt.eraseSuperTriangle();

    PlanetGeometry geometry;
    geometry.vertices = std::move(poisson_points);

    for(auto triangle : cdt.triangles)
    {
        for(int i = 0; i < 3; i++)
        {
            geometry.indices.push_back(triangle.vertices.at(2 - i));
        }
    }

    return geometry;
}