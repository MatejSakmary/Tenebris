#pragma once
#include <string>
#include <vector>
#include <variant>

#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/pipeline_manager.hpp>

struct LoadTextureInfo
{
    std::string filepath;
    daxa::TaskImage & dest_image;
};

struct CompressTextureInfo
{
    daxa::TaskImage & raw_texture;
    daxa::TaskImage & compressed_texture;
};

struct NormalsFromHeightInfo
{
    daxa::TaskImage & height_texture;
    daxa::TaskImage & normals_texture;
};

struct TextureManagerInfo
{
    daxa::Device device;
    daxa::PipelineManager pipeline_manager;
};

struct TextureManager
{
    TextureManager(TextureManager const &) = delete;
    TextureManager & operator= (TextureManager const &) = delete;

    TextureManager(TextureManagerInfo const & info);
    void load_texture(const LoadTextureInfo & load_info);
    void compress_hdr_texture(const CompressTextureInfo & compress_info);
    void normals_from_heightmap(const NormalsFromHeightInfo & normals_info);

    ~TextureManager();

    private:

        bool should_compress = false;
        TextureManagerInfo info;

        // compress image resources
        std::shared_ptr<daxa::ComputePipeline> compress;
        daxa::TaskImage compress_src_hdr_texture;
        daxa::TaskImage uint_compress_texture;
        daxa::TaskImage compress_dst_bc6h_texture;

        // load texture resources
        daxa::BufferId loaded_raw_data_buffer_id;
        daxa::TaskImage load_dst_hdr_texture;

        // normal map get resources
        std::shared_ptr<daxa::ComputePipeline> height_to_normal;
        daxa::TaskImage normal_src_hdr_texture;
        daxa::TaskImage normal_dst_hdr_texture;

        daxa::SamplerId nearest_sampler;

        daxa::TaskGraph upload_texture_task_graph;
        daxa::TaskGraph compress_texture_task_graph;
        daxa::TaskGraph height_to_normal_task_graph;
};
