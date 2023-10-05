#include "texture_manager.hpp"

#include "../../utils.hpp"
#include <array>
#include <variant>

#include "load_formats.hpp"
#include "tasks/bc6h_compress.inl"
#include "tasks/height_to_normal.inl"

TextureManager::TextureManager(TextureManagerInfo const & c_info) : info{c_info}
{
    nearest_sampler = info.device.create_sampler({
        .magnification_filter = daxa::Filter::NEAREST,
        .minification_filter = daxa::Filter::NEAREST,
        .mipmap_filter = daxa::Filter::NEAREST,
        .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .address_mode_w = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .border_color = daxa::BorderColor::FLOAT_OPAQUE_BLACK
    });
    compress = info.pipeline_manager.add_compute_pipeline(get_BC6H_pipeline()).value(); 
    height_to_normal = info.pipeline_manager.add_compute_pipeline(get_height_to_normal_pipeline()).value(); 

    // ================= UPLOAD TEXTURE TASK GRAPH ====================================================
    load_dst_hdr_texture = daxa::TaskImage({.name = "texture manager load dst task image"});
    upload_texture_task_graph = daxa::TaskGraph({
        .device = info.device,
        .permutation_condition_count = 0,
        .name = "texture manager upload task graph"
    });

    upload_texture_task_graph.use_persistent_image(load_dst_hdr_texture);

    upload_texture_task_graph.add_task({
        .uses = { daxa::ImageTransferWrite<>{load_dst_hdr_texture}},
        .task = [=, this](daxa::TaskInterface ti)
        {
            auto cmd_list = ti.get_command_list();

            auto image_info = info.device.info_image(ti.uses[load_dst_hdr_texture].image());
            cmd_list.copy_buffer_to_image({
                .buffer = this->loaded_raw_data_buffer_id,
                .buffer_offset = 0,
                .image = ti.uses[load_dst_hdr_texture].image(),
                .image_extent = {
                    static_cast<u32>(image_info.size.x),
                    static_cast<u32>(image_info.size.y),
                    static_cast<u32>(image_info.size.z)
                }
            });
        },
        .name = "copy buffer into raw image",
    });

    upload_texture_task_graph.submit({});
    upload_texture_task_graph.complete({});

    // ================== HEIGHT TO NORMAL TASK GRAPH ================================================
    normal_src_hdr_texture = daxa::TaskImage({.name = "texture manager normal src task image"});
    normal_dst_hdr_texture = daxa::TaskImage({.name = "texture manager normal dst task image"}); 

    height_to_normal_task_graph = daxa::TaskGraph({
        .device = info.device,
        .permutation_condition_count = 0,
        .name = "texture manager height to normals task graph"
    });

    height_to_normal_task_graph.use_persistent_image(normal_src_hdr_texture);
    height_to_normal_task_graph.use_persistent_image(normal_dst_hdr_texture);

    height_to_normal_task_graph.add_task(HeightToNormalTask{{
        .uses = {
            ._height_texture = normal_src_hdr_texture.view(),
            ._normal_texture = normal_dst_hdr_texture.view()
        }},
        height_to_normal,
        info.device,
        nearest_sampler
    });

    height_to_normal_task_graph.submit({});
    height_to_normal_task_graph.complete({});

    // ================== COMPRESS TEXTURE TASK GRAPH =================================================
    compress_src_hdr_texture = daxa::TaskImage({.name = "texture manager compress src task image"});
    uint_compress_texture = daxa::TaskImage({.name = "texture manager compress uint task image"}); 
    compress_dst_bc6h_texture = daxa::TaskImage({.name = "texture manager compress dst task image"}); 

    compress_texture_task_graph = daxa::TaskGraph({
        .device = info.device,
        .permutation_condition_count = 0,
        .name = "texture manager compress texture task graph"
    });

    compress_texture_task_graph.use_persistent_image(compress_src_hdr_texture);
    compress_texture_task_graph.use_persistent_image(uint_compress_texture);
    compress_texture_task_graph.use_persistent_image(compress_dst_bc6h_texture);

    compress_texture_task_graph.add_task(BC6HCompressTask{{
        .uses = {
            ._src_texture = compress_src_hdr_texture.view(),
            ._dst_texture = uint_compress_texture.view()
        }},
        compress,
        info.device,
        nearest_sampler
    });

    compress_texture_task_graph.add_task({
        .uses = { 
            daxa::ImageTransferRead<>{uint_compress_texture},
            daxa::ImageTransferWrite<>{compress_dst_bc6h_texture}
        },
        .task = [&, this](daxa::TaskInterface ti)
        {
            auto cmd_list = ti.get_command_list();
            {
                auto image_info = info.device.info_image(ti.uses[uint_compress_texture].image());
                cmd_list.copy_image_to_image({
                    .src_image = ti.uses[uint_compress_texture].image(),
                    .dst_image = ti.uses[compress_dst_bc6h_texture].image(),
                    .extent = {image_info.size.x - 1, image_info.size.y - 1, 1}
                });
            }
        },
        .name = "transfer compressed into bc6h image",
    });

    compress_texture_task_graph.submit({});
    compress_texture_task_graph.complete({});
}

#include <chrono>
#include <atomic>

namespace shino
{
    template <typename Clock = std::chrono::high_resolution_clock>
    class stopwatch
    {
        const typename Clock::time_point start_point;
    public:
        stopwatch() : 
            start_point(Clock::now())
        {}

        template <typename Rep = typename Clock::duration::rep, typename Units = typename Clock::duration>
        Rep elapsed_time() const
        {
            std::atomic_thread_fence(std::memory_order_relaxed);
            auto counted_time = std::chrono::duration_cast<Units>(Clock::now() - start_point).count();
            std::atomic_thread_fence(std::memory_order_relaxed);
            return static_cast<Rep>(counted_time);
        }
    };

    using precise_stopwatch = stopwatch<>;
    using system_stopwatch = stopwatch<std::chrono::system_clock>;
    using monotonic_stopwatch = stopwatch<std::chrono::steady_clock>;
};

void TextureManager::load_texture(const LoadTextureInfo &load_info)
{
    LoadedImageInfo image_info;
    shino::precise_stopwatch stopwatch;

    if(load_info.filepath.ends_with(".exr"sv)) { image_info = load_exr_data(load_info.filepath, info.device); }
    if(load_info.filepath.ends_with(".dds"sv)) { image_info = load_dds_data(load_info.filepath, info.device); }

    auto actual_wait_time = stopwatch.elapsed_time<unsigned int, std::chrono::milliseconds>();
    DEBUG_OUT("[TextureManager::loat_texture_data()] Load of " + load_info.filepath + " took " << actual_wait_time << " ms");

    u32 const image_dimensions = 
        std::min(image_info.resolution.z - 1, 1) + 
        std::min(image_info.resolution.y - 1, 1) +
        std::min(image_info.resolution.x - 1, 1);

    // Creating load hdr destination image
    load_dst_hdr_texture.set_images({
        .images = {
            std::array{
                info.device.create_image({
                    .dimensions = image_dimensions,
                    .format = image_info.format,
                    .size = {
                        static_cast<u32>(image_info.resolution.x),
                        static_cast<u32>(image_info.resolution.y),
                        static_cast<u32>(image_info.resolution.z)
                    },
                    // TODO(msakmary) The usages should probably be exposed to the user
                    .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | 
                             daxa::ImageUsageFlagBits::SHADER_STORAGE |
                             daxa::ImageUsageFlagBits::TRANSFER_DST,
                    .allocate_info = daxa::AutoAllocInfo{daxa::MemoryFlagBits::DEDICATED_MEMORY},
                    .name = "raw texture"
                })
            }
        }
    });

    loaded_raw_data_buffer_id = image_info.staging_buffer_id;
    upload_texture_task_graph.execute({});

    info.device.wait_idle();
    
    load_dst_hdr_texture.swap_images(load_info.dest_image);
    load_dst_hdr_texture.set_images({});
    info.device.destroy_buffer(loaded_raw_data_buffer_id);
}

void TextureManager::normals_from_heightmap(const NormalsFromHeightInfo & normals_info)
{
    auto texture_dimensions = info.device.info_image(normals_info.height_texture.get_state().images[0]).size;
    normals_info.height_texture.swap_images(normal_src_hdr_texture);

    normal_dst_hdr_texture.set_images({
        .images = {
            std::array{
                info.device.create_image({
                    .format = daxa::Format::R32G32B32A32_SFLOAT,
                    .size = {static_cast<u32>(texture_dimensions.x), static_cast<u32>(texture_dimensions.y), 1},
                    .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .allocate_info = daxa::AutoAllocInfo{daxa::MemoryFlagBits::DEDICATED_MEMORY},
                    .name = "normal dst map hdr"
                })
            }
        },
    });

    height_to_normal_task_graph.execute({});
    info.device.wait_idle();

    normal_src_hdr_texture.swap_images(normals_info.height_texture);
    normal_dst_hdr_texture.swap_images(normals_info.normals_texture);

    normal_src_hdr_texture.set_images({});
    normal_dst_hdr_texture.set_images({});
}

void TextureManager::compress_hdr_texture(const CompressTextureInfo & compress_info)
{
    auto texture_dimensions = info.device.info_image(compress_info.raw_texture.get_state().images[0]).size;

	u32 width_round = (texture_dimensions.x + BC6HCompressTask::BC_BLOCK_SIZE - 1) / BC6HCompressTask::BC_BLOCK_SIZE;
	u32 height_round = (texture_dimensions.y + BC6HCompressTask::BC_BLOCK_SIZE - 1) / BC6HCompressTask::BC_BLOCK_SIZE;

    compress_info.raw_texture.swap_images(compress_src_hdr_texture);

    uint_compress_texture.set_images({
        .images = {
            std::array{
                info.device.create_image({
                    .format = daxa::Format::R32G32B32A32_UINT,
                    .size = {width_round, height_round, 1},
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::SHADER_STORAGE,
                    .allocate_info = daxa::AutoAllocInfo{daxa::MemoryFlagBits::DEDICATED_MEMORY},
                    .name = "uint compress texture"
                })
            }
        },
    });

    compress_dst_bc6h_texture.set_images({
        .images = {
            std::array{
                info.device.create_image({
                    .format = daxa::Format::BC6H_UFLOAT_BLOCK,
                    .size = {static_cast<u32>(texture_dimensions.x), static_cast<u32>(texture_dimensions.y), 1},
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
                    .allocate_info = daxa::AutoAllocInfo{daxa::MemoryFlagBits::DEDICATED_MEMORY},
                    .name = "diffuse map bc6h"
                })
            }
        },
    });

    compress_texture_task_graph.execute({});
    info.device.wait_idle();

    compress_src_hdr_texture.swap_images(compress_info.raw_texture);
    compress_dst_bc6h_texture.swap_images(compress_info.compressed_texture);
    info.device.destroy_image(uint_compress_texture.get_state().images[0]);

    compress_src_hdr_texture.set_images({});
    uint_compress_texture.set_images({});
    compress_dst_bc6h_texture.set_images({});
}

TextureManager::~TextureManager()
{
    info.device.destroy_sampler(nearest_sampler);
}