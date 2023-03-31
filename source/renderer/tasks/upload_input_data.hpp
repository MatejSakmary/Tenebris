#pragma once

#include <daxa/daxa.hpp>
#include <daxa/utils/task_list.hpp>

#include "../context.hpp"
#include "../shared/shared.inl"

inline void task_upload_input_data(Context & context)
{
    context.main_task_list.task_list.add_task({
        .used_buffers = 
        { 
            { 
                context.main_task_list.task_buffers.t_atmosphere_parameters,
                daxa::TaskBufferAccess::HOST_TRANSFER_WRITE
            },
            { 
                context.main_task_list.task_buffers.t_camera_parameters,
                daxa::TaskBufferAccess::HOST_TRANSFER_WRITE
            },
            { 
                context.main_task_list.task_buffers.t_poisson_header,
                daxa::TaskBufferAccess::HOST_TRANSFER_WRITE
            },
        },
        .task = [&](daxa::TaskRuntimeInterface const & runtime)
        {
            auto cmd_list = runtime.get_command_list();

            auto atmosphere_parameters_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_atmosphere_parameters)[0];
            auto camera_parameters_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_camera_parameters)[0];
            auto poisson_header_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_poisson_header)[0];
            // create staging buffer
            auto staging_atmosphere_parameters_gpu_buffer = context.device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = sizeof(AtmosphereParameters),
                .debug_name = "staging atmosphere gpu buffer"
            });
            // copy data into staging buffer
            auto buffer_ptr = context.device.get_host_address_as<AtmosphereParameters>(staging_atmosphere_parameters_gpu_buffer);
            memcpy(buffer_ptr, &context.buffers.atmosphere_parameters.cpu_buffer, sizeof(AtmosphereParameters));
            // copy staging buffer into gpu_buffer
            cmd_list.copy_buffer_to_buffer({
                .src_buffer = staging_atmosphere_parameters_gpu_buffer,
                .dst_buffer = atmosphere_parameters_gpu_buffer,
                .size = sizeof(AtmosphereParameters),
            });
            // destroy the staging buffer after the copy is done
            cmd_list.destroy_buffer_deferred(staging_atmosphere_parameters_gpu_buffer);

            auto poisson_header_staging_buffer = context.device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = sizeof(PoissonHeader),
                .debug_name = "staging poisson header gpu buffer"
            });
            auto buffer_ptr__ = context.device.get_host_address_as<PoissonHeader>(poisson_header_staging_buffer);
            memcpy(buffer_ptr__, &context.buffers.poisson_header.cpu_buffer, sizeof(PoissonHeader));
            cmd_list.copy_buffer_to_buffer({
                .src_buffer = poisson_header_staging_buffer,
                .dst_buffer = poisson_header_gpu_buffer,
                .size = sizeof(PoissonHeader),
            });
            cmd_list.destroy_buffer_deferred(poisson_header_staging_buffer);

            auto staging_camera_parameters_gpu_buffer = context.device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = sizeof(CameraParameters),
                .debug_name = "staging camera gpu buffer"
            });
            // copy data into staging buffer
            auto buffer_ptr_ = context.device.get_host_address_as<CameraParameters>(staging_camera_parameters_gpu_buffer);
            memcpy(buffer_ptr_, &context.buffers.camera_parameters.cpu_buffer, sizeof(CameraParameters));
            // copy staging buffer into gpu_buffer
            cmd_list.copy_buffer_to_buffer({
                .src_buffer = staging_camera_parameters_gpu_buffer,
                .dst_buffer = camera_parameters_gpu_buffer,
                .size = sizeof(CameraParameters),
            });
            // destroy the staging buffer after the copy is done
            cmd_list.destroy_buffer_deferred(staging_camera_parameters_gpu_buffer);
        },
        .debug_name = "upload atmosphere and camera params"
    });

    context.main_task_list.task_list.conditional({
        .condition_index = 0,
        .when_true = [&]() -> void
        {
            context.main_task_list.task_list.add_task({
                .used_buffers = 
                { 
                    { 
                        context.main_task_list.task_buffers.t_terrain_vertices,
                        daxa::TaskBufferAccess::HOST_TRANSFER_WRITE
                    },
                    { 
                        context.main_task_list.task_buffers.t_terrain_indices,
                        daxa::TaskBufferAccess::HOST_TRANSFER_WRITE
                    } 
                },
                .task = [&](daxa::TaskRuntimeInterface const & runtime)
                {
                    auto cmd_list = runtime.get_command_list();

                    auto terrain_vertices_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_terrain_vertices)[0];
                    auto terrain_indices_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_terrain_indices)[0];
                    u32 vertex_buffer_size = u32(sizeof(TerrainVertex) * context.buffers.terrain_vertices.cpu_buffer.size());
                    auto staging_vertices_gpu_buffer = context.device.create_buffer({
                        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                        .size = vertex_buffer_size,
                        .debug_name = "staging vertices gpu buffer"
                    });
                    auto * vertices_buffer_ptr = context.device.get_host_address_as<TerrainVertex>(staging_vertices_gpu_buffer);
                    memcpy(vertices_buffer_ptr, context.buffers.terrain_vertices.cpu_buffer.data(), vertex_buffer_size);
                    cmd_list.copy_buffer_to_buffer({
                        .src_buffer = staging_vertices_gpu_buffer,
                        .dst_buffer = terrain_vertices_gpu_buffer,
                        .size = vertex_buffer_size,
                    });

                    cmd_list.destroy_buffer_deferred(staging_vertices_gpu_buffer);

                    u32 index_buffer_size = u32(sizeof(TerrainIndex) * context.buffers.terrain_indices.cpu_buffer.size());
                    auto staging_indices_gpu_buffer = context.device.create_buffer({
                        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                        .size = index_buffer_size,
                        .debug_name = "staging index gpu buffer"
                    });
                    auto indices_buffer_ptr = context.device.get_host_address_as<TerrainIndex>(staging_indices_gpu_buffer);
                    memcpy(indices_buffer_ptr, context.buffers.terrain_indices.cpu_buffer.data(), index_buffer_size);
                    cmd_list.copy_buffer_to_buffer({
                        .src_buffer = staging_indices_gpu_buffer,
                        .dst_buffer = terrain_indices_gpu_buffer,
                        .size = index_buffer_size,
                    });
                    cmd_list.destroy_buffer_deferred(staging_indices_gpu_buffer);
                    context.conditionals.at(Context::Conditionals::COPY_PLANET_GEOMETRY) = false;
                },
                .debug_name = "upload terrain geometry data"
            });
        },
    });
}