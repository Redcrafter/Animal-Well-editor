#pragma once

#include <memory>
#include <span>
#include <unordered_map>

#include "glStuff.hpp"

#include "structures/map.hpp"
#include "structures/sprite.hpp"
#include "structures/tile.hpp"

enum class BufferType {
    fg_tile,
    bg_tile,
    bunny,
    time_capsule
};

struct RenderData {
    ShaderProgram flat_shader {"src/shaders/flat.vs", "src/shaders/flat.fs"};
    ShaderProgram textured_shader {"src/shaders/textured.vs", "src/shaders/textured.fs"};

    Mesh fg_tiles, bg_tiles, bg_text, overlay;
    Mesh bunny;
    Mesh time_capsule;

    Texture atlas;
    Texture bg_tex {320 * 4, 180 * 4};
    Texture bunny_tex;
    Texture time_capsule_tex;

    glm::vec4 bg_color {0.8, 0.8, 0.8, 1};
    glm::vec4 fg_color {1, 1, 1, 1};
    glm::vec4 bg_tex_color {0.5, 0.5, 0.5, 1};

    std::vector<BufferType> type_stack;

    bool show_fg = true;
    bool show_bg = true;
    bool show_bg_tex = true;

    bool room_grid = false;
    bool show_water = false;
    bool accurate_vines = true;

    RenderData() = default;
    RenderData(const RenderData&) = delete;
    void operator=(const RenderData&) = delete;

    void push_type(BufferType type) {
        type_stack.push_back(type);
    }
    void pop_type() {
        type_stack.pop_back();
    }

    std::tuple<Mesh&, Texture&> get_current() {
        switch(type_stack.back()) {
            case BufferType::fg_tile: return {fg_tiles, atlas};
            case BufferType::bg_tile: return {bg_tiles, atlas};
            case BufferType::bunny: return {bunny, bunny_tex};
            case BufferType::time_capsule: return {time_capsule, time_capsule_tex};
            default:
                assert(false);
                throw std::runtime_error("unreachable");
        }
    }

    void add_face(glm::vec2 p_min, glm::vec2 p_max, glm::ivec2 uv_min, glm::ivec2 uv_max, uint32_t col = IM_COL32_WHITE) {
        auto [mesh, tex] = get_current();
        glm::vec2 tex_size {tex.width, tex.height};
        mesh.AddRectFilled(p_min, p_max, glm::vec2(uv_min) / tex_size, glm::vec2(uv_max) / tex_size, col);
    }
};

inline std::unique_ptr<RenderData> render_data;

void renderMap(const Map& map, std::span<const uv_data> uvs, std::unordered_map<uint32_t, SpriteData>& sprites);

void renderBgs(const Map& map);
