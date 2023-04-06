#pragma once
#include <string>
#include <vector>
#include <variant>

#include <daxa/daxa.hpp>
using namespace daxa::types;

struct LoadTextureInfo
{
    std::string path = {};
    daxa::Device & device;
};

struct ManagedTextureHandle
{
    i32 index = -1;
};

struct ManagedTexture
{
    daxa::BufferId id = {};
    std::string path = {};
    i32vec2 dimensions = {-1, -1};
    daxa::Format format = daxa::Format::UNDEFINED;
};

struct TextureManager
{
    auto load_texture(const LoadTextureInfo & info) -> ManagedTextureHandle;
    auto reload_textures() -> std::vector<ManagedTextureHandle>;
    auto get_info(const ManagedTextureHandle & handle) const -> const ManagedTexture &;

  private:
    std::vector<ManagedTexture> managed_textures;
};
