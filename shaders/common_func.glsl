#define DAXA_SHADER_NO_NAMESPACE
#include <shared/shared.inl>

/* Return sqrt clamped to 0 */
f32 safe_sqrt(f32 x)
{
    return sqrt(max(0, x));
}

struct TransmittanceParams
{
    f32 height;
    f32 zenith_cos_angle;
};

/// Transmittance LUT uses not uniform mapping -> transfer from uv to this mapping
/// @param uv - uv in the range [0,1]
/// @param bottom_radius - bottom radius of the atmosphere in km
/// @param top_radius - top radius of the atmosphere in km
/// @return - height in x, zenith cos angle in y
TransmittanceParams uv_to_transmittance_lut_params(f32vec2 uv, f32 bottom_radius, f32 top_radius)
{
	TransmittanceParams params;
	f32 H = safe_sqrt(top_radius * top_radius - bottom_radius * bottom_radius.x);

	f32 rho = H * uv.y;
	params.height = safe_sqrt( rho * rho + bottom_radius * bottom_radius);

	f32 d_min = top_radius - params.height;
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
/// @return atmosphere density at the desired point
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