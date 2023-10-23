// Adapted from https://github.com/corporateshark/poisson-disk-generator
#pragma once
#include <vector>
#include <cmath>
#include <random>

#include <daxa/types.hpp>
using namespace daxa::types;

struct GeneratePointsInfo
{
    daxa_i32 num_points;
    daxa_f32 min_dist = -1.0f;
    daxa_i32 retries = 100;
    daxa_u32 seed = 1;
};

//Acceleration structure for searching nearby points
struct Grid
{
    Grid(daxa_i32 cell_count, daxa_f32 cell_size) : 
        cell_count{cell_count},
        cell_size{cell_size},
        grid{std::vector(cell_count, std::vector(cell_count, daxa_f32vec2{-1.0f, -1.0f}))}
    {}

    inline auto point_to_grid(const daxa_f32vec2 point) const -> daxa_i32vec2 
    {
        return daxa_i32vec2{static_cast<daxa_i32>(point.x / cell_size), static_cast<daxa_i32>(point.y / cell_size)};
    }

    inline auto insert_point(const daxa_f32vec2 point) -> void
    {
        auto grid_pos = point_to_grid(point);
        grid.at(grid_pos.x).at(grid_pos.y) = point;
    }

    inline auto get_distance(const daxa_f32vec2 p1, const daxa_f32vec2 p2) const -> daxa_f32
    {
        daxa_f32 x_dist = p1.x - p2.x;
        daxa_f32 y_dist = p1.y - p2.y;
        return std::sqrt(x_dist * x_dist + y_dist * y_dist);
    }

    inline auto is_point_too_close(const daxa_f32vec2 point, const daxa_f32 min_dist) const -> bool
    {
        auto grid_pos = point_to_grid(point);
        for(daxa_i32 x = grid_pos.x - 1; x <= grid_pos.x + 1; x++)
        {
            for(daxa_i32 y = grid_pos.y - 1; y <= grid_pos.y + 1; y++)
            {
                if(x >= 0 && x < cell_count && y >= 0 && y < cell_count)
                {
                    const daxa_f32vec2 checked_point = grid.at(x).at(y);
                    if(checked_point.x != -1.0f && get_distance(point, checked_point) < min_dist)
                    {
                        return true;
                    }
                }
            }
        }
        return false;
    }

    private:
        daxa_i32 cell_count;
        daxa_f32 cell_size;
        std::vector<std::vector<daxa_f32vec2>> grid;

};

inline auto generate_poisson_points(const GeneratePointsInfo & info) -> std::vector<daxa_f32vec2> 
{ 
    std::mt19937 generator(info.seed);
    std::uniform_real_distribution<daxa_f32> dis(0.0, 1.0);

    auto generate_random_point_around = [&](const daxa_f32vec2 point, const daxa_f32 min_dist)
    {
        const daxa_f32 R1 = dis(generator);
        const daxa_f32 R2 = dis(generator);

        const daxa_f32 radius = min_dist * (R1 + 1.0f);
        const daxa_f32 angle = 2.0f * 3.141592653589f * R2;

        const daxa_f32 x = point.x + radius * std::cos(angle);
        const daxa_f32 y = point.y + radius * std::sin(angle);

        return daxa_f32vec2{x, y};
    };

    daxa_i32 real_num_points = static_cast<daxa_i32>(info.num_points);
    daxa_f32 real_min_dist = info.min_dist < 0.0f ? std::sqrt(daxa_f32(real_num_points)) / daxa_f32(real_num_points) : info.min_dist;

    std::vector<daxa_f32vec2> sample_points;
    std::vector<daxa_f32vec2> process_list;

    if(real_num_points == 0) { return sample_points; }

    const daxa_f32 cell_size = real_min_dist / std::sqrt(2.0f);
    const daxa_i32 cell_count = static_cast<daxa_i32>(std::ceil(1.0f/ cell_size));
    Grid grid = Grid(cell_count, cell_size);

    auto first_point = daxa_f32vec2{dis(generator), dis(generator)};
    process_list.push_back(first_point);
    sample_points.push_back(first_point);

    while(!process_list.empty() && sample_points.size() <= real_num_points)
    {
        const daxa_i32 idx = static_cast<daxa_i32>(dis(generator) * static_cast<daxa_f32>(process_list.size()));

        daxa_f32vec2 point = process_list.at(idx);
        process_list.at(idx) = process_list.at(process_list.size() - 1);
        process_list.pop_back();

        for(daxa_u32 i = 0; i < info.retries; i++)
        {
            const daxa_f32vec2 new_point = generate_random_point_around(point, real_min_dist);
            bool is_point_valid = new_point.x >= 0.0f && new_point.x <= 1.0f && new_point.y >= 0.0f && new_point.y <= 1.0f;
            if(is_point_valid && !grid.is_point_too_close(new_point, real_min_dist))
            {
                process_list.push_back(new_point);
                sample_points.push_back(new_point);
                grid.insert_point(new_point);
            }
        }
    }
    
    return sample_points;
}