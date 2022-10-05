#pragma once

#include <shared/user_input.inl>
#include <shared/player.inl>
#include <shared/scene.inl>

DAXA_DECL_BUFFER_STRUCT(GpuInput, {
    u32vec2 frame_dim;
    f32 time;
    f32 delta_time;
    Settings settings;
    MouseInput mouse;
    KeyboardInput keyboard;
});
DAXA_DECL_BUFFER_STRUCT(GpuGlobals, {
    Player player;
    Scene scene;
});

struct StartupCompPush {
    BufferRef(GpuGlobals) gpu_globals;
};
struct PerframeCompPush {
    BufferRef(GpuGlobals) gpu_globals;
    BufferRef(GpuInput) gpu_input;
};
struct DrawCompPush {
    BufferRef(GpuGlobals) gpu_globals;
    BufferRef(GpuInput) gpu_input;

    ImageViewId image_id;
};

#define GLOBALS push_constant.gpu_globals
#define SCENE push_constant.gpu_globals.scene
#define INPUT push_constant.gpu_input
#define PLAYER push_constant.gpu_globals.player
