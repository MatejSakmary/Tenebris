#include <shared/shared.inl>

DAXA_USE_PUSH_CONSTANT(StartupCompPush)

#include <utils/math.glsl>

void startup_player() {
    f32 p_offset = 0;
    PLAYER.pos = f32vec3(0, 0, 0) + 0.001;
    PLAYER.vel = f32vec3(0, 0, 0);
    PLAYER.rot = f32vec3(0, 0, 0);

    // PLAYER.view_state = (PLAYER.view_state & ~(0x1 << 6)) | (1 << 6);
}

layout(local_size_x = 1, local_size_y = 1, local_size_z = 1) in;
void main() {
    startup_player();

    SCENE.capsule_n++;

    SCENE.spheres[SCENE.sphere_n].o = f32vec3(-1.0, 3.0, 0.5);
    SCENE.spheres[SCENE.sphere_n].r = 0.5;
    SCENE.sphere_n++;

    SCENE.boxes[SCENE.box_n].bound_min = f32vec3(-10000, -10000, -1);
    SCENE.boxes[SCENE.box_n].bound_max = f32vec3(+10000, +10000, +0);
    SCENE.box_n++;

    SCENE.boxes[SCENE.box_n].bound_min = f32vec3(0.5, 2.5, 0.0);
    SCENE.boxes[SCENE.box_n].bound_max = f32vec3(1.5, 3.5, 1.0);
    SCENE.box_n++;
}
