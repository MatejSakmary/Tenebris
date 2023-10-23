
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
    daxa_i32vec2 up_pos    = clamp(daxa_i32vec2(gl_GlobalInvocationID.xy) + daxa_i32vec2(0, 1) , daxa_i32vec2(0, 0), daxa_i32vec2(pc.texture_size) - daxa_i32vec2(1, 1));
    daxa_i32vec2 down_pos  = clamp(daxa_i32vec2(gl_GlobalInvocationID.xy) + daxa_i32vec2(0, -1), daxa_i32vec2(0, 0), daxa_i32vec2(pc.texture_size) - daxa_i32vec2(1, 1));
    daxa_i32vec2 right_pos = clamp(daxa_i32vec2(gl_GlobalInvocationID.xy) + daxa_i32vec2(1, 0) , daxa_i32vec2(0, 0), daxa_i32vec2(pc.texture_size) - daxa_i32vec2(1, 1));
    daxa_i32vec2 left_pos  = clamp(daxa_i32vec2(gl_GlobalInvocationID.xy) + daxa_i32vec2(-1, 0), daxa_i32vec2(0, 0), daxa_i32vec2(pc.texture_size) - daxa_i32vec2(1, 1));
    // daxa_f32 sample_up    = imageLoad(daxa_image2D(_height_texture), up_pos).r / 4332.0;    
    // daxa_f32 sample_down  = imageLoad(daxa_image2D(_height_texture), down_pos).r / 4332.0;    
    // daxa_f32 sample_right = imageLoad(daxa_image2D(_height_texture), right_pos).r/ 4332.0;    
    // daxa_f32 sample_left  = imageLoad(daxa_image2D(_height_texture), left_pos).r / 4332.0;    
    daxa_f32 sample_up    = imageLoad(daxa_image2D(_height_texture), up_pos).r;    
    daxa_f32 sample_down  = imageLoad(daxa_image2D(_height_texture), down_pos).r;    
    daxa_f32 sample_right = imageLoad(daxa_image2D(_height_texture), right_pos).r;    
    daxa_f32 sample_left  = imageLoad(daxa_image2D(_height_texture), left_pos).r;    

    daxa_f32vec3 norm_pos_up = daxa_f32vec3(daxa_f32vec2(up_pos) / daxa_f32vec2(pc.texture_size), sample_up);
    daxa_f32vec3 norm_pos_down = daxa_f32vec3(daxa_f32vec2(down_pos) / daxa_f32vec2(pc.texture_size), sample_down);
    daxa_f32vec3 norm_pos_right = daxa_f32vec3(daxa_f32vec2(right_pos) / daxa_f32vec2(pc.texture_size), sample_right);
    daxa_f32vec3 norm_pos_left = daxa_f32vec3(daxa_f32vec2(left_pos) / daxa_f32vec2(pc.texture_size), sample_left);

    daxa_f32vec3 vertical_dir = normalize(norm_pos_up - norm_pos_down);
    daxa_f32vec3 horizontal_dir = normalize(norm_pos_right - norm_pos_left);

    daxa_f32vec3 normal = normalize(cross(horizontal_dir, vertical_dir));

    imageStore(daxa_image2D(_normal_texture), daxa_i32vec2(gl_GlobalInvocationID.xy), daxa_f32vec4(normal, 1.0));
}