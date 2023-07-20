#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#define DAXA_ENABLE_IMAGE_OVERLOADS_BASIC 1
#include <shared/shared.inl>
#include "tasks/debug_draw_frustum.inl"

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX
layout (location = 0) out f32vec3 color;
void main()
{
    const u32 frustum_vertex_count = 8;
    u32 offset = frustum_vertex_count * gl_InstanceIndex;
    f32vec3 pos = deref(_frustum_vertices[gl_VertexIndex + offset]).vertex;
    f32vec4 pre_trans_offset_pos = f32vec4(pos.xyz + deref(_globals).offset.xyz, 1.0);

    f32mat4x4 m_proj_view_model = deref(_globals).projection * deref(_globals).view;
    gl_Position = m_proj_view_model * pre_trans_offset_pos;
    color = deref(_frustum_colors[gl_InstanceIndex]).color;
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT
layout (location = 0) in f32vec3 color;
layout (location = 0) out f32vec4 albedo_out;
layout (location = 1) out f32vec4 normal_out;

void main()
{
    albedo_out = f32vec4(color, 1.0);
    normal_out = f32vec4(deref(_globals).sun_direction, 1.0);
}
#endif