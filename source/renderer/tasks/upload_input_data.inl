#pragma once

#include <daxa/daxa.inl>
#include <daxa/utils/task_list.inl>

#include "../shared/shared.inl"

DAXA_INL_TASK_USE_BEGIN(UpdateParametersTaskBase, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_BUFFER(_atmosphere_parameters, daxa_BufferPtr(AtmosphereParameters), HOST_TRANSFER_WRITE)
DAXA_INL_TASK_USE_BUFFER(_camera_parameters, daxa_BufferPtr(CameraParameters), HOST_TRANSFER_WRITE)
DAXA_INL_TASK_USE_END()

DAXA_INL_TASK_USE_BEGIN(UpdateGeometryTaskBase, DAXA_CBUFFER_SLOT0)
DAXA_INL_TASK_USE_BUFFER(_terrain_vertices, daxa_BufferPtr(TerrainVertex), HOST_TRANSFER_WRITE)
DAXA_INL_TASK_USE_BUFFER(_terrain_indices, daxa_BufferPtr(TerrainIndex), HOST_TRANSFER_WRITE)
DAXA_INL_TASK_USE_END()

#if __cplusplus
#include "../context.hpp"

struct TransferBufferInfo
{
    daxa::TaskInterface & ti;
    void * src;
    daxa::BufferId dst;
    u32 size;
};

static bool transfer_buffer(TransferBufferInfo const & info)
{
    auto cmd_list = info.ti.get_command_list();
    auto staging_mem_res = info.ti.get_allocator().allocate(info.size);
    if(!staging_mem_res.has_value()) { return false; }
    auto staging_mem = staging_mem_res.value();

    memcpy(staging_mem.host_address, info.src, info.size);
    cmd_list.copy_buffer_to_buffer({
        .src_buffer = info.ti.get_allocator().get_buffer(),
        .src_offset = staging_mem.buffer_offset,
        .dst_buffer = info.dst,
        .size = info.size,
    });

    return true;
}

struct UpdateParametersTask : UpdateParametersTaskBase
{
    Context *context = {};

    void callback(daxa::TaskInterface ti)
    {
        auto atmo_res = transfer_buffer({ 
            .ti = ti,
            .src = &(context->buffers.atmosphere_parameters.cpu_buffer),
            .dst = uses._atmosphere_parameters.buffer(),
            .size = sizeof(AtmosphereParameters)
        });
        if(!atmo_res) { throw std::runtime_error("UpdateParametersTask - Failed to allocate Atmosphere parameters staging buffer"); }

        auto camera_res = transfer_buffer({ 
            .ti = ti,
            .src = &(context->buffers.camera_parameters.cpu_buffer),
            .dst = uses._camera_parameters.buffer(),
            .size = sizeof(CameraParameters)
        });
        if(!camera_res) { throw std::runtime_error("UpdateParametersTask - Failed to allocate Camera parameters staging buffer"); }
    }
};

struct UpdateGeometryTask : UpdateGeometryTaskBase
{
    Context *context = {};

    void callback(daxa::TaskInterface ti)
    {
        auto cmd_list = ti.get_command_list();

        u32 vertex_buffer_size = u32(sizeof(TerrainVertex) * context->buffers.terrain_vertices.cpu_buffer.size());
        auto vertex_res = transfer_buffer({ 
            .ti = ti,
            .src = context->buffers.terrain_vertices.cpu_buffer.data(),
            .dst = uses._terrain_vertices.buffer(),
            .size = vertex_buffer_size
        });
        if(!vertex_res) { throw std::runtime_error("UpdateGeometryTask - Failed to allocate vertex staging buffer"); }

        u32 index_buffer_size = u32(sizeof(TerrainIndex) * context->buffers.terrain_indices.cpu_buffer.size());
        auto index_res = transfer_buffer({ 
            .ti = ti,
            .src = context->buffers.terrain_indices.cpu_buffer.data(),
            .dst = uses._terrain_indices.buffer(),
            .size = index_buffer_size
        });
        if(!index_res) { throw std::runtime_error("UpdateGeometryTask - Failed to allocate index staging buffer"); }

        context->conditionals.at(Context::Conditionals::COPY_PLANET_GEOMETRY) = false;
    }
};
#endif