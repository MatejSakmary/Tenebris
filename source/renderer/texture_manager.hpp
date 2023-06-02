#pragma once
#include <string>
#include <vector>
#include <variant>

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>
using namespace daxa::types;


struct LoadTextureInfo
{
    std::string path = {};
    daxa::TaskImage & dest_image;
};

struct ManagedTextureHandle
{
    i32 index = -1;
};

struct ManagedTexture
{
    daxa::BufferId id = {};
    std::string path = {};
    i32vec2 dimensions = {-1, -1};
    daxa::Format format = daxa::Format::UNDEFINED;
};

struct TextureManagerInfo
{
    daxa::Device device;
    std::shared_ptr<daxa::ComputePipeline> compress_pipeline;
};

struct TextureManager
{
    TextureManager(TextureManagerInfo const & info);
    void load_texture(const LoadTextureInfo & load_info);

    private:
        bool should_compress = false;
        TextureManagerInfo info;

        daxa::TaskList upload_texture_task_list;

        daxa::TaskImage hdr_texture;
        daxa::TaskImage uint_compress_texture;
        daxa::TaskImage bc6h_texture;

        daxa::BufferId curr_buffer_id;

        daxa::SamplerId nearest_sampler;
};
