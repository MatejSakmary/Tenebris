#pragma once
#include <string>
#include <vector>
#include <variant>

#include <daxa/daxa.hpp>
#include <daxa/utils/task_graph.hpp>
using namespace daxa::types;


struct LoadTextureInfo
{
    std::string path = {};
    daxa::TaskImage & dest_image;
    std::optional<daxa::TaskImage *> dest_normal_map;
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
    std::shared_ptr<daxa::ComputePipeline> height_to_normal_pipeline;
};

struct TextureManager
{
    TextureManager(TextureManager const &) = delete;
    TextureManager & operator= (TextureManager const &) = delete;

    TextureManager(TextureManagerInfo const & info);
    void load_texture(const LoadTextureInfo & load_info);

    ~TextureManager();

    private:
        bool should_compress = false;
        TextureManagerInfo info;

        enum Conditionals
        {
            COMPRESS = 0,
            GEN_NORMALS,
            COUNT
        };

        std::array<bool, Conditionals::COUNT> conditionals_state;


        daxa::TaskImage hdr_texture;
        daxa::TaskImage uint_compress_texture;
        daxa::TaskImage bc6h_texture;

        daxa::TaskImage normal_texture;

        daxa::BufferId curr_buffer_id;

        daxa::SamplerId nearest_sampler;

        daxa::TaskGraph upload_texture_task_list;
};
