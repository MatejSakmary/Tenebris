#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/post_process.inl"

DAXA_DECL_PUSH_CONSTANT(PostProcessPC, pc)

#define AGX 0
#define SCUFFED_ACES 1
#define TONY_MC_MAPFACE 2

#define TONEMAPPING TONY_MC_MAPFACE

// Source - https://github.com/BelmuTM/Noble/blob/master/shaders/programs/post/pre_final.glsl
//        - https://github.com/BelmuTM/Noble/blob/master/shaders/include/post/grading.glsl

const mat3 SRGB_2_XYZ_MAT = mat3(
	0.4124564, 0.3575761, 0.1804375,
    0.2126729, 0.7151522, 0.0721750,
    0.0193339, 0.1191920, 0.9503041
);

const mat3 XYZ_2_SRGB_MAT = mat3(
	 3.2409699419,-1.5373831776,-0.4986107603,
    -0.9692436363, 1.8759675015, 0.0415550574,
	 0.0556300797,-0.2039769589, 1.0569715142
);
#if TONEMAPPING == AGX
const daxa_f32 SRGB_ALPHA = 0.055;


daxa_f32vec3  saturate(daxa_f32vec3 x)  { return clamp(x, daxa_f32vec3(0.0), daxa_f32vec3(1.0)); }

daxa_f32 luminance(daxa_f32vec3 color) 
{
    daxa_f32vec3 luminanceCoefficients = SRGB_2_XYZ_MAT[1];
    return dot(color, luminanceCoefficients);
}

const daxa_f32mat3x3 agxTransform = daxa_f32mat3x3(
    0.842479062253094 , 0.0423282422610123, 0.0423756549057051,
    0.0784335999999992, 0.878468636469772 , 0.0784336,
    0.0792237451477643, 0.0791661274605434, 0.879142973793104
);

const daxa_f32mat3x3 agxTransformInverse = daxa_f32mat3x3(
     1.19687900512017  , -0.0528968517574562, -0.0529716355144438,
    -0.0980208811401368,  1.15190312990417  , -0.0980434501171241,
    -0.0990297440797205, -0.0989611768448433,  1.15107367264116
);

daxa_f32vec3 agxDefaultContrastApproximation(daxa_f32vec3 x) {
    daxa_f32vec3 x2 = x * x;
    daxa_f32vec3 x4 = x2 * x2;
  
    return + 15.5     * x4 * x2
           - 40.14    * x4 * x
           + 31.96    * x4
           - 6.868    * x2 * x
           + 0.4298   * x2
           + 0.1191   * x
           - 0.00232;
}

void agx(inout daxa_f32vec3 color) {
    const daxa_f32 minEv = -12.47393;
    const daxa_f32 maxEv = 4.026069;

    color = agxTransform * color;
    color = clamp(log2(color), minEv, maxEv);
    color = (color - minEv) / (maxEv - minEv);
    color = agxDefaultContrastApproximation(color);
}

void agxEotf(inout daxa_f32vec3 color) {
    color = agxTransformInverse * color;
}

void agxLook(inout daxa_f32vec3 color) {
    // Punchy
    const daxa_f32vec3  slope      = daxa_f32vec3(1.1);
    const daxa_f32vec3  power      = daxa_f32vec3(1.2);
    const daxa_f32 saturation = 1.3;

    daxa_f32 luma = luminance(color);
  
    color = pow(color * slope, power);
    color = luma + saturation * (color - luma);
}
#endif

#if (TONEMAPPING == AGX) || (TONEMAPPING == TONY_MC_MAPFACE)
const daxa_f32 exposureBias      = 0.5;
const daxa_f32 calibration       = 12.5;  // Light meter calibration
const daxa_f32 sensorSensitivity = 100.0; // Sensor sensitivity

daxa_f32 computeEV100fromLuminance(daxa_f32 luminance) {
    return log2(luminance * sensorSensitivity * exposureBias / calibration);
}

daxa_f32 computeExposureFromEV100(daxa_f32 ev100) {
    return 1.0 / (1.2 * exp2(ev100));
}

daxa_f32 computeExposure(daxa_f32 averageLuminance) {
	daxa_f32 ev100	 = computeEV100fromLuminance(averageLuminance);
	daxa_f32 exposure = computeExposureFromEV100(ev100);

	return exposure;
}
#endif

#if TONEMAPPING == SCUFFED_ACES
// Used to convert from linear RGB to XYZ space
const daxa_f32mat3x3 RGB_2_XYZ = (daxa_f32mat3x3(
    0.4124564, 0.2126729, 0.0193339,
    0.3575761, 0.7151522, 0.1191920,
    0.1804375, 0.0721750, 0.9503041
));

// Used to convert from XYZ to linear RGB space
const daxa_f32mat3x3 XYZ_2_RGB = (daxa_f32mat3x3(
     3.2404542,-0.9692660, 0.0556434,
    -1.5371385, 1.8760108,-0.2040259,
    -0.4985314, 0.0415560, 1.0572252
));

// Converts a color from linear RGB to XYZ space
daxa_f32vec3 rgb_to_xyz(daxa_f32vec3 rgb) {
    return RGB_2_XYZ * rgb;
}

// Converts a color from XYZ to linear RGB space
daxa_f32vec3 xyz_to_rgb(daxa_f32vec3 xyz) {
    return XYZ_2_RGB * xyz;
}

// Converts a color from XYZ to xyY space (Y is luminosity)
daxa_f32vec3 xyz_to_xyY(daxa_f32vec3 xyz) {
    daxa_f32 Y = xyz.y;
    daxa_f32 x = xyz.x / (xyz.x + xyz.y + xyz.z);
    daxa_f32 y = xyz.y / (xyz.x + xyz.y + xyz.z);
    return daxa_f32vec3(x, y, Y);
}

// Converts a color from linear RGB to xyY space
daxa_f32vec3 rgb_to_xyY(daxa_f32vec3 rgb) {
    daxa_f32vec3 xyz = rgb_to_xyz(rgb);
    return xyz_to_xyY(xyz);
}

// Converts a color from xyY space to XYZ space
daxa_f32vec3 xyY_to_xyz(daxa_f32vec3 xyY) {
    daxa_f32 Y = xyY.z;
    daxa_f32 x = Y * xyY.x / xyY.y;
    daxa_f32 z = Y * (1.0 - xyY.x - xyY.y) / xyY.y;
    return daxa_f32vec3(x, Y, z);
}

// Converts a color from xyY space to linear RGB
daxa_f32vec3 xyY_to_rgb(daxa_f32vec3 xyY) {
    daxa_f32vec3 xyz = xyY_to_xyz(xyY);
    return xyz_to_rgb(xyz);
}

daxa_f32 tonemap_ACES(daxa_f32 x) {
    // Narkowicz 2015, "ACES Filmic Tone Mapping Curve"
    const daxa_f32 a = 2.51;
    const daxa_f32 b = 0.03;
    const daxa_f32 c = 2.43;
    const daxa_f32 d = 0.59;
    const daxa_f32 e = 0.14;
    return (x * (a * x + b)) / (x * (c * x + d) + e);
}
#endif
// Source - https://github.com/h3r2tic/tony-mc-mapface/tree/main 
#if TONEMAPPING == TONY_MC_MAPFACE
daxa_f32vec3 tony_mc_mapface(daxa_f32vec3 exposed_color)
{
    // Apply a non-linear transform that the LUT is encoded with.
    const daxa_f32vec3 encoded = exposed_color / (exposed_color + 1.0);

    // Align the encoded range to texel centers.
    const daxa_f32 LUT_DIMS = 48.0;
    const daxa_f32vec3 uv = encoded * ((LUT_DIMS - 1.0) / LUT_DIMS) + 0.5 / LUT_DIMS;

    // Note: for OpenGL, do `uv.y = 1.0 - uv.y`
    return texture(daxa_sampler3D(_tonemapping_lut, pc.llce_sampler), uv).xyz;
}
#endif

#define WHITE_POINT   6500
#define WHITE_BALANCE 6500


const daxa_f32mat3x3 CONE_RESP_CAT02 = daxa_f32mat3x3(
	daxa_f32vec3( 0.7328, 0.4296,-0.1624),
	daxa_f32vec3(-0.7036, 1.6975, 0.0061),
	daxa_f32vec3( 0.0030, 0.0136, 0.9834)
);

vec3 fromXYZ(vec3 color) { return color * XYZ_2_SRGB_MAT; }
vec3 toXYZ(vec3 color) { return color * SRGB_2_XYZ_MAT; }
mat3 fromXYZ(mat3 mat) { return mat * XYZ_2_SRGB_MAT; }
mat3 toXYZ(mat3 mat) { return mat * SRGB_2_XYZ_MAT; }

vec3 plancks(float temperature, vec3 lambda) {
    const float h = 6.62607015e-16; // Planck's constant
    const float c = 2.99792458e17;  // Speed of light in a vacuum
    const float k = 1.38064852e-5;  // Boltzmann's constant

    float numerator   = 2.0 * h * pow(c, 2.0);
    vec3  denominator = (exp(h * c / (lambda * k * temperature)) - vec3(1.0)) * pow(lambda, daxa_f32vec3(5.0));
    return (numerator / denominator) * pow(1e9, 2.0);
}

vec3 blackbody(float temperature) {
    vec3 rgb  = plancks(temperature, vec3(660.0, 550.0, 440.0));
         rgb /= max(max(rgb.r, rgb.g), rgb.b); // Keeping the values below 1.0
    return rgb;
}

daxa_f32mat3x3 chromaticAdaptationMatrix(daxa_f32vec3 source, daxa_f32vec3 destination) {
	daxa_f32vec3 sourceLMS      = source * CONE_RESP_CAT02;
	daxa_f32vec3 destinationLMS = destination * CONE_RESP_CAT02;
	daxa_f32vec3 tmp            = destinationLMS / sourceLMS;

	daxa_f32mat3x3 vonKries = daxa_f32mat3x3(
		tmp.x, 0.0, 0.0,
		0.0, tmp.y, 0.0,
		0.0, 0.0, tmp.z
	);

	return (CONE_RESP_CAT02 * vonKries) * inverse(CONE_RESP_CAT02);
}

void whiteBalance(inout daxa_f32vec3 color) {
    vec3 source           = toXYZ(blackbody(WHITE_BALANCE));
    vec3 destination      = toXYZ(blackbody(WHITE_POINT  ));
    mat3 chromaAdaptation = fromXYZ(toXYZ(chromaticAdaptationMatrix(source, destination)));

    color *= chromaAdaptation;
}

layout (location = 0) in daxa_f32vec2 in_uv;
layout (location = 0) out daxa_f32vec4 out_color;
void main()
{
    daxa_f32vec3 hdr_color = texture(daxa_sampler2D(_offscreen, pc.sampler_id), in_uv).rgb;

#if TONEMAPPING == AGX
    daxa_f32 exposure = computeExposure(deref(_average_luminance).luminance);
    daxa_f32vec3 exposed_color = hdr_color * exposure;

    agx(exposed_color);
    agxLook(exposed_color);
    agxEotf(exposed_color);
    daxa_f32vec3 tonemapped_color = exposed_color;
#elif TONEMAPPING == TONY_MC_MAPFACE
    const daxa_f32 exposure = computeExposure(deref(_average_luminance).luminance);
    const daxa_f32vec3 exposed_color = hdr_color * exposure;
    daxa_f32vec3 tonemapped_color = tony_mc_mapface(exposed_color);
#elif TONEMAPPING == SCUFFED_ACES
    daxa_f32vec3 xyY = rgb_to_xyY(hdr_color);
    daxa_f32 lp = xyY.z / (9.6 * deref(_average_luminance).luminance + 0.0001); 
    xyY.z = tonemap_ACES(lp);
    daxa_f32vec3 tonemapped_color = xyY_to_rgb(xyY);
#endif
    whiteBalance(tonemapped_color);
    out_color = daxa_f32vec4(tonemapped_color, 1.0);
}