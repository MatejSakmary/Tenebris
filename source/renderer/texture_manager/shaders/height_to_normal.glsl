#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "texture_manager/tasks/height_to_normal.inl"

#extension GL_EXT_debug_printf : enable

DAXA_DECL_PUSH_CONSTANT(HeightToNormalPC, pc)

layout (local_size_x = 8, local_size_y = 4) in;

void main()
{
	if(!all(lessThan(gl_GlobalInvocationID.xy, pc.texture_size)))
    {
        return;
    }
    i32vec2 up_pos    = clamp(i32vec2(gl_GlobalInvocationID.xy) + i32vec2(0, 1) , i32vec2(0, 0), i32vec2(pc.texture_size) - i32vec2(1, 1));
    i32vec2 down_pos  = clamp(i32vec2(gl_GlobalInvocationID.xy) + i32vec2(0, -1), i32vec2(0, 0), i32vec2(pc.texture_size) - i32vec2(1, 1));
    i32vec2 right_pos = clamp(i32vec2(gl_GlobalInvocationID.xy) + i32vec2(1, 0) , i32vec2(0, 0), i32vec2(pc.texture_size) - i32vec2(1, 1));
    i32vec2 left_pos  = clamp(i32vec2(gl_GlobalInvocationID.xy) + i32vec2(-1, 0), i32vec2(0, 0), i32vec2(pc.texture_size) - i32vec2(1, 1));
    // f32 sample_up    = imageLoad(daxa_image2D(_height_texture), up_pos).r / 4332.0;    
    // f32 sample_down  = imageLoad(daxa_image2D(_height_texture), down_pos).r / 4332.0;    
    // f32 sample_right = imageLoad(daxa_image2D(_height_texture), right_pos).r/ 4332.0;    
    // f32 sample_left  = imageLoad(daxa_image2D(_height_texture), left_pos).r / 4332.0;    
    f32 sample_up    = imageLoad(daxa_image2D(_height_texture), up_pos).r;    
    f32 sample_down  = imageLoad(daxa_image2D(_height_texture), down_pos).r;    
    f32 sample_right = imageLoad(daxa_image2D(_height_texture), right_pos).r;    
    f32 sample_left  = imageLoad(daxa_image2D(_height_texture), left_pos).r;    

    f32vec3 norm_pos_up = f32vec3(f32vec2(up_pos) / f32vec2(pc.texture_size), sample_up);
    f32vec3 norm_pos_down = f32vec3(f32vec2(down_pos) / f32vec2(pc.texture_size), sample_down);
    f32vec3 norm_pos_right = f32vec3(f32vec2(right_pos) / f32vec2(pc.texture_size), sample_right);
    f32vec3 norm_pos_left = f32vec3(f32vec2(left_pos) / f32vec2(pc.texture_size), sample_left);

    f32vec3 vertical_dir = normalize(norm_pos_up - norm_pos_down);
    f32vec3 horizontal_dir = normalize(norm_pos_right - norm_pos_left);

    f32vec3 normal = normalize(cross(horizontal_dir, vertical_dir));

    imageStore(daxa_image2D(_normal_texture), i32vec2(gl_GlobalInvocationID.xy), f32vec4(normal, 1.0));
}