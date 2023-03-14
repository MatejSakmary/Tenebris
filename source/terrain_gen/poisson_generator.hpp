// Adapted from https://github.com/corporateshark/poisson-disk-generator
#pragma once
#include <vector>
#include <cmath>
#include <random>

#include <daxa/types.hpp>
using namespace daxa::types;

struct GeneratePointsInfo
{
    i32 num_points;
    f32 min_dist = -1.0f;
    i32 retries = 30;
    u32 seed = 1;
};

//Acceleration structure for searching nearby points
struct Grid
{
    Grid(i32 cell_count, f32 cell_size) : 
        cell_count{cell_count},
        cell_size{cell_size},
        grid{std::vector(cell_count, std::vector(cell_count, f32vec2{-1.0f, -1.0f}))}
    {}

    inline auto point_to_grid(const f32vec2 point) const -> i32vec2 
    {
        return i32vec2{static_cast<i32>(point.x / cell_size), static_cast<i32>(point.y / cell_size)};
    }

    inline auto insert_point(const f32vec2 point) -> void
    {
        auto grid_pos = point_to_grid(point);
        grid.at(grid_pos.x).at(grid_pos.y) = point;
    }

    inline auto get_distance(const f32vec2 p1, const f32vec2 p2) const -> f32
    {
        f32 x_dist = p1.x - p2.x;
        f32 y_dist = p1.y - p2.y;
        return std::sqrt(x_dist * x_dist + y_dist * y_dist);
    }

    inline auto is_point_too_close(const f32vec2 point, const f32 min_dist) const -> bool
    {
        auto grid_pos = point_to_grid(point);
        for(i32 x = grid_pos.x - 1; x <= grid_pos.x + 1; x++)
        {
            for(i32 y = grid_pos.y - 1; y <= grid_pos.y + 1; y++)
            {
                if(x >= 0 && x < cell_count && y >= 0 && y < cell_count)
                {
                    const f32vec2 checked_point = grid.at(x).at(y);
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
        i32 cell_count;
        f32 cell_size;
        std::vector<std::vector<f32vec2>> grid;

};

inline auto generate_poisson_points(const GeneratePointsInfo & info) -> std::vector<f32vec2> 
{ 
    std::mt19937 generator(info.seed);
    std::uniform_real_distribution<f32> dis(0.0, 1.0);

    auto generate_random_point_around = [&](const f32vec2 point, const f32 min_dist)
    {
        const f32 R1 = dis(generator);
        const f32 R2 = dis(generator);

        const f32 radius = min_dist * (R1 + 1.0f);
        const f32 angle = 2.0f * 3.141592653589f * R2;

        const f32 x = point.x + radius * std::cos(angle);
        const f32 y = point.y + radius * std::sin(angle);

        return f32vec2{x, y};
    };

    const f32 fourth_PI = 0.78539816339f;
    i32 real_num_points = static_cast<i32>(fourth_PI * info.num_points);
    f32 real_min_dist = info.min_dist < 0.0f ? std::sqrt(f32(real_num_points)) / f32(real_num_points) : info.min_dist;

    std::vector<f32vec2> sample_points;
    std::vector<f32vec2> process_list;

    if(real_num_points == 0) { return sample_points; }

    const f32 cell_size = real_min_dist / std::sqrt(2.0f);
    const i32 cell_count = static_cast<i32>(std::ceil(1.0f/ cell_size));
    Grid grid = Grid(cell_count, cell_size);

    f32vec2 first_point = f32vec2{dis(generator), dis(generator)};
    process_list.push_back(first_point);
    sample_points.push_back(first_point);

    while(!process_list.empty() && sample_points.size() <= real_num_points)
    {
        const i32 idx = static_cast<i32>(dis(generator) * process_list.size());

        f32vec2 point = process_list.at(idx);
        process_list.at(idx) = process_list.at(process_list.size() - 1);
        process_list.pop_back();

        for(u32 i = 0; i < info.retries; i++)
        {
            const f32vec2 new_point = generate_random_point_around(point, real_min_dist);
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