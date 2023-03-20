#include "planet_generator.hpp"
#include <CDT.h>

auto generate_planet() -> PlanetGeometry
{
    static u32 seed = 0;
    auto poisson_points = generate_poisson_points({ .num_points = 10000, .seed = seed++});
    CDT::Triangulation<float> cdt;

    int end_edge_idx = poisson_points.size();
    poisson_points.push_back({0.0f, 0.0f});
    poisson_points.push_back({1.0f, 0.0f});
    poisson_points.push_back({1.0f, 1.0f});
    poisson_points.push_back({0.0f, 1.0f});

    cdt.insertVertices(
        poisson_points.begin(),
        poisson_points.end(),
        [](const f32vec2 & p){ return p.x; },
        [](const f32vec2 & p){ return p.y; }
    );

    cdt.insertEdges(std::vector<CDT::Edge>{
        CDT::Edge(end_edge_idx    , end_edge_idx + 1),
        CDT::Edge(end_edge_idx + 1, end_edge_idx + 2),
        CDT::Edge(end_edge_idx + 2, end_edge_idx + 3),
        CDT::Edge(end_edge_idx + 3, end_edge_idx    )
    });

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