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
#ifdef __DEBUG__
        if(!parsed_channels.empty())
            if(parsed_channels.back().type != i.channel().type ||
               parsed_channels.back().linear != i.channel().pLinear)
        {
            DEBUG_OUT("[TextureManager::get_texture_element()]" <<
                      "Images with nonuniform channel types are not supported");
            return {daxa::Format::UNDEFINED, -1u, PixelType::NUM_PIXELTYPES};
        }
#endif
        parsed_channels.emplace_back(ChannelInfo{
            .name = i.name(),
            .type = i.channel().type,
            .linear = i.channel().pLinear
        });
    }
    ElemType ret;
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

    auto texture_format = formats_lut.at(parsed_channels.size() - 1).at(u32(parsed_channels.back().type));
    ret.format = texture_format;
    ret.elem_cnt = u32(parsed_channels.size());
    ret.type = parsed_channels.back().type;
    return ret;
}

struct CreateStagingBufferInfo
{
    i32vec2 dimensions;
    daxa::Device & device;
    std::string name;
    std::array<std::string,4> channel_names;
    std::unique_ptr<InputFile> & file;
};
template <i32 NumElems, typename T, PixelType PixT>
auto load_texture_data(const CreateStagingBufferInfo & info) -> daxa::BufferId
{
    auto new_buffer_id = info.device.create_buffer({
        .size = info.dimensions.x * info.dimensions.y * NumElems * u32(sizeof(T)),
        .allocate_info = daxa::AutoAllocInfo{daxa::MemoryFlagBits::HOST_ACCESS_RANDOM},
        .name = info.name
    });
    auto * buffer_ptr = info.device.get_host_address_as<char>(new_buffer_id);

    FrameBuffer frame_buffer;
    for(int i = 0; i <= NumElems - 1; i++) {
        frame_buffer.insert(
            info.channel_names.at(i),
            Slice(PixT, buffer_ptr, sizeof(T), sizeof(T) * info.dimensions.x));
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

auto TextureManager::load_texture(const LoadTextureInfo &info) -> ManagedTextureHandle
{
    std::unique_ptr<InputFile> file;
    try {
        // Open the EXR file and read its header
        file = std::make_unique<InputFile>(info.path.c_str());
    } catch (const std::exception &e) {
        DEBUG_OUT("error when reading file: " << info.path << " " << e.what());
    }

    Box2i data_window = file->header().dataWindow();
    i32vec2 size = {
        data_window.max.x - data_window.min.x + 1,
        data_window.max.y - data_window.min.y + 1
    };
    ASSERT_MSG(data_window.min.x == 0 && data_window.min.y == 0, "TODO(msakmary) Allocate does not handle this case");

    DEBUG_OUT("=========== Loaded texture header: " << info.path << " ===========");

    auto texture_elem = get_texture_element(file);

    DEBUG_OUT("Size " << size.x << "x" << size.y);
    DEBUG_OUT("Format " << daxa::to_string(texture_elem.format));

    ManagedTexture new_texture = ManagedTexture{
        .path = info.path,
        .dimensions = size,
        .format = texture_elem.format,
    };

    CreateStagingBufferInfo stanging_info{
        .dimensions = size,
        .device = info.device,
        .name = "managed texture buffer " + info.path,
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
            if      (texture_elem.type == PixelType::UINT) { new_texture.id = load_texture_data<2, u32, PixelType::UINT>(stanging_info); }
            else if (texture_elem.type == PixelType::HALF) { new_texture.id = load_texture_data<2, half, PixelType::HALF>(stanging_info);}
            else if (texture_elem.type == PixelType::FLOAT) { new_texture.id = load_texture_data<2, f32, PixelType::FLOAT>(stanging_info);}
            break;
        }
        case 3:
        {
            if      (texture_elem.type == PixelType::UINT) { new_texture.id = load_texture_data<3, u32, PixelType::UINT>(stanging_info); }
            else if (texture_elem.type == PixelType::HALF) { new_texture.id = load_texture_data<3, half, PixelType::HALF>(stanging_info);}
            else if (texture_elem.type == PixelType::FLOAT) { new_texture.id = load_texture_data<3, f32, PixelType::FLOAT>(stanging_info);}
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
    managed_textures.emplace_back(new_texture);
    return {.index = i32(managed_textures.size()) - 1};
}

auto TextureManager::reload_textures() -> std::vector<ManagedTextureHandle>
{
    return {};
}
