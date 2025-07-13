#pragma once
#include "../glStuff.hpp"
#include "../globals.hpp"
#include <memory>

enum class BufferType {
    fg_tile,
    bg_tile,
    bunny,
    time_capsule,

    midground
};

struct Shaders {
    ShaderProgram flat     {"src/shaders/mvp.vs", "src/shaders/flat.fs"};
    ShaderProgram textured {"src/shaders/mvp.vs", "src/shaders/textured.fs"};
    ShaderProgram copy     {"src/shaders/raw.vs", "src/shaders/textured.fs"};

    ShaderProgram visibility      {"src/shaders/raw.vs", "src/shaders/visibility.fs"};
    ShaderProgram light           {"src/shaders/raw.vs", "src/shaders/light_blur.fs"};
    ShaderProgram blur            {"src/shaders/raw.vs", "src/shaders/blur.fs"};
    ShaderProgram visibility_mask {"src/shaders/raw.vs", "src/shaders/visibility_mask.fs"};
    ShaderProgram water           {"src/shaders/mvp.vs", "src/shaders/water.fs"};
    ShaderProgram waterfalls      {"src/shaders/raw.vs", "src/shaders/waterfalls.fs"};

    ShaderProgram merge_fg     {"src/shaders/raw.vs", "src/shaders/merge.fs"};
    ShaderProgram merge_bg     {"src/shaders/raw.vs", "src/shaders/merge_bg.fs"};
    ShaderProgram merge_bg_tex {"src/shaders/raw.vs", "src/shaders/merge_bg_tex.fs"};

    ShaderProgram bloom_darken {"src/shaders/raw.vs", "src/shaders/bloom_darken.fs"};
    ShaderProgram color_correction {"src/shaders/raw.vs", "src/shaders/color_correction.fs"};
};

struct Textures {
    Texture atlas;
    Texture background {320 * 4, 180 * 4};
    Texture bunny;
    Texture time_capsule;

    void update() {
        { // chroma key atlas texture
            auto tex = game_data.atlas.copy();
            // chroma key cyan and replace with alpha
            auto vptr = tex.data();
            for(int i = 0; i < tex.width() * tex.height(); ++i) {
                if(vptr[i] == 0xFFFFFF00) {
                    vptr[i] = 0;
                }
            }
            atlas.Load(tex);
        }

        bunny.Load(game_data.bunny);
        time_capsule.Load(game_data.time_capsule);

        background.Bind();

        background.LoadSubImage(320 * 0, 180 * 0, game_data.backgrounds[0]); // 13
        background.LoadSubImage(320 * 1, 180 * 0, game_data.backgrounds[1]); // 14
        background.LoadSubImage(320 * 2, 180 * 0, game_data.backgrounds[2]); // 7, 8
        background.LoadSubImage(320 * 3, 180 * 0, game_data.backgrounds[3]); // 1

        background.LoadSubImage(320 * 0, 180 * 1, game_data.backgrounds[4]); // 6
        background.LoadSubImage(320 * 1, 180 * 1, game_data.backgrounds[5]); // 9, 11
        background.LoadSubImage(320 * 2, 180 * 1, game_data.backgrounds[6]); // 10
        background.LoadSubImage(320 * 3, 180 * 1, game_data.backgrounds[7]); // 16

        background.LoadSubImage(320 * 0, 180 * 2, game_data.backgrounds[8]); // 4, 5
        background.LoadSubImage(320 * 1, 180 * 2, game_data.backgrounds[9]); // 15
        background.LoadSubImage(320 * 2, 180 * 2, game_data.backgrounds[10]); // 19
        background.LoadSubImage(320 * 3, 180 * 2, game_data.backgrounds[11]); // 2, 3

        background.LoadSubImage(320 * 0, 180 * 3, game_data.backgrounds[12]); // 17
        background.LoadSubImage(320 * 1, 180 * 3, game_data.backgrounds[13]); // 18
        background.LoadSubImage(320 * 2, 180 * 3, game_data.backgrounds[14]); // 12
    }
};

struct Waterfall {
    glm::ivec2 pos, size;
    int layer;
};

struct RoomBuffers {
    // Mesh mesh;
    std::vector<glm::ivec3> lights;
    std::vector<Waterfall> waterfalls;
    // Mesh waterfall_mesh;
};

struct RenderData {
    Shaders shaders;
    Textures textures;

    Mesh fg_tiles, bg_tiles, bg_text, bg_normals, overlay;
    Mesh bunny;
    Mesh time_capsule;
    Mesh waterfall_mesh;

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
    bool sprite_composition = false;

    bool accurate_render = false;

    // Textured_Framebuffer visibility_buffer;
    Mesh visibility;
    Mesh mg_tiles;
    Mesh water;
    Mesh lights;

    Textured_Framebuffer small_light_buffer {128, 128};

    Textured_Framebuffer fg_buffer {0, 0};
    Textured_Framebuffer mg_buffer {0, 0};
    Textured_Framebuffer bg_buffer {0, 0};
    Textured_Framebuffer bg_normals_buffer {0, 0};
    Textured_Framebuffer bg_tex_buffer {0, 0};

    Textured_Framebuffer visibility_buffer {0, 0};

    Textured_Framebuffer light_buffer {0, 0};
    Textured_Framebuffer temp_buffer {0, 0};
    Textured_Framebuffer room_temp_buffer {320, 176};

    std::vector<RoomBuffers> room_buffers;

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
            case BufferType::fg_tile: return {fg_tiles, textures.atlas};
            case BufferType::bg_tile: return {bg_tiles, textures.atlas};
            case BufferType::midground: return {fg_tiles, textures.atlas}; // todo add midground buffer
            case BufferType::bunny: return {bunny, textures.bunny};
            case BufferType::time_capsule: return {time_capsule, textures.time_capsule};
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
