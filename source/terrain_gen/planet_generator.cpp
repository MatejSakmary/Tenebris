#include "planet_generator.hpp"
#include <CDT.h>

auto generate_planet() -> PlanetGeometry
{
    auto poisson_points = generate_poisson_points({ .num_points = 1000 });
    CDT::Triangulation<float> cdt;

    auto begin_ptr = reinterpret_cast<CDT::V2d<float> *>(poisson_points.data());
    cdt.insertVertices(std::vector(begin_ptr, begin_ptr + poisson_points.size()));

    PlanetGeometry geometry;
    geometry.vertices = std::move(poisson_points);

    for(auto triangle : cdt.triangles)
    {
        for(int i = 0; i < 3; i++)
        {
            geometry.indices.push_back(triangle.vertices.at(i));
        }
    }

    return geometry;
}