#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/post_process.inl"

DAXA_DECL_PUSH_CONSTANT(PostProcessPC, pc)

layout (location = 0) in f32vec2 in_uv;
layout (location = 0) out f32vec4 out_color;

// Used to convert from linear RGB to XYZ space
const f32mat3x3 RGB_2_XYZ = f32mat3x3(
    0.4124564, 0.2126729, 0.0193339,
    0.3575761, 0.7151522, 0.1191920,
    0.1804375, 0.0721750, 0.9503041
);

// Used to convert from XYZ to linear RGB space
const f32mat3x3 XYZ_2_RGB = f32mat3x3(
     3.2404542,-0.9692660, 0.0556434,
    -1.5371385, 1.8760108,-0.2040259,
    -0.4985314, 0.0415560, 1.0572252
);

// Converts a color from XYZ to xyY space (Y is luminosity)
f32vec3 xyz_to_xyY(f32vec3 xyz) {
    f32 Y = xyz.y;
    f32 x = xyz.x / (xyz.x + xyz.y + xyz.z);
    f32 y = xyz.y / (xyz.x + xyz.y + xyz.z);
    return f32vec3(x, y, Y);
}

// Converts a color from linear RGB to XYZ space
f32vec3 rgb_to_xyz(f32vec3 rgb) {
    return RGB_2_XYZ * rgb;
}

// Converts a color from linear RGB to xyY space
f32vec3 rgb_to_xyY(f32vec3 rgb) {
    f32vec3 xyz = rgb_to_xyz(rgb);
    return xyz_to_xyY(xyz);
}

// Converts a color from xyY space to XYZ space
f32vec3 xyY_to_xyz(f32vec3 xyY) {
    f32 Y = xyY.z;
    f32 x = Y * xyY.x / xyY.y;
    f32 z = Y * (1.0 - xyY.x - xyY.y) / xyY.y;
    return f32vec3(x, Y, z);
}

// Converts a color from XYZ to linear RGB space
f32vec3 xyz_to_rgb(f32vec3 xyz) {
    return XYZ_2_RGB * xyz;
}

// Converts a color from xyY space to linear RGB
f32vec3 xyY_to_rgb(f32vec3 xyY) {
    f32vec3 xyz = xyY_to_xyz(xyY);
    return xyz_to_rgb(xyz);
}

f32 Tonemap_ACES(f32 x) {
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const f32 a = 2.51;
    const f32 b = 0.03;
    const f32 c = 2.43;
    const f32 d = 0.59;
    const f32 e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}

void main()
{
    const f32vec3 hdr_color = texture(daxa_sampler2D(_offscreen, pc.sampler_id), in_uv).rgb;

    f32vec3 xyY = rgb_to_xyY(hdr_color);
    const f32 lp = xyY.z / (9.6 * deref(_average_luminance).luminance + 0.0001);
    xyY.z = Tonemap_ACES(lp);

    out_color = f32vec4(xyY_to_rgb(xyY), 1.0);
}