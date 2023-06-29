#include "planet_generator.hpp"
#include <CDT.h>

auto generate_planet() -> PlanetGeometry
{
    const u32 terrain_res = 30u;

    PlanetGeometry geometry;
    geometry.vertices.reserve(terrain_res * terrain_res);

    /* Generate uniform plane filled with vertices */
    for (u32 i = 0; i < terrain_res; i++) {
        for (u32 j = 0; j < terrain_res; j++) {
            geometry.vertices.push_back(f32vec2{float(i) / (terrain_res - 1), float(j) / (terrain_res - 1)});
        }
    }

    /* Generate indices for above generated uniform plane */
    for (u32 i = 0; i < terrain_res - 1; i++) {
        for (u32 j = 0; j < terrain_res - 1; j++) {
            i32 i0 = j + i * terrain_res;
            i32 i1 = i0 + 1;
            i32 i2 = i0 + terrain_res;
            i32 i3 = i2 + 1;
            geometry.indices.emplace_back(i0);
            geometry.indices.emplace_back(i1);
            geometry.indices.emplace_back(i2);
            geometry.indices.emplace_back(i3);
            // geometry.indices.emplace_back(i3);
            // geometry.indices.emplace_back(i0);
        }
    }
    
    return geometry;

    // static u32 seed = 0;
    // auto poisson_points = generate_poisson_points({ .num_points = 10000, .seed = seed++});
    // CDT::Triangulation<float> cdt;

    // cdt.insertVertices(
    //     poisson_points.begin(),
    //     poisson_points.end(),
    //     [](const f32vec2 & p){ return p.x; },
    //     [](const f32vec2 & p){ return p.y; }
    // );

    // cdt.eraseSuperTriangle();

    // PlanetGeometry geometry;
    // geometry.vertices = std::move(poisson_points);

    // for(auto triangle : cdt.triangles)
    // {
    //     for(int i = 0; i < 3; i++)
    //     {
    //         geometry.indices.push_back(triangle.vertices.at(2 - i));
    //     }
    // }

    // return geometry;
}