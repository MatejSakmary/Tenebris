#include "texture_manager.hpp"
#include <ImfArray.h>
#include <ImfChannelList.h>
#include <ImfInputFile.h>
#include <ImfMatrixAttribute.h>
#include <ImfOutputFile.h>
#include <ImfStringAttribute.h>
#include <ImfRgbaFile.h>
#include <OpenEXRConfig.h>

using namespace OPENEXR_IMF_NAMESPACE;
using namespace IMATH_NAMESPACE;

#include "../utils.hpp"
#include <array>
#include <variant>

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
    setGlobalThreadCount(8);




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
                .image_extent = {static_cast<u32>(image_info.size.x), static_cast<u32>(image_info.size.y), 1}
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
        &(this->info),
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
        &(this->info),
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

struct ElemType
{
    daxa::Format format;
    u32 elem_cnt;
    PixelType type;
    std::array<std::string, 4> channel_names;
};

auto get_texture_element(const std::unique_ptr<InputFile> & file) -> ElemType
{
    struct ChannelInfo
    {
        std::string name;
        PixelType type;
        bool linear;
    };
    std::vector<ChannelInfo> parsed_channels;

    const ChannelList &channels = file->header().channels();
    for (ChannelList::ConstIterator i = channels.begin(); i != channels.end(); ++i)
    {
        parsed_channels.emplace_back(ChannelInfo{
            .name = i.name(),
            .type = i.channel().type,
            .linear = i.channel().pLinear
        });
        if(!parsed_channels.empty())
            if(parsed_channels.back().type != i.channel().type ||
               parsed_channels.back().linear != i.channel().pLinear)
        {
            DBG_ASSERT_TRUE_M(false, "[TextureManager::get_texture_element()] Images with nonuniform channel types are not supported");
        }
    }
    ElemType ret;
    DBG_ASSERT_TRUE_M(parsed_channels.size() <= 4, "[TextureManager::get_texture_element()] Not supporting more channels than 4");
    DBG_ASSERT_TRUE_M(parsed_channels.size() != 2, "[TextureManager::get_texture_element()] Not supporting two channel textures");

#ifdef __DEBUG__
    std::array channels_present{false, false, false, false};
    for(int j = 0; const auto & parsed_channel : parsed_channels)
    {
        ret.channel_names.at(j++) = parsed_channel.name;
        if(parsed_channel.name == "R") {channels_present.at(0) = true;}
        else if(parsed_channel.name == "G") {channels_present.at(1) = true;}
        else if(parsed_channel.name == "B") {channels_present.at(2) = true;}
        else if(parsed_channel.name == "A") {channels_present.at(3) = true;}
    }
    for(int i = 0; i < parsed_channels.size(); i++)
    {
        static constexpr const char channel_names[4]{'R', 'G', 'B', 'A'};
        if(!channels_present.at(i))
        {
            DEBUG_OUT("Texture has " << parsed_channels.size() << " channels but does not contain channel with name "
                       << channel_names[i] << " This might result in incorrect data being loaded");
        }
    }
#endif

    // first index is number of channels, second is the channel data type
    std::array<std::array<daxa::Format, 4>, 4> formats_lut =
    {{
        {daxa::Format::R32_UINT, daxa::Format::R16_SFLOAT, daxa::Format::R32_SFLOAT, daxa::Format::R8_UINT},
        {daxa::Format::R32G32_UINT, daxa::Format::R16G16_SFLOAT, daxa::Format::R32G32_SFLOAT, daxa::Format::R8G8_UINT},
        {daxa::Format::R32G32B32_UINT, daxa::Format::R16G16B16_SFLOAT, daxa::Format::R32G32B32_SFLOAT, daxa::Format::R8G8B8_UINT},
        {daxa::Format::R32G32B32A32_UINT, daxa::Format::R16G16B16A16_SFLOAT, daxa::Format::R32G32B32A32_SFLOAT, daxa::Format::R8G8B8A8_UINT},
    }};

    // no support for texture size 2
    u32 channel_cnt = parsed_channels.size() == 1 ? 0 : 3;
    auto texture_format = formats_lut.at(channel_cnt).at(u32(parsed_channels.back().type));
    ret.format = texture_format;
    ret.elem_cnt = u32(parsed_channels.size());
    ret.type = parsed_channels.back().type;
    return ret;
}

struct CreateStagingBufferInfo
{
    i32vec2 dimensions;
    u32 present_channel_count;
    daxa::Device & device;
    std::string name;
    std::array<std::string,4> channel_names;
    std::unique_ptr<InputFile> & file;
};


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

template <i32 NumElems, typename T, PixelType PixT>
auto load_texture_data(CreateStagingBufferInfo & info) -> daxa::BufferId
{
    using Elem = std::array<T,NumElems>;

    auto pos_from_name = [](const std::string_view name) -> i32
    {
        if(name[0] == 'R') return 0;
        else if(name[0] == 'G') return 1;
        else if(name[0] == 'B') return 2;
        else if(name[0] == 'A') return 3;
        else return -1;
    };

    auto new_buffer_id = info.device.create_buffer({
        .size = info.dimensions.x * info.dimensions.y * NumElems * u32(sizeof(T)),
        .allocate_info = daxa::AutoAllocInfo{daxa::MemoryFlagBits::HOST_ACCESS_RANDOM},
        .name = info.name
    });
    auto * buffer_ptr = info.device.get_host_address_as<Elem>(new_buffer_id);
    FrameBuffer frame_buffer;

    std::array<int, 4> positions{-1, -1, -1, -1};
    std::array<bool, 4> position_occupied{false, false, false, false};

    if(info.present_channel_count == 3) { info.channel_names.at(3) = "A"; }

    for(int i = 0; i < NumElems; i++) {
        positions.at(i) = pos_from_name(info.channel_names.at(i));
        if (positions.at(i) != -1) {position_occupied.at(positions.at(i)) = true; }
    }

    for(int i = 0; i < NumElems; i++)
    {
        if(positions.at(i) == -1)
        {
            for(int j = 0; j < NumElems; j++)
            {
                if(!position_occupied.at(j))
                {
                    position_occupied.at(j) = true;
                    positions.at(i) = j;
                    break;
                }
            }
        }
    }

    for(int i = 0; i < NumElems; i++) {
        frame_buffer.insert(
            info.channel_names.at(i),
            Slice(
                PixT,                                                        // Type
                reinterpret_cast<char*>(&buffer_ptr[0].at(positions.at(i))), // Position offset
                sizeof(Elem), sizeof(Elem) * info.dimensions.x,              // x_string and y_stride
                1, 1,                                                        // sampling rates
                0.0                                                          // fill value
            )
        );  
    }
    try
    {
        info.file->setFrameBuffer(frame_buffer);
        shino::precise_stopwatch stopwatch;
        info.file->readPixels(0, info.dimensions.y - 1);
        auto actual_wait_time = stopwatch.elapsed_time<unsigned int, std::chrono::milliseconds>();
        DEBUG_OUT("[TextureManager::loat_texture_data()] Load took " << actual_wait_time << " ms");
    } catch (const std::exception &e) {
        DEBUG_OUT("[TextureManager::load_texture_data()] Error encountered " << e.what());
    }
    return new_buffer_id;
}

void TextureManager::load_texture(const LoadTextureInfo &load_info)
{
    std::unique_ptr<InputFile> file;
    try {
        // Open the EXR file and read its header
        file = std::make_unique<InputFile>(load_info.path.c_str());
    } catch (const std::exception &e) {
        DEBUG_OUT("error when reading file: " << load_info.path << " " << e.what());
    }

    Box2i data_window = file->header().dataWindow();
    i32vec2 size = {
        data_window.max.x - data_window.min.x + 1,
        data_window.max.y - data_window.min.y + 1
    };
    DBG_ASSERT_TRUE_M(data_window.min.x == 0 && data_window.min.y == 0, "TODO(msakmary) Allocate does not handle this case");

    DEBUG_OUT("=========== Loaded texture header: " << load_info.path << " ===========");

    auto texture_elem = get_texture_element(file);

    DEBUG_OUT("Size " << size.x << "x" << size.y);
    DEBUG_OUT("Format " << daxa::to_string(texture_elem.format));

    ManagedTexture new_texture = ManagedTexture{
        .path = load_info.path,
        .dimensions = size,
        .format = texture_elem.format,
    };

    CreateStagingBufferInfo stanging_info{
        .dimensions = size,
        .present_channel_count = texture_elem.elem_cnt,
        .device = info.device,
        .name = "managed texture buffer " + load_info.path,
        .channel_names = texture_elem.channel_names,
        .file = file
    };

    switch(texture_elem.elem_cnt)
    {
        case 1:
        {
            if      (texture_elem.type == PixelType::UINT) { new_texture.id = load_texture_data<1, u32, PixelType::UINT>(stanging_info); }
            else if (texture_elem.type == PixelType::HALF) { new_texture.id = load_texture_data<1, half, PixelType::HALF>(stanging_info);}
            else if (texture_elem.type == PixelType::FLOAT) { new_texture.id = load_texture_data<1, f32, PixelType::FLOAT>(stanging_info);}
            break;
        }
        case 2:
        {
            DBG_ASSERT_TRUE_M(false, "[TextureManager::load_texture()] Unsupported number of channels in a texture");
            break;
        }
        case 3:
        {
            if      (texture_elem.type == PixelType::UINT) { new_texture.id = load_texture_data<4, u32, PixelType::UINT>(stanging_info); }
            else if (texture_elem.type == PixelType::HALF) { new_texture.id = load_texture_data<4, half, PixelType::HALF>(stanging_info);}
            else if (texture_elem.type == PixelType::FLOAT) { new_texture.id = load_texture_data<4, f32, PixelType::FLOAT>(stanging_info);}
            break;
        }
        case 4:
        {
            if      (texture_elem.type == PixelType::UINT) { new_texture.id = load_texture_data<4, u32, PixelType::UINT>(stanging_info); }
            else if (texture_elem.type == PixelType::HALF) { new_texture.id = load_texture_data<4, half, PixelType::HALF>(stanging_info);}
            else if (texture_elem.type == PixelType::FLOAT) { new_texture.id = load_texture_data<4, f32, PixelType::FLOAT>(stanging_info);}
            break;
        }
    }

    // Creating load hdr destination image
    load_dst_hdr_texture.set_images({
        .images = {
            std::array{
                info.device.create_image({
                    .format = texture_elem.format,
                    .size = {static_cast<u32>(new_texture.dimensions.x), static_cast<u32>(new_texture.dimensions.y), 1},
                    .usage = daxa::ImageUsageFlagBits::SHADER_READ_ONLY | 
                             daxa::ImageUsageFlagBits::TRANSFER_DST |
                             daxa::ImageUsageFlagBits::SHADER_READ_WRITE,
                    .allocate_info = daxa::AutoAllocInfo{daxa::MemoryFlagBits::DEDICATED_MEMORY},
                    .name = "raw texture"
                })
            }
        }
    });

    loaded_raw_data_buffer_id = new_texture.id;
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
                    .usage = daxa::ImageUsageFlagBits::SHADER_READ_WRITE | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
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
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_SRC | daxa::ImageUsageFlagBits::SHADER_READ_WRITE,
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
                    .usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_READ_ONLY,
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