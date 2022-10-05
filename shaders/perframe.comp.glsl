#include <shared/shared.inl>

DAXA_USE_PUSH_CONSTANT(PerframeCompPush)

#define PLAYER_HEIGHT 1.8
#define PLAYER_HEAD_RADIUS (0.197 / 2)
#define COLLIDE_DELTA 0.09

#if 1 // REALISTIC
#define PLAYER_SPEED 1.5
#define PLAYER_SPRINT_MUL 2.5
#define PLAYER_ACCEL 10.0
#define EARTH_JUMP_HEIGHT 0.59
#else // MINECRAFT-LIKE
#define PLAYER_SPEED 4.3
#define PLAYER_SPRINT_MUL 1.3
#define PLAYER_ACCEL 30.0
#define EARTH_JUMP_HEIGHT 1.1
#endif

#define EARTH_GRAV 9.807
#define MOON_GRAV 1.625
#define MARS_GRAV 3.728
#define JUPITER_GRAV 25.93

#define GRAVITY EARTH_GRAV

void toggle_view() {
    PLAYER.view_state = (PLAYER.view_state & ~0xf) | ((PLAYER.view_state & 0xf) + 1);
    if ((PLAYER.view_state & 0xf) > 1)
        PLAYER.view_state = (PLAYER.view_state & ~0xf) | 0;
}
void toggle_fly() {
    u32 is_flying = (PLAYER.view_state >> 6) & 0x1;
    PLAYER.view_state = (PLAYER.view_state & ~(0x1 << 6)) | ((1 - is_flying) << 6);
}

f32vec3 view_vec() {
    switch (PLAYER.view_state & 0xf) {
    case 0: return PLAYER.forward * (PLAYER_HEAD_RADIUS + 0.001);
    case 1: return PLAYER.cam.rot_mat * f32vec3(0, -2, 0);
    default: return f32vec3(0, 0, 0);
    }
}

b32 get_flag(u32 index) {
    return ((INPUT.settings.flags >> index) & 0x01) == 0x01;
}

void perframe_player() {
    const f32 mouse_sens = 1.0;

    b32 is_flying = ((PLAYER.view_state >> 6) & 0x1) == 1;

    if (PLAYER.pos.z <= 0) {
        PLAYER.pos.z = 0;
        PLAYER.on_ground = true;
        PLAYER.force_vel.z = 0;
        PLAYER.move_vel.z = 0;
    }

    if (is_flying) {
        PLAYER.speed = PLAYER_SPEED * 30;
        PLAYER.accel_rate = PLAYER_ACCEL * 30;
        PLAYER.on_ground = true;
    } else {
        PLAYER.speed = PLAYER_SPEED;
        PLAYER.accel_rate = PLAYER_ACCEL;
        if (INPUT.keyboard.keys[GAME_KEY_JUMP] != 0 && PLAYER.on_ground)
            PLAYER.force_vel.z = EARTH_GRAV * sqrt(EARTH_JUMP_HEIGHT * 2.0 / EARTH_GRAV);
    }
    PLAYER.sprint_speed = 2.5;

    if (INPUT.keyboard.keys[GAME_KEY_CYCLE_VIEW] != 0) {
        if ((PLAYER.view_state & 0x10) == 0) {
            PLAYER.view_state |= 0x10;
            toggle_view();
        }
    } else {
        PLAYER.view_state &= ~0x10;
    }
    if (INPUT.keyboard.keys[GAME_KEY_TOGGLE_FLY] != 0) {
        if ((PLAYER.view_state & 0x20) == 0) {
            PLAYER.view_state |= 0x20;
            toggle_fly();
        }
    } else {
        PLAYER.view_state &= ~0x20;
    }

    PLAYER.rot.z += INPUT.mouse.pos_delta.x * mouse_sens * INPUT.settings.sensitivity * 0.001;
    PLAYER.rot.x -= INPUT.mouse.pos_delta.y * mouse_sens * INPUT.settings.sensitivity * 0.001;

    const float MAX_ROT = 1.57;
    if (PLAYER.rot.x > MAX_ROT)
        PLAYER.rot.x = MAX_ROT;
    if (PLAYER.rot.x < -MAX_ROT)
        PLAYER.rot.x = -MAX_ROT;
    float sin_rot_x = sin(PLAYER.rot.x), cos_rot_x = cos(PLAYER.rot.x);
    float sin_rot_y = sin(PLAYER.rot.y), cos_rot_y = cos(PLAYER.rot.y);
    float sin_rot_z = sin(PLAYER.rot.z), cos_rot_z = cos(PLAYER.rot.z);

    // clang-format off
    PLAYER.cam.rot_mat = 
        f32mat3x3(
            cos_rot_z, -sin_rot_z, 0,
            sin_rot_z,  cos_rot_z, 0,
            0,          0,         1
        ) *
        f32mat3x3(
            1,          0,          0,
            0,  cos_rot_x,  sin_rot_x,
            0, -sin_rot_x,  cos_rot_x
        );
    // clang-format on

    f32vec3 move_vec = f32vec3(0, 0, 0);
    PLAYER.forward = f32vec3(+sin_rot_z, +cos_rot_z, 0);
    PLAYER.lateral = f32vec3(+cos_rot_z, -sin_rot_z, 0);

    if (INPUT.keyboard.keys[GAME_KEY_MOVE_FORWARD] != 0)
        move_vec += PLAYER.forward;
    if (INPUT.keyboard.keys[GAME_KEY_MOVE_BACKWARD] != 0)
        move_vec -= PLAYER.forward;
    if (INPUT.keyboard.keys[GAME_KEY_MOVE_LEFT] != 0)
        move_vec -= PLAYER.lateral;
    if (INPUT.keyboard.keys[GAME_KEY_MOVE_RIGHT] != 0)
        move_vec += PLAYER.lateral;

    if (is_flying) {
        if (INPUT.keyboard.keys[GAME_KEY_JUMP] != 0)
            move_vec += f32vec3(0, 0, 1);
        if (INPUT.keyboard.keys[GAME_KEY_CROUCH] != 0)
            move_vec -= f32vec3(0, 0, 1);
    }

    f32 applied_accel = PLAYER.accel_rate;
    if (INPUT.keyboard.keys[GAME_KEY_SPRINT] != 0)
        PLAYER.max_speed += INPUT.delta_time * PLAYER.accel_rate;
    else
        PLAYER.max_speed -= INPUT.delta_time * PLAYER.accel_rate;
    PLAYER.max_speed = clamp(PLAYER.max_speed, PLAYER.speed, PLAYER.speed * PLAYER.sprint_speed);

    f32 move_magsq = dot(move_vec, move_vec);
    if (move_magsq > 0) {
        move_vec = normalize(move_vec) * INPUT.delta_time * applied_accel * (PLAYER.on_ground ? 1 : 0.1);
    }

    f32 move_vel_mag = length(PLAYER.move_vel);
    f32vec3 move_vel_dir;
    if (move_vel_mag > 0) {
        move_vel_dir = PLAYER.move_vel / move_vel_mag;
        if (PLAYER.on_ground)
            PLAYER.move_vel -= move_vel_dir * min(PLAYER.accel_rate * 0.4 * INPUT.delta_time, move_vel_mag);
    }
    PLAYER.move_vel += move_vec * 2;

    move_vel_mag = length(PLAYER.move_vel);
    if (move_vel_mag > 0) {
        PLAYER.move_vel = PLAYER.move_vel / move_vel_mag * min(move_vel_mag, PLAYER.max_speed);
    }

    if (is_flying) {
        PLAYER.force_vel = f32vec3(0, 0, 0);
    } else {
        PLAYER.force_vel += f32vec3(0, 0, -1) * GRAVITY * INPUT.delta_time;
    }

    f32vec3 vel = PLAYER.move_vel + PLAYER.force_vel;
    PLAYER.pos += vel * INPUT.delta_time;
    PLAYER.on_ground = false;

    f32vec3 cam_offset = view_vec();

    PLAYER.cam.pos = PLAYER.pos + f32vec3(0, 0, PLAYER_HEIGHT - PLAYER_HEAD_RADIUS);

    f32 cam_offset_len = length(cam_offset);

    PLAYER.cam.pos += cam_offset;
    PLAYER.cam.fov = INPUT.settings.fov * 3.14159 / 180.0;
    PLAYER.cam.tan_half_fov = tan(PLAYER.cam.fov * 0.5);

    SCENE.capsules[0].r = PLAYER_HEAD_RADIUS;
    SCENE.capsules[0].p0 = PLAYER.pos + f32vec3(0, 0, PLAYER_HEAD_RADIUS);
    SCENE.capsules[0].p1 = PLAYER.pos + f32vec3(0, 0, PLAYER_HEIGHT - PLAYER_HEAD_RADIUS);

    f32vec3 head_p = PLAYER.pos + f32vec3(0, 0, 1.7);
    f32vec3 shoulder_p = PLAYER.pos + f32vec3(0, 0, 1.5);
    f32vec3 chest_p = PLAYER.pos + f32vec3(0, 0, 1.4) + PLAYER.forward * 0.01;
    f32vec3 waist_p = PLAYER.pos + f32vec3(0, 0, 1.02);
    f32vec3 knee_p = PLAYER.pos + f32vec3(0, 0, 0.54);
    f32vec3 hand_p = PLAYER.pos + f32vec3(0, 0, 0.8);
    f32vec3 foot_p = PLAYER.pos;

    SCENE.capsules[0].r = PLAYER_HEAD_RADIUS;
    SCENE.capsules[0].p0 = head_p - f32vec3(0, 0, PLAYER_HEAD_RADIUS * 0.2);
    SCENE.capsules[0].p1 = foot_p + f32vec3(0, 0, PLAYER_HEAD_RADIUS);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
    perframe_player();
}
