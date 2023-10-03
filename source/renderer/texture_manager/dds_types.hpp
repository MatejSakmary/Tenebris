#pragma once
#include <daxa/daxa.hpp>
using namespace daxa::types;

enum DXGI_FORMAT {
    DXGI_FORMAT_UNKNOWN	                    = 0,
    DXGI_FORMAT_R32G32B32A32_TYPELESS       = 1,
    DXGI_FORMAT_R32G32B32A32_FLOAT          = 2,
    DXGI_FORMAT_R32G32B32A32_UINT           = 3,
    DXGI_FORMAT_R32G32B32A32_SINT           = 4,
    DXGI_FORMAT_R32G32B32_TYPELESS          = 5,
    DXGI_FORMAT_R32G32B32_FLOAT             = 6,
    DXGI_FORMAT_R32G32B32_UINT              = 7,
    DXGI_FORMAT_R32G32B32_SINT              = 8,
    DXGI_FORMAT_R16G16B16A16_TYPELESS       = 9,
    DXGI_FORMAT_R16G16B16A16_FLOAT          = 10,
    DXGI_FORMAT_R16G16B16A16_UNORM          = 11,
    DXGI_FORMAT_R16G16B16A16_UINT           = 12,
    DXGI_FORMAT_R16G16B16A16_SNORM          = 13,
    DXGI_FORMAT_R16G16B16A16_SINT           = 14,
    DXGI_FORMAT_R32G32_TYPELESS             = 15,
    DXGI_FORMAT_R32G32_FLOAT                = 16,
    DXGI_FORMAT_R32G32_UINT                 = 17,
    DXGI_FORMAT_R32G32_SINT                 = 18,
    DXGI_FORMAT_R32G8X24_TYPELESS           = 19,
    DXGI_FORMAT_D32_FLOAT_S8X24_UINT        = 20,
    DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS    = 21,
    DXGI_FORMAT_X32_TYPELESS_G8X24_UINT     = 22,
    DXGI_FORMAT_R10G10B10A2_TYPELESS        = 23,
    DXGI_FORMAT_R10G10B10A2_UNORM           = 24,
    DXGI_FORMAT_R10G10B10A2_UINT            = 25,
    DXGI_FORMAT_R11G11B10_FLOAT             = 26,
    DXGI_FORMAT_R8G8B8A8_TYPELESS           = 27,
    DXGI_FORMAT_R8G8B8A8_UNORM              = 28,
    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB         = 29,
    DXGI_FORMAT_R8G8B8A8_UINT               = 30,
    DXGI_FORMAT_R8G8B8A8_SNORM              = 31,
    DXGI_FORMAT_R8G8B8A8_SINT               = 32,
    DXGI_FORMAT_R16G16_TYPELESS             = 33,
    DXGI_FORMAT_R16G16_FLOAT                = 34,
    DXGI_FORMAT_R16G16_UNORM                = 35,
    DXGI_FORMAT_R16G16_UINT                 = 36,
    DXGI_FORMAT_R16G16_SNORM                = 37,
    DXGI_FORMAT_R16G16_SINT                 = 38,
    DXGI_FORMAT_R32_TYPELESS                = 39,
    DXGI_FORMAT_D32_FLOAT                   = 40,
    DXGI_FORMAT_R32_FLOAT                   = 41,
    DXGI_FORMAT_R32_UINT                    = 42,
    DXGI_FORMAT_R32_SINT                    = 43,
    DXGI_FORMAT_R24G8_TYPELESS              = 44,
    DXGI_FORMAT_D24_UNORM_S8_UINT           = 45,
    DXGI_FORMAT_R24_UNORM_X8_TYPELESS       = 46,
    DXGI_FORMAT_X24_TYPELESS_G8_UINT        = 47,
    DXGI_FORMAT_R8G8_TYPELESS               = 48,
    DXGI_FORMAT_R8G8_UNORM                  = 49,
    DXGI_FORMAT_R8G8_UINT                   = 50,
    DXGI_FORMAT_R8G8_SNORM                  = 51,
    DXGI_FORMAT_R8G8_SINT                   = 52,
    DXGI_FORMAT_R16_TYPELESS                = 53,
    DXGI_FORMAT_R16_FLOAT                   = 54,
    DXGI_FORMAT_D16_UNORM                   = 55,
    DXGI_FORMAT_R16_UNORM                   = 56,
    DXGI_FORMAT_R16_UINT                    = 57,
    DXGI_FORMAT_R16_SNORM                   = 58,
    DXGI_FORMAT_R16_SINT                    = 59,
    DXGI_FORMAT_R8_TYPELESS                 = 60,
    DXGI_FORMAT_R8_UNORM                    = 61,
    DXGI_FORMAT_R8_UINT                     = 62,
    DXGI_FORMAT_R8_SNORM                    = 63,
    DXGI_FORMAT_R8_SINT                     = 64,
    DXGI_FORMAT_A8_UNORM                    = 65,
    DXGI_FORMAT_R1_UNORM                    = 66,
    DXGI_FORMAT_R9G9B9E5_SHAREDEXP          = 67,
    DXGI_FORMAT_R8G8_B8G8_UNORM             = 68,
    DXGI_FORMAT_G8R8_G8B8_UNORM             = 69,
    DXGI_FORMAT_BC1_TYPELESS                = 70,
    DXGI_FORMAT_BC1_UNORM                   = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB              = 72,
    DXGI_FORMAT_BC2_TYPELESS                = 73,
    DXGI_FORMAT_BC2_UNORM                   = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB              = 75,
    DXGI_FORMAT_BC3_TYPELESS                = 76,
    DXGI_FORMAT_BC3_UNORM                   = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB              = 78,
    DXGI_FORMAT_BC4_TYPELESS                = 79,
    DXGI_FORMAT_BC4_UNORM                   = 80,
    DXGI_FORMAT_BC4_SNORM                   = 81,
    DXGI_FORMAT_BC5_TYPELESS                = 82,
    DXGI_FORMAT_BC5_UNORM                   = 83,
    DXGI_FORMAT_BC5_SNORM                   = 84,
    DXGI_FORMAT_B5G6R5_UNORM                = 85,
    DXGI_FORMAT_B5G5R5A1_UNORM              = 86,
    DXGI_FORMAT_B8G8R8A8_UNORM              = 87,
    DXGI_FORMAT_B8G8R8X8_UNORM              = 88,
    DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM  = 89,
    DXGI_FORMAT_B8G8R8A8_TYPELESS           = 90,
    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB         = 91,
    DXGI_FORMAT_B8G8R8X8_TYPELESS           = 92,
    DXGI_FORMAT_B8G8R8X8_UNORM_SRGB         = 93,
    DXGI_FORMAT_BC6H_TYPELESS               = 94,
    DXGI_FORMAT_BC6H_UF16                   = 95,
    DXGI_FORMAT_BC6H_SF16                   = 96,
    DXGI_FORMAT_BC7_TYPELESS                = 97,
    DXGI_FORMAT_BC7_UNORM                   = 98,
    DXGI_FORMAT_BC7_UNORM_SRGB              = 99,
    DXGI_FORMAT_AYUV                        = 100,
    DXGI_FORMAT_Y410                        = 101,
    DXGI_FORMAT_Y416                        = 102,
    DXGI_FORMAT_NV12                        = 103,
    DXGI_FORMAT_P010                        = 104,
    DXGI_FORMAT_P016                        = 105,
    DXGI_FORMAT_420_OPAQUE                  = 106,
    DXGI_FORMAT_YUY2                        = 107,
    DXGI_FORMAT_Y210                        = 108,
    DXGI_FORMAT_Y216                        = 109,
    DXGI_FORMAT_NV11                        = 110,
    DXGI_FORMAT_AI44                        = 111,
    DXGI_FORMAT_IA44                        = 112,
    DXGI_FORMAT_P8                          = 113,
    DXGI_FORMAT_A8P8                        = 114,
    DXGI_FORMAT_B4G4R4A4_UNORM              = 115,
    DXGI_FORMAT_P208                        = 130,
    DXGI_FORMAT_V208                        = 131,
    DXGI_FORMAT_V408                        = 132,
    DXGI_FORMAT_FORCE_UINT                  = 0xffffffff
};

static constexpr inline auto dxgi_to_daxa_format(DXGI_FORMAT dxgi_format) -> daxa::Format
{
    switch(dxgi_format)
    {
        case DXGI_FORMAT_UNKNOWN:	       
            return daxa::Format::UNDEFINED;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:
            return daxa::Format::R32G32B32A32_SFLOAT;
        case DXGI_FORMAT_R32G32B32A32_UINT:        
            return daxa::Format::R32G32B32A32_UINT;
        case DXGI_FORMAT_R32G32B32A32_SINT:          
            return daxa::Format::R32G32B32A32_SINT;
        case DXGI_FORMAT_R32G32B32_FLOAT:  
            return daxa::Format::R32G32B32_SFLOAT;
        case DXGI_FORMAT_R32G32B32_UINT:             
            return daxa::Format::R32G32B32_UINT;
        case DXGI_FORMAT_R32G32B32_SINT:             
            return daxa::Format::R32G32B32_SINT;
        default:
            //TODO(msakmary) Implement rest of the types
            return daxa::Format::UNDEFINED;
    }
}

#define MAKE_FOUR_CHARACTER_CODE(char1, char2, char3, char4)                                                                               \
    static_cast<u32>(char1) | (static_cast<u32>(char2) << 8) | (static_cast<u32>(char3) << 16) |                            \
        (static_cast<u32>(char4) << 24)

enum DdsMagicNumber {
    DDS = MAKE_FOUR_CHARACTER_CODE('D', 'D', 'S', ' '),

    DXT1 = MAKE_FOUR_CHARACTER_CODE('D', 'X', 'T', '1'), // BC1_UNORM
    DXT2 = MAKE_FOUR_CHARACTER_CODE('D', 'X', 'T', '2'), // BC2_UNORM
    DXT3 = MAKE_FOUR_CHARACTER_CODE('D', 'X', 'T', '3'), // BC2_UNORM
    DXT4 = MAKE_FOUR_CHARACTER_CODE('D', 'X', 'T', '4'), // BC3_UNORM
    DXT5 = MAKE_FOUR_CHARACTER_CODE('D', 'X', 'T', '5'), // BC3_UNORM
    ATI1 = MAKE_FOUR_CHARACTER_CODE('A', 'T', 'I', '1'), // BC4_UNORM
    BC4U = MAKE_FOUR_CHARACTER_CODE('B', 'C', '4', 'U'), // BC4_UNORM
    BC4S = MAKE_FOUR_CHARACTER_CODE('B', 'C', '4', 'S'), // BC4_SNORM
    ATI2 = MAKE_FOUR_CHARACTER_CODE('A', 'T', 'I', '2'), // BC5_UNORM
    BC5U = MAKE_FOUR_CHARACTER_CODE('B', 'C', '5', 'U'), // BC5_UNORM
    BC5S = MAKE_FOUR_CHARACTER_CODE('B', 'C', '5', 'S'), // BC5_SNORM
    RGBG = MAKE_FOUR_CHARACTER_CODE('R', 'G', 'B', 'G'), // R8G8_B8G8_UNORM
    GRBG = MAKE_FOUR_CHARACTER_CODE('G', 'R', 'B', 'G'), // G8R8_G8B8_UNORM
    YUY2 = MAKE_FOUR_CHARACTER_CODE('Y', 'U', 'Y', '2'), // YUY2
    UYVY = MAKE_FOUR_CHARACTER_CODE('U', 'Y', 'V', 'Y'),

    DX10 = MAKE_FOUR_CHARACTER_CODE('D', 'X', '1', '0'), // Any DXGI format
};

enum HeaderFlags : u32 {
    Caps = 0x1,
    Height = 0x2,
    Width = 0x4,
    Pitch = 0x8,
    PixelFormat = 0x1000,
    Texture = Caps | Height | Width | PixelFormat,
    Mipmap = 0x20000,
    Volume = 0x800000,
    LinearSize = 0x00080000,
};

enum Caps2Flags : u32 {
    Cubemap = 0x200,
};



/** An additional header for DX10 */
struct Dx10Header {
    DXGI_FORMAT dxgiFormat;
    i32 resourceDimension;
    u32 miscFlags;
    u32 arraySize;
    u32 miscFlags2;
};

enum class PixelFormatFlags : uint32_t {
    AlphaPixels = 0x1,
    Alpha = 0x2,
    FourCC = 0x4,
    PAL8 = 0x20,
    RGB = 0x40,
    RGBA = RGB | AlphaPixels,
    YUV = 0x200,
    Luminance = 0x20000,
    LuminanceA = Luminance | AlphaPixels,
};

inline PixelFormatFlags operator&(PixelFormatFlags a, PixelFormatFlags b) {
    return static_cast<PixelFormatFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

struct FilePixelFormat 
{
    u32 size;
    PixelFormatFlags flags;
    u32 fourCC;
    u32 bitCount;
    u32 rBitMask;
    u32 gBitMask;
    u32 bBitMask;
    u32 aBitMask;
};

struct DDSHeader
{

    u32 size;
    HeaderFlags flags;
    u32 height;
    u32 width;
    u32 pitch;
    u32 depth;
    u32 mipmapCount;
    u32 reserved[11];
    FilePixelFormat pixelFormat;
    u32 caps1;
    u32 caps2;
    u32 caps3;
    u32 caps4;
    u32 reserved2;
};