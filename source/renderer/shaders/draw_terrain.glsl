#define DAXA_ENABLE_SHADER_NO_NAMESPACE 1
#include <shared/shared.inl>
#extension GL_EXT_debug_printf : enable

DAXA_USE_PUSH_CONSTANT(DrawTerrainPC)
daxa_BufferPtr(TerrainVertex) terrain_vertices = daxa_push_constant.vertices;
daxa_BufferPtr(CameraParameters) camera_parameters = daxa_push_constant.camera_parameters;

#if DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_VERTEX
void main()
{
    const f32vec4 pre_trans_pos = f32vec4(deref(terrain_vertices[gl_VertexIndex]).position, 1.0);
    gl_Position = pre_trans_pos;
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TESSELATION_CONTROL
layout (vertices = 3) out;

void main()
{
    if(gl_InvocationID == 0)
    {
        f32vec4 scaled_pos_0 = f32vec4(gl_in[0].gl_Position.xyz * daxa_push_constant.terrain_scale, 1.0);
        f32vec4 scaled_pos_1 = f32vec4(gl_in[1].gl_Position.xyz * daxa_push_constant.terrain_scale, 1.0);
        f32vec4 scaled_pos_2 = f32vec4(gl_in[2].gl_Position.xyz * daxa_push_constant.terrain_scale, 1.0);
        f32vec3 view_scale_pos_0 = (deref(camera_parameters).view * scaled_pos_0).xyz;
        f32vec3 view_scale_pos_1 = (deref(camera_parameters).view * scaled_pos_1).xyz;
        f32vec3 view_scale_pos_2 = (deref(camera_parameters).view * scaled_pos_2).xyz;
        f32 depth_0 = clamp((abs(view_scale_pos_1.z + view_scale_pos_2.z / 2.0) - daxa_push_constant.min_depth) / daxa_push_constant.delta, 0.0, 1.0);
        f32 depth_1 = clamp((abs(view_scale_pos_2.z + view_scale_pos_0.z / 2.0) - daxa_push_constant.min_depth) / daxa_push_constant.delta, 0.0, 1.0);
        f32 depth_2 = clamp((abs(view_scale_pos_0.z + view_scale_pos_1.z / 2.0) - daxa_push_constant.min_depth) / daxa_push_constant.delta, 0.0, 1.0);

        f32 avg_depth = clamp((depth_0 + depth_1 + depth_2)/ 3.0, 0.0, 1.0);
        gl_TessLevelInner[0] = mix(daxa_push_constant.max_tess_level, daxa_push_constant.min_tess_level, sqrt(avg_depth));

        gl_TessLevelOuter[0] = mix(daxa_push_constant.max_tess_level, daxa_push_constant.min_tess_level, sqrt(depth_0));
        gl_TessLevelOuter[1] = mix(daxa_push_constant.max_tess_level, daxa_push_constant.min_tess_level, sqrt(depth_1));
        gl_TessLevelOuter[2] = mix(daxa_push_constant.max_tess_level, daxa_push_constant.min_tess_level, sqrt(depth_2));
        // debugPrintfEXT("Tess level inner %f, Tess level outer %f %f %f\n", gl_TessLevelInner[0], gl_TessLevelOuter[0], gl_TessLevelOuter[1], gl_TessLevelOuter[2] );
        // debugPrintfEXT("Avg depth %f, individual depth %f %f %f\n", avg_depth, depth_0, depth_1, depth_2);
    }
    gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_TESSELATION_EVALUATION
layout (triangles, fractional_odd_spacing , cw) in;

void main()
{
    gl_Position = (gl_TessCoord.x * gl_in[0].gl_Position) +
                  (gl_TessCoord.y * gl_in[1].gl_Position) +
                  (gl_TessCoord.z * gl_in[2].gl_Position);

    f32vec3 scale = daxa_push_constant.terrain_scale;
    const f32vec4 pre_trans_scaled_pos = f32vec4(gl_Position.xyz * scale, 1.0);

    f32mat4x4 m_proj_view_model = deref(camera_parameters).projection * deref(camera_parameters).view;
    gl_Position = m_proj_view_model * pre_trans_scaled_pos;
}
#elif DAXA_SHADER_STAGE == DAXA_SHADER_STAGE_FRAGMENT
layout (location = 0) out f32vec4 out_color;

void main()
{
    out_color = f32vec4(1.0, 0.0, 0.0, 1.0);
}
#endif