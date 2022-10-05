#pragma once

#include <daxa/daxa.inl>

struct Camera {
    f32mat3x3 rot_mat;
    f32vec3 pos;
    f32 fov, tan_half_fov;
};

struct Player {
    Camera cam;
    f32vec3 pos, vel;
    f32vec3 rot;
    f32vec3 forward, lateral;

    f32vec3 move_vel, force_vel;
    f32 accel_rate, speed, max_speed, sprint_speed;

    u32 view_state;
    b32 on_ground; // TODO: use flags
};
