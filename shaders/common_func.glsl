#define DAXA_SHADER_NO_NAMESPACE
#include <shared/shared.inl>

const f32 PLANET_RADIUS_OFFSET = 0.01;
const f32 PI = 3.1415926535897932384626433832795;

/* Return sqrt clamped to 0 */
f32 safe_sqrt(f32 x)
{
    return sqrt(max(0, x));
}

f32 from_subuv_to_unit(f32 u, f32 resolution) {
	return (u - 0.5 / resolution) * (resolution / (resolution - 1.0)); 
}

f32 from_unit_to_subuv(f32 u, f32 resolution) {
	 return (u + 0.5 / resolution) * (resolution / (resolution + 1.0));
}

struct TransmittanceParams
{
    f32 height;
    f32 zenith_cos_angle;
};

///	Transmittance LUT uses not uniform mapping -> transfer from mapping to texture uv
///	@param parameters
/// @param atmosphere_bottom - bottom radius of the atmosphere in km
/// @param atmosphere_top - top radius of the atmosphere in km
///	@return - uv of the corresponding texel
vec2 transmittance_lut_to_uv(TransmittanceParams parameters, f32 atmosphere_bottom, f32 atmosphere_top)
{
	float H = safe_sqrt(atmosphere_top * atmosphere_top - atmosphere_bottom * atmosphere_bottom);
	float rho = safe_sqrt(parameters.height * parameters.height - atmosphere_bottom * atmosphere_bottom);
	
	float discriminant = parameters.height * parameters.height * 
		(parameters.zenith_cos_angle * parameters.zenith_cos_angle - 1.0) +
		atmosphere_top * atmosphere_top;
	/* Distance to top atmosphere boundary */
	float d = max(0.0, (-parameters.height * parameters.zenith_cos_angle + safe_sqrt(discriminant)));

	float d_min = atmosphere_top - parameters.height;
	float d_max = rho + H;
	float mu = (d - d_min) / (d_max - d_min);
	float r = rho / H;

	return vec2(mu, r);
}

/// Transmittance LUT uses not uniform mapping -> transfer from uv to this mapping
/// @param uv - uv in the range [0,1]
/// @param atmosphere_bottom - bottom radius of the atmosphere in km
/// @param atmosphere_top - top radius of the atmosphere in km
/// @return - TransmittanceParams structure
TransmittanceParams uv_to_transmittance_lut_params(f32vec2 uv, f32 atmosphere_bottom, f32 atmosphere_top)
{
	TransmittanceParams params;
	f32 H = safe_sqrt(atmosphere_top * atmosphere_top - atmosphere_bottom * atmosphere_bottom.x);

	f32 rho = H * uv.y;
	params.height = safe_sqrt( rho * rho + atmosphere_bottom * atmosphere_bottom);

	f32 d_min = atmosphere_top - params.height;
	f32 d_max = rho + H;
	f32 d = d_min + uv.x * (d_max - d_min);
	
	params.zenith_cos_angle = d == 0.0 ? 1.0 : (H * H - rho * rho - d * d) / (2.0 * params.height * d);
	params.zenith_cos_angle = clamp(params.zenith_cos_angle, -1.0, 1.0);

	return params;
}

/// Return distance of the first intersection between ray and sphere
/// @param r0 - ray origin
/// @param rd - normalized ray direction
/// @param s0 - sphere center
/// @param sR - sphere radius
/// @return return distance of intersection or -1.0 if there is no intersection
f32 ray_sphere_intersect_nearest(f32vec3 r0, f32vec3 rd, f32vec3 s0, f32 sR)
{
	f32 a = dot(rd, rd);
	f32vec3 s0_r0 = r0 - s0;
	f32 b = 2.0 * dot(rd, s0_r0);
	f32 c = dot(s0_r0, s0_r0) - (sR * sR);
	f32 delta = b * b - 4.0*a*c;
	if (delta < 0.0 || a == 0.0)
	{
		return -1.0;
	}
	f32 sol0 = (-b - safe_sqrt(delta)) / (2.0*a);
	f32 sol1 = (-b + safe_sqrt(delta)) / (2.0*a);
	if (sol0 < 0.0 && sol1 < 0.0)
	{
		return -1.0;
	}
	if (sol0 < 0.0)
	{
		return max(0.0, sol1);
	}
	else if (sol1 < 0.0)
	{
		return max(0.0, sol0);
	}
	return max(0.0, min(sol0, sol1));
}

/// @param params - buffer reference to the atmosphere parameters buffer
/// @param position - position in the world where the sample is to be taken
/// @return atmosphere extinction at the desired point
f32vec3 sample_medium_extinction(BufferRef(AtmosphereParameters) params, f32vec3 position)
{
    const f32 height = length(position) - params.atmosphere_bottom;

    const f32 density_mie = exp(params.mie_density[1].exp_scale * height);
    const f32 density_ray = exp(params.rayleigh_density[1].exp_scale * height);
    const f32 density_ozo = clamp(height < params.absorption_density[0].layer_width ?
        params.absorption_density[0].lin_term * height + params.absorption_density[0].const_term :
        params.absorption_density[1].lin_term * height + params.absorption_density[1].const_term,
        0.0, 1.0);

    f32vec3 mie_extinction = params.mie_extinction * density_mie;
    f32vec3 ray_extinction = params.rayleigh_scattering * density_ray;
    f32vec3 ozo_extinction = params.absorption_extinction * density_ozo; 
    
    return mie_extinction + ray_extinction + ozo_extinction;
}

/// @param params - buffer reference to the atmosphere parameters buffer
/// @param position - position in the world where the sample is to be taken
/// @return atmosphere scattering at the desired point
f32vec3 sample_medium_scattering(BufferRef(AtmosphereParameters) params, f32vec3 position)
{
    const f32 height = length(position) - params.atmosphere_bottom;

    const f32 density_mie = exp(params.mie_density[1].exp_scale * height);
    const f32 density_ray = exp(params.rayleigh_density[1].exp_scale * height);
    const f32 density_ozo = clamp(height < params.absorption_density[0].layer_width ?
        params.absorption_density[0].lin_term * height + params.absorption_density[0].const_term :
        params.absorption_density[1].lin_term * height + params.absorption_density[1].const_term,
        0.0, 1.0);

    f32vec3 mie_scattering = params.mie_scattering * density_mie;
    f32vec3 ray_scattering = params.rayleigh_scattering * density_ray;
    /* Not considering ozon scattering in current version of this model */
    f32vec3 ozo_scattering = f32vec3(0.0, 0.0, 0.0);
    
    return mie_scattering + ray_scattering + ozo_scattering;
}