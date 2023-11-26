#include "planet_generator.hpp"

auto generate_planet() -> PlanetGeometry
{
    const daxa_u32 terrain_res = 100u;

    PlanetGeometry geometry;
    geometry.vertices.reserve(terrain_res * terrain_res);

    /* Generate uniform plane filled with vertices */
    for (daxa_u32 i = 0; i < terrain_res; i++) {
        for (daxa_u32 j = 0; j < terrain_res; j++) {
            geometry.vertices.push_back(daxa_f32vec2{float(i) / (terrain_res - 1), float(j) / (terrain_res - 1)});
        }
    }

    /* Generate indices for above generated uniform plane */
    for (daxa_u32 i = 0; i < terrain_res - 1; i++) {
        for (daxa_u32 j = 0; j < terrain_res - 1; j++) {
            daxa_i32 i0 = j + i * terrain_res;
            daxa_i32 i1 = i0 + 1;
            daxa_i32 i2 = i0 + terrain_res;
            daxa_i32 i3 = i2 + 1;
            geometry.indices.emplace_back(i0);
            geometry.indices.emplace_back(i1);
            geometry.indices.emplace_back(i2);
            geometry.indices.emplace_back(i3);
            // geometry.indices.emplace_back(i3);
            // geometry.indices.emplace_back(i0);
        }
    }
    
    return geometry;

    // static daxa_u32 seed = 0;
    // auto poisson_points = generate_poisson_points({ .num_points = 10000, .seed = seed++});
    // CDT::Triangulation<float> cdt;

    // cdt.insertVertices(
    //     poisson_points.begin(),
    //     poisson_points.end(),
    //     [](const daxa_f32vec2 & p){ return p.x; },
    //     [](const daxa_f32vec2 & p){ return p.y; }
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