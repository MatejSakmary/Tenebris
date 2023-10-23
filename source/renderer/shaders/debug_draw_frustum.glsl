#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/debug_draw_frustum.inl"

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX
layout (location = 0) out daxa_f32vec3 color;
void main()
{
    const daxa_u32 instance = gl_InstanceIndex; 
    const daxa_u32 offset = instance * FRUSTUM_VERTEX_COUNT;
    const daxa_u32 vertex_index = gl_VertexIndex + offset;

    const daxa_f32vec3 position = deref(_frustum_vertices[vertex_index]).vertex;
    const daxa_f32vec4 offset_position = daxa_f32vec4(position.xyz + deref(_globals).offset.xyz, 1.0);

    const daxa_f32mat4x4 projection_view = deref(_globals).projection * deref(_globals).view;

    color = deref(_frustum_colors[instance]).color;
    gl_Position = projection_view * offset_position;

}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT
layout (location = 0) in daxa_f32vec3 color;
layout (location = 0) out daxa_f32vec4 color_out;

void main()
{
    color_out = daxa_f32vec4(color, 1.0);
}
#endif