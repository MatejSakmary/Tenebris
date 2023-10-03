#pragma once

#include <string>

#include <daxa/daxa.hpp>
using namespace daxa::types;

struct LoadedImageInfo
{
    daxa::Format format;
    daxa::BufferId staging_buffer_id;
    i32vec3 resolution = {-1, -1, -1};
};

auto load_exr_data(std::string const & filepath, daxa::Device device) -> LoadedImageInfo;
auto load_dds_data(std::string const & filepath, daxa::Device device) -> LoadedImageInfo;