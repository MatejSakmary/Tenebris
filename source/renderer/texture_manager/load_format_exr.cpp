#include "load_formats.hpp"

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

#include "../../utils.hpp"

struct CreateStagingBufferInfo
{
    daxa_i32vec2 dimensions;
    daxa_u32 present_channel_count;
    daxa::Device & device;
    std::string name;
    std::array<std::string,4> channel_names;
    std::unique_ptr<InputFile> & file;
};

struct ElemType
{
    daxa::Format format;
    daxa_u32 elem_cnt;
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
    daxa_u32 channel_cnt = parsed_channels.size() == 1 ? 0 : 3;
    auto texture_format = formats_lut.at(channel_cnt).at(daxa_u32(parsed_channels.back().type));
    ret.format = texture_format;
    ret.elem_cnt = daxa_u32(parsed_channels.size());
    ret.type = parsed_channels.back().type;
    return ret;
}

template <daxa_i32 NumElems, typename T, PixelType PixT>
auto load_texture_data(CreateStagingBufferInfo & info) -> daxa::BufferId
{
    using Elem = std::array<T,NumElems>;

    auto pos_from_name = [](const std::string_view name) -> daxa_i32
    {
        if(name[0] == 'R') return 0;
        else if(name[0] == 'G') return 1;
        else if(name[0] == 'B') return 2;
        else if(name[0] == 'A') return 3;
        else return -1;
    };

    auto new_buffer_id = info.device.create_buffer({
        .size = info.dimensions.x * info.dimensions.y * NumElems * daxa_u32(sizeof(T)),
        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        .name = info.name
    });
    auto * buffer_ptr = info.device.get_host_address_as<Elem>(new_buffer_id).value();
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
        info.file->readPixels(0, info.dimensions.y - 1);
    } catch (const std::exception &e) {
        DEBUG_OUT("[TextureManager::load_texture_data()] Error encountered " << e.what());
    }
    return new_buffer_id;
}

auto load_exr_data(std::string const & filepath, daxa::Device device) -> LoadedImageInfo
{
    setGlobalThreadCount(8);
    std::unique_ptr<InputFile> file;
    try 
    {
        // Open the EXR file and read its header
        file = std::make_unique<InputFile>(filepath.c_str());
    } 
    catch (const std::exception &e) 
    {
        throw std::runtime_error("[load_exr_data()] Error when reading file: " + filepath + " " + e.what());
    }

    Box2i data_window = file->header().dataWindow();
    daxa_i32vec2 resolution = {
        data_window.max.x - data_window.min.x + 1,
        data_window.max.y - data_window.min.y + 1
    };
    DBG_ASSERT_TRUE_M(data_window.min.x == 0 && data_window.min.y == 0, "TODO(msakmary) Allocate does not handle this case");

    DEBUG_OUT("=========== Loaded texture header: " << filepath << " ===========");

    auto texture_elem = get_texture_element(file);

    DEBUG_OUT("Size " << resolution.x << "x" << resolution.y);
    DEBUG_OUT("Format " << daxa::to_string(texture_elem.format));

    CreateStagingBufferInfo stanging_info{
        .dimensions = resolution,
        .present_channel_count = texture_elem.elem_cnt,
        .device = device,
        .name = "exr staging texture buffer",
        .channel_names = texture_elem.channel_names,
        .file = file
    };

    daxa::BufferId staging_buffer_id;
    switch(texture_elem.elem_cnt)
    {
        case 1:
        {
            if      (texture_elem.type == PixelType::UINT)  { staging_buffer_id = load_texture_data<1, daxa_u32, PixelType::UINT>(stanging_info); }
            else if (texture_elem.type == PixelType::HALF)  { staging_buffer_id = load_texture_data<1, half, PixelType::HALF>(stanging_info);}
            else if (texture_elem.type == PixelType::FLOAT) { staging_buffer_id = load_texture_data<1, daxa_f32, PixelType::FLOAT>(stanging_info);}
            break;
        }
        case 2:
        {
            DBG_ASSERT_TRUE_M(false, "[TextureManager::load_texture()] Unsupported number of channels in a texture");
            break;
        }
        case 3:
        {
            if      (texture_elem.type == PixelType::UINT)  { staging_buffer_id = load_texture_data<4, daxa_u32, PixelType::UINT>(stanging_info); }
            else if (texture_elem.type == PixelType::HALF)  { staging_buffer_id = load_texture_data<4, half, PixelType::HALF>(stanging_info);}
            else if (texture_elem.type == PixelType::FLOAT) { staging_buffer_id = load_texture_data<4, daxa_f32, PixelType::FLOAT>(stanging_info);}
            break;
        }
        case 4:
        {
            if      (texture_elem.type == PixelType::UINT)  { staging_buffer_id = load_texture_data<4, daxa_u32, PixelType::UINT>(stanging_info); }
            else if (texture_elem.type == PixelType::HALF)  { staging_buffer_id = load_texture_data<4, half, PixelType::HALF>(stanging_info);}
            else if (texture_elem.type == PixelType::FLOAT) { staging_buffer_id = load_texture_data<4, daxa_f32, PixelType::FLOAT>(stanging_info);}
            break;
        }
    }
    return {
        .format = texture_elem.format,
        .staging_buffer_id = staging_buffer_id,
        .resolution = {resolution.x, resolution.y, 1}
    };
}