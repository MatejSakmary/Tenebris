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
            } 
        },
        .task = [&](daxa::TaskRuntime const & runtime)
        {
            auto cmd_list = runtime.get_command_list();

            auto atmosphere_parameters_gpu_buffer = runtime.get_buffers(context.main_task_list.task_buffers.t_atmosphere_parameters)[0];
            // create staging buffer
            auto staging_atmosphere_parameters_gpu_buffer = context.device.create_buffer({
                .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_RANDOM,
                .size = sizeof(AtmosphereParameters),
                .debug_name = "staging_atmosphere_gpu_buffer"
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
        },
        .debug_name = "upload input"
    }); 
}