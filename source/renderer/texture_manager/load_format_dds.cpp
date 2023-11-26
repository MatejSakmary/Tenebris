// based on https://github.com/spnda/dds_image/tree/main
#include "load_formats.hpp"

#include <fstream>
#include <iostream>

#include "dds_types.hpp"
#include "../../utils.hpp"

template <typename T>
inline constexpr bool has_bit(T value, T bit) { return (value & bit) == bit; }

auto load_dds_data(std::string const & filepath, daxa::Device device) -> LoadedImageInfo
{
    DBG_ASSERT_TRUE_M(sizeof(DDSHeader) == 124, "[load_format_dds.cpp] DDS Header size mismatch. Must be 124 bytes");

    std::ifstream filestream(filepath, std::ios::binary | std::ios::in);
    if (!filestream.is_open()) 
    { 
        throw std::runtime_error("[load_dds_data()] Error unable to open file: " + filepath);
    }

    filestream.seekg(0, std::ios::end);
    auto file_size = filestream.tellg();

    // Read the header
    if (file_size < sizeof(DDSHeader)) 
    { 
        throw std::runtime_error(
            "[load_dds_data()] Error file " + filepath + " is too small to fit header"
        );
    }
        
    // Magic + Header
    static constexpr uint32_t MAGIC_PLUS_HEADER_SIZE = sizeof(daxa_u32) + sizeof(DDSHeader);
    static constexpr uint32_t ADDITIONAL_HEADER_SIZE = sizeof(Dx10Header);
    std::array<uint8_t, std::max(MAGIC_PLUS_HEADER_SIZE, ADDITIONAL_HEADER_SIZE)> read_buffer;

    // Read header
    filestream.seekg(0);
    filestream.read(reinterpret_cast<char*>(read_buffer.data()), MAGIC_PLUS_HEADER_SIZE);

    daxa_u32 const dds_magic = *(reinterpret_cast<daxa_u32*>(read_buffer.data()));
    DDSHeader header = *(reinterpret_cast<const DDSHeader*>(read_buffer.data() + sizeof(uint32_t)));

    // Validate header. A DWORD (magic number) containing the four character code value 'DDS ' (0x20534444).
    if (dds_magic != DdsMagicNumber::DDS) 
    {
        throw std::runtime_error(
            "[load_dds_data()] Error file " + filepath + " has wrong magic constant"
        );
    }

    bool has_additional_header = has_bit(header.pixelFormat.flags , PixelFormatFlags::FourCC ) &&
                                 has_bit(header.pixelFormat.fourCC, static_cast<daxa_u32>(DdsMagicNumber::DX10));
    Dx10Header additional_header;
    if(has_additional_header)
    {
        filestream.read(reinterpret_cast<char*>(read_buffer.data()), ADDITIONAL_HEADER_SIZE);
        additional_header = (*reinterpret_cast<const Dx10Header*>(read_buffer.data()));

        // "If the DDS_PIXELFORMAT dwFlags is set to DDPF_FOURCC and a dwFourCC is
        // set to "DX10", then the total file size needs to be at least 148
        // bytes."
        if(file_size < 148) 
        { 
            throw std::runtime_error(
                "[load_dds_data()] Error file " + filepath + " has additional header but filesize is too small"
            ); 
        }
    }
    if(header.mipmapCount > 1)
    {
        // TODO(msakmary) implement if needed
        throw std::runtime_error(
            "[load_dds_data()] Error file " + filepath + " mas mips TODO(msakmary) IMPLEMENT"
        );
    }
    // We'll always trust the DX10 header.
    if (has_additional_header == false) 
    {
        // TODO(msakmary) I am too lazy to implement this rn - if the need arises the format
        // can also be inffered from the header.pixelFormat (pixelFormatFlags and bitcount)
        throw std::runtime_error(
            "[load_dds_data()] Error file " + filepath + " does not have additional header TODO(msakmary) IMPLEMENT"
        );
    }
    if(additional_header.arraySize > 1)
    {
        // TODO(msakmary) implement if needed
        throw std::runtime_error(
            "[load_dds_data()] Error file " + filepath + " mas array size > 1 TODO(msakmary) IMPLEMENT"
        );
    }
    if((additional_header.dxgiFormat >= DXGI_FORMAT_BC1_TYPELESS && additional_header.dxgiFormat <= DXGI_FORMAT_BC5_SNORM) || 
       (additional_header.dxgiFormat >= DXGI_FORMAT_BC6H_TYPELESS && additional_header.dxgiFormat <= DXGI_FORMAT_BC7_UNORM_SRGB))
    {
        throw std::runtime_error(
            "[load_dds_data()] Error file " + filepath + " has BCX compressed format TODO(msakmary) IMPLEMENT"
        );
    }

    if (header.flags & HeaderFlags::Volume || header.caps2 & Caps2Flags::Cubemap) 
    {
        additional_header.resourceDimension = 3;
    } 
    else 
    {
        additional_header.resourceDimension = header.height > 1 ? 2 : 1;
    }
    
    auto const format = dxgi_to_daxa_format(additional_header.dxgiFormat);
    if(format == daxa::Format::UNDEFINED)
    {
        throw std::runtime_error(
            "[load_dds_data()] Error file " + filepath + " has unreckgonized format (probably just not implemented)"
        );
    }

    daxa_u32 const data_size = static_cast<daxa_u32>(file_size) - (MAGIC_PLUS_HEADER_SIZE + ADDITIONAL_HEADER_SIZE);

    DBG_ASSERT_TRUE_M(header.depth != 0, "TODO(msakmary) set this properly even for 2D images");
    daxa::ImageInfo tmp_info = 
    {
        .dimensions = static_cast<daxa_u32>(additional_header.resourceDimension),
        .format = format,
        .size = {header.width, header.height, header.depth},
        .usage = daxa::ImageUsageFlagBits::TRANSFER_DST   | 
                 daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                 daxa::ImageUsageFlagBits::SHADER_STORAGE,
        .name = "dds mem req tmp image"
    };
    auto const memory_requirements = device.get_memory_requirements(tmp_info);
    DBG_ASSERT_TRUE_M(memory_requirements.size == data_size, "TODO(msakmary) bug or compressed texture?");

    auto staging_buffer_id = device.create_buffer({
        .size = static_cast<daxa_u32>(memory_requirements.size),
        .allocate_info = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
        .name = "dds image staging buffer"
    });

    auto staging_buffer_ptr = device.get_host_address_as<char>(staging_buffer_id).value();
    filestream.read(staging_buffer_ptr, memory_requirements.size);
    filestream.close();
    return {
        .format = format,
        .staging_buffer_id = staging_buffer_id, 
        .resolution = {
            static_cast<daxa_i32>(header.width),
            static_cast<daxa_i32>(header.height),
            static_cast<daxa_i32>(header.depth)
        }
    };
}