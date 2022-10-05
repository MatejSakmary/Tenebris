#include <shared/shared.inl>

DAXA_USE_PUSH_CONSTANT(DrawCompPush)

#include <utils/rand.glsl>
#include <utils/raytrace.glsl>

#define SKY_COL (f32vec3(0.02, 0.05, 0.90))
#define SKY_COL_B (f32vec3(0.08, 0.10, 0.54))

#define SUN_COL (f32vec3(1, 0.85, 0.5) * 2)
#define SUN_TIME (2)
#define SUN_DIR normalize(f32vec3(0.8 * sin(SUN_TIME), 2.3 * cos(SUN_TIME), sin(SUN_TIME)))
#define SUN_FACTOR (1 - pow(1 - max(sin(SUN_TIME), 0.0001), 2))

f32vec3 filmic(f32vec3 color) {
    color = max(color, f32vec3(0, 0, 0));
    color = (color * (6.2 * color + 0.5)) / (color * (6.2 * color + 1.7) + 0.06);
    return color;
}

f32vec3 filmic_inv(f32vec3 color) {
    color = max(color, f32vec3(0, 0, 0));
    color = (-sqrt(5.0) * sqrt(701.0 * color * color - 106.0 * color + 125.0) - 85 * color + 25) / (620 * (color - 1));
    return color;
}

f32vec3 sample_sky_ambient(f32vec3 nrm) {
    f32 sun_val = dot(nrm, SUN_DIR) * 0.25 + 0.5;
    sun_val = pow(sun_val, 2) * 0.2;
    f32 sky_val = clamp(dot(nrm, f32vec3(0, 0, -1)) * 0.5 + 0.5, 0, 1);
    return mix(SKY_COL + sun_val * SUN_COL, SKY_COL_B, pow(sky_val, 2)) * SUN_FACTOR;
}

f32vec3 sample_sky(f32vec3 nrm) {
    f32 sun_val = dot(nrm, SUN_DIR) * 0.5 + 0.5;
    sun_val = sun_val * 1000 - 999;
    sun_val = pow(clamp(sun_val * 1.5, 0, 1), 20);

    f32vec3 light = sample_sky_ambient(nrm);
    light += sun_val * SUN_COL;
    return light;
}

TraceRecord trace_scene(in Ray ray, in out i32 complexity) {
    complexity = 0;

    TraceRecord trace;
    default_init(trace.intersection_record);
    trace.color = sample_sky(ray.nrm);
    trace.material = 0;
    trace.object_i = 0;

    for (u32 i = 0; i < SCENE.sphere_n; ++i) {
        Sphere s = SCENE.spheres[i];
        IntersectionRecord s_hit = intersect(ray, s);
        if (s_hit.hit && s_hit.dist < trace.intersection_record.dist) {
            trace.intersection_record = s_hit;
            trace.color = f32vec3(0.5, 0.5, 0.5);
            trace.material = 1;
            trace.object_i = i;
        }
    }

    for (u32 i = 0; i < SCENE.box_n; ++i) {
        Box b = SCENE.boxes[i];
        IntersectionRecord b_hit = intersect(ray, b);
        if (b_hit.hit && b_hit.dist < trace.intersection_record.dist) {
            trace.intersection_record = b_hit;
            trace.color = f32vec3(0.1, 0.1, 0.1);
            trace.material = 2;
            trace.object_i = i;
        }
    }

    for (u32 i = 0; i < SCENE.capsule_n; ++i) {
        Capsule c = SCENE.capsules[i];
        IntersectionRecord c_hit = intersect(ray, c);
        if (c_hit.hit && c_hit.dist < trace.intersection_record.dist) {
            trace.intersection_record = c_hit;
            trace.color = f32vec3(0.5, 0.5, 0.5);
            trace.material = 3;
            trace.object_i = i;
        }
    }

    return trace;
}

layout(local_size_x = 8, local_size_y = 8, local_size_z = 1) in;
void main() {
    u32vec3 pixel_i = gl_GlobalInvocationID.xyz;
    if (pixel_i.x >= INPUT.frame_dim.x ||
        pixel_i.y >= INPUT.frame_dim.y)
        return;

    f32vec2 pixel_p = pixel_i.xy;

    f32 uv_rand_offset = INPUT.time;
    f32vec2 uv_offset = f32vec2(rand(pixel_p + uv_rand_offset + 10), rand(pixel_p + uv_rand_offset)) * 1.0 - 0.5;
    pixel_p += uv_offset * INPUT.settings.jitter_scl;

    f32vec2 frame_dim = INPUT.frame_dim;
    f32vec2 inv_frame_dim = f32vec2(1.0, 1.0) / frame_dim;
    f32 aspect = frame_dim.x * inv_frame_dim.y;
    f32vec2 uv = (pixel_p * inv_frame_dim - 0.5) * f32vec2(2 * aspect, 2);

    Ray view_ray = create_view_ray(uv);
    view_ray.inv_nrm = 1.0 / view_ray.nrm;

    f32vec3 col = f32vec3(0, 0, 0);

    i32 complexity;
    TraceRecord view_trace_record = trace_scene(view_ray, complexity);
    col = view_trace_record.color;

    if (view_trace_record.intersection_record.hit) {
        f32vec3 hit_pos = view_ray.o + view_ray.nrm * view_trace_record.intersection_record.dist;
        f32vec3 hit_nrm = view_trace_record.intersection_record.nrm;
        f32 hit_dist = view_trace_record.intersection_record.dist;

        f32 shade = max(dot(SUN_DIR, hit_nrm), 0.0);

        if (view_trace_record.material == 2 && view_trace_record.object_i == 0) {
            f32 val = f32((fract(hit_pos.x * 0.5) > 0.5) != (fract(hit_pos.y * 0.5) > 0.5));
            col = mix(f32vec3(0.4, 0.1, 0.05), f32vec3(0.5, 0.16, 0.1), val);
        }

        Ray bounce_ray;
        bounce_ray.o = hit_pos;
        bounce_ray.nrm = SUN_DIR;
        bounce_ray.o += hit_nrm * 0.001;
        bounce_ray.inv_nrm = 1.0 / bounce_ray.nrm;
        i32 temp_i32;
        TraceRecord bounce_trace_record = trace_scene(bounce_ray, temp_i32);
        shade *= max(f32(!bounce_trace_record.intersection_record.hit), 0.0);

        col *= SUN_COL * SUN_FACTOR * shade + sample_sky_ambient(hit_nrm);
    }

    imageStore(
        daxa_GetRWImage(image2D, rgba32f, push_constant.image_id),
        i32vec2(pixel_i.xy),
        f32vec4(filmic(col), 1));
}
