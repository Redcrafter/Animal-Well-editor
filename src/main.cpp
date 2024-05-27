#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <numbers>
#include <random>
#include <set>
#include <span>
#include <unordered_map>

#include "glStuff.hpp" // has to be included before glfw
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <imgui_internal.h>

#include "structures/asset.hpp"
#include "structures/entity.hpp"
#include "structures/map.hpp"
#include "structures/tile.hpp"

#include "dos_parser.hpp"

#include "windows/errors.hpp"

#include "nfd.h"

// TODO:
// reduce global state
// move windows into separate files
// improve lighting

GLFWwindow* window;

float gScale = 1;
// Camera matrix
glm::mat4 view = glm::lookAt(
    glm::vec3(0, 0, 3), // Camera is at (0, 0, 3), in World Space
    glm::vec3(0, 0, 0), // and looks at the origin
    glm::vec3(0, 1, 0)  // Head is up
);
glm::mat4 projection;
glm::mat4 MVP;

glm::vec2 mousePos = glm::vec2(-1);

glm::vec2 screenSize;

std::vector<Textured_Framebuffer*> framebuffers;

std::unique_ptr<VBO> VBO_fg;
std::unique_ptr<VBO> VBO_bg;
std::unique_ptr<VBO> VBO_bg_tex;
std::unique_ptr<VBO> VBO_light;
std::unique_ptr<VBO> VBO_selection;

int selectedMap = 0;

std::vector<char> rawData;

static const char* mapNames[5] = {"Overworld", "CE temple", "Space", "Bunny temple", "Time Capsule"};
constexpr int mapIds[5] = {300, 157, 193, 52, 222};

Map maps[5] {};
std::unordered_map<uint32_t, SpriteData> sprites;
std::vector<uv_data> uvs;

std::unique_ptr<Texture> atlas;
std::unique_ptr<Texture> bg_tex;

std::vector<glm::vec4> vertecies_fg;
std::vector<glm::vec4> vertecies_bg;
std::vector<std::pair<glm::vec2, glm::vec3>> vertecies_bg_tex;
std::vector<std::pair<glm::vec2, glm::vec3>> vertecies_light;
std::vector<std::pair<glm::vec2, glm::vec4>> selectionVerts;

glm::vec4 bg_color {0.8, 0.8, 0.8, 1};
glm::vec4 fg_color {1, 1, 1, 1};
glm::vec4 bg_tex_color {0.5, 0.5, 0.5, 1};

bool show_fg = true;
bool show_bg = true;
bool show_bg_tex = true;
bool do_lighting = false;
bool room_grid = false;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    glm::vec2 newPos {xpos, ypos};
    if(mousePos == newPos)
        return;

    if(mousePos == glm::vec2(-1)) {
        mousePos = newPos;
        return;
    }
    auto delta = mousePos - newPos;
    mousePos = newPos;

    ImGuiIO& io = ImGui::GetIO();
    if(io.WantCaptureMouse) {
        return;
    }

    if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
        view = glm::translate(view, glm::vec3(-delta / gScale, 0));
    }
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if(io.WantCaptureMouse) {
        return;
    }

    float scale = 1;
    if(yoffset > 0) {
        scale = 1.1f;
    } else if(yoffset < 0) {
        scale = 1 / 1.1f;
    }

    auto mp = glm::vec4(((mousePos - screenSize / 2.0f) / screenSize) * 2.0f, 0, 1);
    mp.y = -mp.y;
    auto wp = glm::vec2(glm::inverse(projection * view) * mp);

    gScale *= scale;
    view = glm::translate(view, glm::vec3(wp, 0));
    view = glm::scale(view, glm::vec3(scale, scale, 1));
    view = glm::translate(view, glm::vec3(-wp, 0));
}

static void onResize(GLFWwindow* window, int width, int height) {
    screenSize = glm::vec2(width, height);
    glViewport(0, 0, width, height);
    projection = glm::ortho<float>(0, width, height, 0, 0.0f, 100.0f);

    for(const auto fb : framebuffers) {
        fb->resize(width, height);
    }
}

auto renderMap(const Map& map, int layer) {
    auto atlasSize = glm::vec2(atlas->width, atlas->height);

    std::vector<glm::vec4> vert;

    for(auto&& room : map.rooms) {
        for(int y2 = 0; y2 < 22; y2++) {
            for(int x2 = 0; x2 < 40; x2++) {
                auto tile = room.tiles[layer][y2][x2];
                // assert((tile & 0xFFFF0000) == 0);
                if(tile.tile_id == 0 || tile.tile_id >= 0x400) continue;

                auto pos = glm::vec2(x2 + room.x * 40, y2 + room.y * 22);
                pos *= 8;
                auto uv = uvs[tile.tile_id];

                if(sprites.contains(tile.tile_id)) {
                    auto& sprite = sprites[tile.tile_id];
                    // __debugbreak();

                    int composition_id = 0;

                    for(int j = 0; j < sprite.layer_count; ++j) {
                        auto subsprite_id = sprite.compositions[composition_id * sprite.layer_count + j];
                        if(subsprite_id >= sprite.subsprite_count) continue;

                        auto& layer = sprite.layers[j];
                        if(layer.is_normals1 || layer.is_normals2 || !layer.is_visible) continue;

                        auto& subsprite = sprite.sub_sprites[subsprite_id];

                        auto aUv = glm::vec2(uv.pos + subsprite.atlas_pos);
                        auto size = glm::vec2(subsprite.size);
                        auto ap = pos + glm::vec2(subsprite.composite_pos);

                        if(tile.vertical_mirror) {
                            ap.y = pos.y + (sprite.composite_size.y - (subsprite.composite_pos.y + subsprite.size.y));
                            aUv.y += size.y;
                            size.y = -size.y;
                        }
                        if(tile.horizontal_mirror) {
                            ap.x = pos.x + (sprite.composite_size.x - (subsprite.composite_pos.x + subsprite.size.x));
                            aUv.x += size.x;
                            size.x = -size.x;
                        }
                        assert(!tile.rotate_90 && !tile.rotate_180); // sprite rotation not implemented

                        vert.emplace_back(ap, aUv / atlasSize);                                                               // tl
                        vert.emplace_back(ap + glm::vec2(subsprite.size.x, 0), glm::vec2(aUv.x + size.x, aUv.y) / atlasSize); // tr
                        vert.emplace_back(ap + glm::vec2(0, subsprite.size.y), glm::vec2(aUv.x, aUv.y + size.y) / atlasSize); // bl

                        vert.emplace_back(ap + glm::vec2(subsprite.size.x, 0), glm::vec2(aUv.x + size.x, aUv.y) / atlasSize); // tr
                        vert.emplace_back(ap + glm::vec2(subsprite.size), (aUv + size) / atlasSize);                          // br
                        vert.emplace_back(ap + glm::vec2(0, subsprite.size.y), glm::vec2(aUv.x, aUv.y + size.y) / atlasSize); // bl
                    }
                } else {
                    auto right = glm::vec2(uv.size.x, 0);
                    auto down = glm::vec2(0, uv.size.y);

                    if(uv.contiguous || uv.self_contiguous) {
                        bool l, r, u, d;

                        auto l_ = map.getTile(layer, x2 + room.x * 40 - 1, y2 + room.y * 22);
                        auto r_ = map.getTile(layer, x2 + room.x * 40 + 1, y2 + room.y * 22);
                        auto u_ = map.getTile(layer, x2 + room.x * 40, y2 + room.y * 22 - 1);
                        auto d_ = map.getTile(layer, x2 + room.x * 40, y2 + room.y * 22 + 1);

                        if(uv.self_contiguous) {
                            l = l_.has_value() && l_.value().tile_id == tile.tile_id;
                            r = r_.has_value() && r_.value().tile_id == tile.tile_id;
                            u = u_.has_value() && u_.value().tile_id == tile.tile_id;
                            d = d_.has_value() && d_.value().tile_id == tile.tile_id;
                        } else {
                            l = !l_.has_value() || uvs[l_.value().tile_id].collides_right;
                            r = !r_.has_value() || uvs[r_.value().tile_id].collides_left;
                            u = !u_.has_value() || uvs[u_.value().tile_id].collides_down;
                            d = !d_.has_value() || uvs[d_.value().tile_id].collides_up;
                        }

                        if(l && r && u && d) {
                            uv.pos.x += 8;
                            uv.pos.y += 8;
                        } else if(l && r && d) {
                            uv.pos.x += 8;
                        } else if(l && u && d) {
                            uv.pos.x += 16;
                            uv.pos.y += 8;
                        } else if(l && u && r) {
                            uv.pos.x += 8;
                            uv.pos.y += 16;
                        } else if(u && d && r) {
                            uv.pos.y += 8;
                        } else if(l && u) {
                            uv.pos.x += 16;
                            uv.pos.y += 16;
                        } else if(l && d) {
                            uv.pos.x += 16;
                        } else if(u && r) {
                            uv.pos.y += 16;
                        } else if(d && r) {
                            // default
                        } else if(l && r) {
                            uv.pos.x += 8;
                            uv.pos.y += 24;
                        } else if(u && d) {
                            uv.pos.x += 24;
                            uv.pos.y += 8;
                        } else if(l) {
                            uv.pos.x += 16;
                            uv.pos.y += 24;
                        } else if(r) {
                            uv.pos.y += 24;
                        } else if(u) {
                            uv.pos.x += 24;
                            uv.pos.y += 16;
                        } else if(d) {
                            uv.pos.x += 24;
                        } else {
                            uv.pos.x += 24;
                            uv.pos.y += 24;
                        }
                    } else { // flags ignore for tiling
                        if(tile.vertical_mirror) {
                            uv.pos += down;
                            down = -down;
                        }
                        if(tile.horizontal_mirror) {
                            uv.pos += right;
                            right = -right;
                        }
                        if(tile.rotate_90) {
                            std::tie(uv.size.x, uv.size.y) = std::tie(uv.size.y, uv.size.x); // flip size
                            uv.pos += down;
                            std::tie(down, right) = std::pair(right, -down);
                        }
                        if(tile.rotate_180) {
                            uv.pos += down + right;
                            down = -down;
                            right = -right;
                        }
                    }
                    auto uvp = glm::vec2(uv.pos);

                    vert.emplace_back(pos                          , uvp / atlasSize);           // tl
                    vert.emplace_back(pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize); // tr
                    vert.emplace_back(pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);  // bl

                    vert.emplace_back(pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize);   // tr
                    vert.emplace_back(pos + glm::vec2(uv.size), (uvp + down + right) / atlasSize); // br
                    vert.emplace_back(pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);    // bl
                }
            }
        }
    }

    return vert;
}

auto renderBgs(const Map& map) {
    std::vector<std::pair<glm::vec2, glm::vec3>> bg;

    for(auto&& room : map.rooms) {
        auto rp = glm::vec2(room.x * 40 * 8, room.y * 22 * 8);

        if(room.bgId != 0) {
            bg.push_back({rp                , {0, 0, room.bgId - 1}}); // tl
            bg.push_back({{rp.x + 320, rp.y}, {1, 0, room.bgId - 1}}); // tr
            bg.push_back({{rp.x, rp.y + 176}, {0, 1, room.bgId - 1}}); // bl

            bg.push_back({{rp.x + 320, rp.y      }, {1, 0, room.bgId - 1}});       // tr
            bg.push_back({{rp.x + 320, rp.y + 176}, {1, 1, room.bgId - 1}}); // br
            bg.push_back({{rp.x      , rp.y + 176}, {0, 1, room.bgId - 1}});       // bl
        }
    }
    return bg;
}

auto renderLights(const Map& map) {
    std::vector<glm::ivec2> lights; // not really needed. i'll remove it later

    for(auto&& room : map.rooms) {
        for(int y2 = 0; y2 < 22; y2++) {
            for(int x2 = 0; x2 < 40; x2++) {
                auto tile = room.tiles[0][y2][x2];
                if(tile.tile_id == 0 || tile.tile_id >= 0x400)
                    continue;

                if(tile.tile_id == 202 || tile.tile_id == 554 || tile.tile_id == 561 || tile.tile_id == 624 || tile.tile_id == 731 || tile.tile_id == 548 || tile.tile_id == 46) {
                    lights.emplace_back(x2 + room.x * 40, y2 + room.y * 22);
                }
            }
        }
    }

    // radius ~6 tiles

    // general concept is to render the light circle and the block out everything not reachable by rendering over it with black triangles
    // currently has issues with light sources rendering over each other
    // will need to implement cutting the light circle polygon to remove problems and reduce number of triangles rendered

    std::vector<std::pair<glm::vec2, glm::vec3>> verts;
    auto black = glm::vec3(0, 0, 0);
    auto col1 = glm::vec3(0.10588, 0.31373, 0.54902);
    auto col2 = glm::vec3(0.30588, 0.64314, 0.78431);

    auto add_shadow = [&](glm::vec2 pos, glm::ivec2 d1, glm::ivec2 d2) {
        d1 -= 4.0f;
        d2 -= 4.0f;

        auto bl = pos + glm::vec2(d1);
        auto br = pos + glm::vec2(d2);
        auto tl = pos + glm::normalize(glm::vec2(d1)) * 55.0f;
        auto tr = pos + glm::normalize(glm::vec2(d2)) * 55.0f;

        verts.emplace_back(tl, black);
        verts.emplace_back(tr, black);
        verts.emplace_back(bl, black);

        verts.emplace_back(tr, black);
        verts.emplace_back(br, black);
        verts.emplace_back(bl, black);
    };

    for(auto light : lights) {
        auto lp = glm::vec2(light * 8) + 4.0f;

        const auto step = std::numbers::pi * 2 / 24;
        double angle = 0;
        auto p0 = glm::round(glm::vec2(std::cos(angle), std::sin(angle)) * 53.0f);

        for(int i = 0; i < 24; i++) {
            verts.emplace_back(lp, col1);
            verts.emplace_back(lp + p0, col1);
            angle += step;
            p0 = glm::round(glm::vec2(std::cos(angle), std::sin(angle)) * 53.0f);
            verts.emplace_back(lp + p0, col1);
        }

        angle = 0;
        p0 = glm::round(glm::vec2(std::cos(angle), std::sin(angle)) * 45.0f);

        for(int i = 0; i < 24; i++) {
            verts.emplace_back(lp, col2);
            verts.emplace_back(lp + p0, col2);
            angle += step;
            p0 = glm::round(glm::vec2(std::cos(angle), std::sin(angle)) * 45.0f);
            verts.emplace_back(lp + p0, col2);
        }

        for(int y = -6; y <= 6; ++y) {
            for(int x = -6; x <= 6; ++x) {
                auto t = map.getTile(0, light.x + x, light.y + y);
                if(!t.has_value() || t->tile_id == 0 || !uvs[t->tile_id].blocks_light)
                    continue;

                if(y > 0) { // top of tile hit
                    add_shadow(lp, glm::ivec2(x, y) * 8, glm::ivec2(x + 1, y) * 8);
                } else if(y < 0) { // bottom of tile hit
                    add_shadow(lp, glm::ivec2(x, y + 1) * 8, glm::ivec2(x + 1, y + 1) * 8);
                }
                if(x > 0) { // left of tile hit
                    add_shadow(lp, glm::ivec2(x, y) * 8, glm::ivec2(x, y + 1) * 8);
                } else if(x < 0) { // right of tile hit
                    add_shadow(lp, glm::ivec2(x + 1, y) * 8, glm::ivec2(x + 1, y + 1) * 8);
                }
            }
        }
    }

    return verts;
}

static void updateRender() {
    vertecies_fg = renderMap(maps[selectedMap], 0);
    vertecies_bg = renderMap(maps[selectedMap], 1);
    vertecies_bg_tex = renderBgs(maps[selectedMap]);
    vertecies_light = renderLights(maps[selectedMap]);

    VBO_fg->BufferData(vertecies_fg.data(), vertecies_fg.size() * sizeof(glm::vec4));
    VBO_bg->BufferData(vertecies_bg.data(), vertecies_bg.size() * sizeof(glm::vec4));
    VBO_bg_tex->BufferData(vertecies_bg_tex.data(), vertecies_bg_tex.size() * sizeof(float) * 5);
    VBO_light->BufferData(vertecies_light.data(), vertecies_light.size() * sizeof(float) * 5);
}

// helpger function renders a quad over the entire screen with uv fron (0,0) to (1,1)
// very useful for processing intermediate frambuffers
static void RenderQuad() {
    static std::unique_ptr<VAO> quadVAO;
    static std::unique_ptr<VBO> quadVBO;

    if(quadVAO == nullptr) {
        const float quadVertices[] = {
            // positions  // texture Coords
            -1.0f,  1.0f, 0.0f, 1.0f,
            -1.0f, -1.0f, 0.0f, 0.0f,
             1.0f,  1.0f, 1.0f, 1.0f,
             1.0f, -1.0f, 1.0f, 0.0f,
        };

        quadVAO = std::make_unique<VAO>();
        quadVAO->Bind();

        quadVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
        quadVBO->BufferData(quadVertices, sizeof(quadVertices));

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    }
    glBindVertexArray(quadVAO->id);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

static void DrawLine(std::vector<std::pair<glm::vec2, glm::vec4>>& verts, glm::vec2 p1, glm::vec2 p2, glm::vec4 color, float thickness) {
    auto d = glm::normalize(p2 - p1) * (thickness * 0.5f);

    verts.emplace_back(glm::vec2(p1.x + d.y, p1.y - d.x), color); // tl
    verts.emplace_back(glm::vec2(p2.x + d.y, p2.y - d.x), color); // tr
    verts.emplace_back(glm::vec2(p1.x - d.y, p1.y + d.x), color); // bl

    verts.emplace_back(glm::vec2(p2.x - d.y, p2.y + d.x), color); // br
    verts.emplace_back(glm::vec2(p1.x - d.y, p1.y + d.x), color); // bl
    verts.emplace_back(glm::vec2(p2.x + d.y, p2.y - d.x), color); // tr
}

static void DrawFilledRect(std::vector<std::pair<glm::vec2, glm::vec4>>& verts, glm::vec2 min, glm::vec2 max, glm::vec4 color) {
    verts.emplace_back(glm::vec2{ min.x, min.y }, color);
    verts.emplace_back(glm::vec2{ min.x, max.y }, color);
    verts.emplace_back(glm::vec2{ max.x, min.y }, color);

    verts.emplace_back(glm::vec2{ max.x, max.y }, color);
    verts.emplace_back(glm::vec2{ max.x, min.y }, color);
    verts.emplace_back(glm::vec2{ min.x, max.y }, color);
}

static void DrawRect(std::vector<std::pair<glm::vec2, glm::vec4>>& verts, glm::vec2 pos, glm::vec2 size, glm::vec4 color, float thickness) {
    DrawLine(verts, pos, pos + glm::vec2(size.x, 0), color, thickness);
    DrawLine(verts, pos + glm::vec2(size.x, 0), pos + size, color, thickness);
    DrawLine(verts, pos + size, pos + glm::vec2(0, size.y), color, thickness);
    DrawLine(verts, pos + glm::vec2(0, size.y), pos, color, thickness);
}

static void DrawCube(std::vector<std::pair<glm::vec2, glm::vec4>>& verts, glm::vec2 p1, glm::vec2 size, glm::vec4 color) {
    verts.emplace_back(p1, color);
    verts.emplace_back(glm::vec2(p1.x + size.x, p1.y), color);
    verts.emplace_back(glm::vec2(p1.x, p1.y + size.y), color);

    verts.emplace_back(glm::vec2(p1.x + size.x, p1.y), color);
    verts.emplace_back(p1 + size, color);
    verts.emplace_back(glm::vec2(p1.x, p1.y + size.y), color);
}

static ImVec2 toImVec(const glm::vec2 vec) {
    return ImVec2(vec.x, vec.y);
}

static void LoadAtlas(std::span<const uint8_t> data) {
    int width, height, n;
    auto* dat = stbi_load_from_memory(data.data(), data.size(), &width, &height, &n, 4);
    if(dat == nullptr) {
        throw std::runtime_error("invalid texture atlas format");
    }

    atlas = std::make_unique<Texture>();
    // chroma key cyan and replace with alpha

    auto vptr = (uint32_t*)dat;
    for(int i = 0; i < width * height; ++i) {
        if(vptr[i] == 0xFFFFFF00) {
            vptr[i] = 0;
        }
    }

    atlas->Bind();
    atlas->width = width;
    atlas->height = height;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, dat);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(dat);
};

static bool load_game(const std::string& path) {
    if(!std::filesystem::exists(path)) {
        ErrorDialog.push("File not found");
        return false;
    }
    rawData = readFile(path.c_str());

    auto sections = getSegmentOffsets(rawData);

    assert(sections.data.size() >= sizeof(asset_entry) * 676);
    auto assets = std::span((asset_entry*)sections.data.data(), 676);

    std::vector<uint8_t> decryptBuffer;

    auto get_asset = [&](int id) {
        assert(id >= 0 && id < 676);
        auto& asset = assets[id];
        assert(sections.rdata.size() >= asset.ptr - sections.rdata_pointer_offset + asset.length);
        auto dat = sections.rdata.subspan(asset.ptr - sections.rdata_pointer_offset, asset.length);

        if(tryDecrypt(asset, dat, decryptBuffer)) {
            return std::span(decryptBuffer);
        }
        return dat;
    };

    LoadAtlas(get_asset(255));

    bg_tex = std::make_unique<Texture>();
    glBindTexture(GL_TEXTURE_2D_ARRAY, bg_tex->id);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGB, 320, 180, 19, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    bg_tex->LoadSubImage(1 - 1, get_asset(14));
    bg_tex->LoadSubImage(2 - 1, get_asset(22));
    bg_tex->LoadSubImage(3 - 1, get_asset(22));
    bg_tex->LoadSubImage(4 - 1, get_asset(19));
    bg_tex->LoadSubImage(5 - 1, get_asset(19));
    bg_tex->LoadSubImage(6 - 1, get_asset(15));
    bg_tex->LoadSubImage(7 - 1, get_asset(13));
    bg_tex->LoadSubImage(8 - 1, get_asset(13));
    bg_tex->LoadSubImage(9 - 1, get_asset(16));
    bg_tex->LoadSubImage(10 - 1, get_asset(17));
    bg_tex->LoadSubImage(11 - 1, get_asset(16));
    bg_tex->LoadSubImage(12 - 1, get_asset(26));
    bg_tex->LoadSubImage(13 - 1, get_asset(11));
    bg_tex->LoadSubImage(14 - 1, get_asset(12));
    bg_tex->LoadSubImage(15 - 1, get_asset(20));
    bg_tex->LoadSubImage(16 - 1, get_asset(18));
    bg_tex->LoadSubImage(17 - 1, get_asset(23));
    bg_tex->LoadSubImage(18 - 1, get_asset(24));
    bg_tex->LoadSubImage(19 - 1, get_asset(21));

    for(size_t i = 0; i < 5; i++) {
        maps[i] = Map(get_asset(mapIds[i]));
    }

    for(auto el : spriteMapping) {
        sprites[el.tile_id] = parse_sprite(get_asset(el.asset_id));
    }

    uvs = parse_uvs(get_asset(254));

    updateRender();

    ImGui::CloseCurrentPopup();
    return true;
}

static void load_game_dialog() {
    static std::string lastPath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Animal Well";
    std::string path;
    auto result = NFD::OpenDialog({{"Game", {"exe"}}}, lastPath.c_str(), path, window);
    if(result == NFD::Result::Error) {
        ErrorDialog.push(NFD::GetError());
    } else if(result == NFD::Result::Okay) {
        lastPath = path;
        load_game(path);
    }
}

static void DrawUvFlags(uv_data& uv) {
    if(ImGui::BeginTable("uv_flags_table", 2)) {
        uint32_t flags = uv.flags;

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides left", &flags, 1 << 0); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Hidden", &flags, 1 << 10);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides right", &flags, 1 << 1); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Blocks Light", &flags, 1 << 8);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides up", &flags, 1 << 2); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("obscures", &flags, 1 << 6);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides down", &flags, 1 << 3); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Contiguous", &flags, 1 << 7); // correct

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Not Placeable", &flags, 1 << 4);
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Self Contiguous", &flags, 1 << 9); // correct

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Additive", &flags, 1 << 5);
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Dirt", &flags, 1 << 11);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Has Normals", &flags, 1 << 12); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("UV Light", &flags, 1 << 13); // correct

        if(flags != uv.flags) {
            uv.flags = flags;
            updateRender();
        }

        ImGui::EndTable();
    }
}

class {
    int selected_animation = 0;
    bool playing = false;
    int frame_step = 0;
    int selected_composition = 0;
    int selected_sprite = 0;

  public:
    void draw() {
        if(!ImGui::Begin("Sprite Viewer")) {
            ImGui::End();
            return;
        }
        if(ImGui::InputInt("Id", &selected_sprite)) {
            select(selected_sprite);
        }

        auto tile_id = spriteMapping[selected_sprite].tile_id;
        auto& sprite = sprites[tile_id];
        auto& uv = uvs[tile_id];

        ImGui::InputScalarN("Composite size", ImGuiDataType_U16, &sprite.composite_size, 2);
        ImGui::InputScalar("Layer count", ImGuiDataType_U16, &sprite.layer_count);
        ImGui::InputScalar("Composite count", ImGuiDataType_U16, &sprite.composition_count);
        ImGui::InputScalar("Subsprite count", ImGuiDataType_U16, &sprite.subsprite_count);
        ImGui::InputScalar("Animation count", ImGuiDataType_U16, &sprite.animation_count);

        ImGui::NewLine();
        if(ImGui::SliderInt("animation", &selected_animation, 0, std::max(0, sprite.animation_count - 1)) && playing) {
            selected_composition = sprite.animations[selected_animation].start;
        }
        if(sprite.animation_count != 0) {
            auto& anim = sprite.animations[selected_animation];
            ImGui::InputScalar("start", ImGuiDataType_U16, &anim.start);
            ImGui::InputScalar("end", ImGuiDataType_U16, &anim.end);
            ImGui::InputScalar("frame_delay", ImGuiDataType_U16, &anim.frame_delay);

            if(playing) {
                frame_step++;
                if(frame_step / 5 > anim.frame_delay) {
                    selected_composition++;
                    if(selected_composition > anim.end) {
                        selected_composition = anim.start;
                    }
                    frame_step = 0;
                }

                if(ImGui::Button("Pause")) playing = false;
            } else {
                if(ImGui::Button("Play")) {
                    playing = true;
                    selected_composition = anim.start;
                    frame_step = 0;
                }
            }
        }
        ImGui::SliderInt("composition", &selected_composition, 0, sprite.composition_count - 1);

        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        auto pos = glm::vec2(p.x, p.y);

        // draw_list->AddCallback([](const ImDrawList* parent_list, const ImDrawCmd* cmd) { }, nullptr);

        auto atlasSize = glm::vec2(atlas->width, atlas->height);
        for(int j = 0; j < sprite.layer_count; ++j) {
            auto subsprite_id = sprite.compositions[selected_composition * sprite.layer_count + j];
            if(subsprite_id >= sprite.subsprite_count)
                continue;

            auto& layer = sprite.layers[j];
            if(layer.is_normals1 || layer.is_normals2 || !layer.is_visible) continue;

            auto& subsprite = sprite.sub_sprites[subsprite_id];

            auto aUv = glm::vec2(uv.pos + subsprite.atlas_pos);
            auto size = glm::vec2(subsprite.size);
            auto ap = pos + glm::vec2(subsprite.composite_pos) * 8.0f;

            draw_list->AddImage((ImTextureID)atlas->id.value, toImVec(ap), toImVec(ap + glm::vec2(subsprite.size) * 8.0f), toImVec(aUv / atlasSize), toImVec((aUv + size) / atlasSize));
        }

        // draw_list->AddCallback()

        ImGui::End();
    }

    void select(int id) {
        selected_sprite = std::clamp(id, 0, 157);
        selected_animation = 0;
        selected_composition = 0;
        playing = false;
        frame_step = 0;
    }

    void select_from_tile(int tile_id) {
        // todo: lazy implementation could be improved with map
        auto spriteId = std::ranges::find(spriteMapping, tile_id, [](const TileMapping t) { return t.tile_id; })->internal_id;
        select(spriteId);
    }

    void focus() {
        ImGui::FocusWindow(ImGui::FindWindowByName("Sprite Viewer"));
    }
} SpriteViewer;

class {
  public:
    int selected_tile;

    void draw() {
        if(!ImGui::Begin("Tile Viewer")) {
            ImGui::End();
            return;
        }

        ImGui::InputInt("Id", &selected_tile);
        selected_tile = std::clamp(selected_tile, 0, (int)uvs.size() - 1);

        auto& uv = uvs[selected_tile];

        auto pos = glm::vec2(uv.pos);
        auto size = glm::vec2(uv.size);
        auto atlas_size = glm::vec2(atlas->width, atlas->height);

        ImGui::Text("preview");
        ImGui::Image((ImTextureID)atlas->id.value, toImVec(size * 8.0f), toImVec(pos / atlas_size), toImVec((pos + size) / atlas_size));

        DrawUvFlags(uv);

        ImGui::InputScalarN("UV", ImGuiDataType_U16, &uv.pos, 2);
        ImGui::InputScalarN("UV Size", ImGuiDataType_U16, &uv.size, 2);

        if(sprites.contains(selected_tile)) {
            auto spriteId = std::ranges::find(spriteMapping, selected_tile, [](const TileMapping t) { return t.tile_id; })->internal_id;

            ImGui::NewLine();
            ImGui::Text("Sprite id %i", spriteId);
            if(ImGui::Button("Open in Sprite Viewer")) {
                SpriteViewer.select(spriteId);
                SpriteViewer.focus();
            }
        }

        ImGui::End();
    }

    void focus() {
        ImGui::FocusWindow(ImGui::FindWindowByName("Tile Viewer"));
    }
} TileViewer;

class {
    int tile_id;
    std::vector<glm::ivec3> results;
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;
    bool show_on_map;

  public:
    void draw() {
        auto& map = maps[selectedMap];

        if(ImGui::Begin("Search")) {
            ImGui::InputInt("tile_id", &tile_id);

            if(ImGui::Button("Search")) {
                results.clear();

                for(auto& room : map.rooms) {
                    for(int y2 = 0; y2 < 22; y2++) {
                        for(int x2 = 0; x2 < 40; x2++) {
                            auto tile1 = room.tiles[0][y2][x2];
                            if(tile1.tile_id == tile_id) {
                                results.emplace_back(room.x * 40 + x2, room.y * 22 + y2, 0);
                            }

                            auto tile2 = room.tiles[1][y2][x2];
                            if(tile2.tile_id == tile_id) {
                                results.emplace_back(room.x * 40 + x2, room.y * 22 + y2, 1);
                            }
                        }
                    }
                }
            }
            if(ImGui::Button("Clear")) {
                results.clear();
            }
            ImGui::Checkbox("Highlight", &show_on_map);

            const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
            // When using ScrollX or ScrollY we need to specify a size for our table container!
            // Otherwise by default the table will fit all available space, like a BeginChild() call.
            ImVec2 outer_size = ImVec2(0.0f, TEXT_BASE_HEIGHT * 8);

            ImGui::Text("%i results", results.size());
            if(ImGui::BeginTable("search_results", 3, flags, outer_size)) {
                ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_None);
                ImGui::TableSetupColumn("y", ImGuiTableColumnFlags_None);
                ImGui::TableSetupColumn("layer", ImGuiTableColumnFlags_None);
                ImGui::TableHeadersRow();

                // Demonstrate using clipper for large vertical lists
                ImGuiListClipper clipper;
                clipper.Begin(results.size());
                while(clipper.Step()) {
                    for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                        ImGui::TableNextRow();

                        auto el = results[row];

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%i", el.x);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%i", el.y);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%i", el.z);
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();

        if(show_on_map) {
            for(auto result : results) {
                auto tile = map.getTile(result.z, result.x, result.y);
                if(!tile.has_value())
                    continue;

                auto uv = uvs[tile->tile_id];

                DrawRect(selectionVerts, glm::vec2(result.x * 8, result.y * 8), glm::vec2(uv.size), {1, 0, 0, 0.8f}, 8 / gScale);
            }
        }
    }

} SearchWindow;

class {
    std::vector<std::tuple<int, glm::vec2, glm::vec2>> tiles_;

  public:
    void draw() {
        if(!ImGui::Begin("Tile List")) {
            ImGui::End();
            return;
        }

        if(tiles_.empty()) {
            auto atlas_size = glm::vec2(atlas->width, atlas->height);

            for(int i = 0; i < uvs.size(); ++i) {
                auto uv = uvs[i];
                if(uv.size == glm::u16vec2(0)) {
                    continue;
                }
                tiles_.emplace_back(i, glm::vec2(uv.pos) / atlas_size, glm::vec2(uv.size) / atlas_size);
            }

            std::ranges::stable_sort(tiles_, {}, [](const auto el) { return std::get<2>(el).x * std::get<2>(el).y; });
        }

        auto width = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
        auto pad = ImGui::GetStyle().FramePadding.x;

        for(const auto& [id, pos, size] : tiles_) {
            ImGui::PushID(id);
            if(ImGui::ImageButton((ImTextureID)atlas->id.value, ImVec2(32, 32), toImVec(pos), toImVec(pos + size))) {
                TileViewer.selected_tile = id;
                TileViewer.focus();
            }
            ImGui::SetItemTooltip("%i", id);
            ImGui::PopID();
            ImGui::SameLine();

            auto x = ImGui::GetCursorPosX();
            if(x + 32 + pad >= width) {
                ImGui::NewLine();
            }
        }

        ImGui::End();
    }
} TileList;

static void dump_assets() {
    static std::string lastPath = std::filesystem::current_path().string();
    std::string path;
    auto result = NFD::PickFolder(lastPath.c_str(), path, window);
    if(result != NFD::Result::Okay) {
        return;
    }
    lastPath = path;

    try {
        std::filesystem::create_directories(path);

        auto sections = getSegmentOffsets(rawData);
        auto assets = std::span((asset_entry*)sections.data.data(), 676);

        std::vector<uint8_t> decrypted;

        for(int i = 0; i < assets.size(); ++i) {
            auto& item = assets[i];

            std::string ext = ".bin";
            switch(item.type) {
                case AssetType::Text:
                    ext = ".txt";
                    break;
                case AssetType::MapData:
                case AssetType::Encrypted_MapData:
                    ext = ".map";
                    break;
                case AssetType::Png:
                case AssetType::Encrypted_Png:
                    ext = ".png";
                    break;
                case AssetType::Ogg:
                case AssetType::Encrypted_Ogg:
                    ext = ".ogg";
                    break;
                case AssetType::SpriteData:
                    ext = ".sprite";
                    break;
                case AssetType::Shader:
                    ext = ".shader";
                    break;
                case AssetType::Font:
                    ext = ".font";
                    break;
                case AssetType::Encrypted_XPS:
                    ext = ".xps";
                    break;
            }

            auto dat = sections.rdata.subspan(item.ptr - sections.rdata_pointer_offset, item.length);

            std::ofstream file("assets/" + std::to_string(i) + ext, std::ios::binary);
            if(tryDecrypt(item, dat, decrypted)) {
                file.write((char*)decrypted.data(), decrypted.size());
            } else {
                file.write((char*)dat.data(), dat.size());
            }
            file.close();
        }
    } catch(std::exception& e) {
        ErrorDialog.push(e.what());
    }
}

static void randomize() {
    std::vector<MapTile> items;
    std::vector<glm::ivec2> locations;

    std::set<int> target;

    //  16 = save_point
    //  39 = telephone
    target.insert(40); // = key
    target.insert(41); // = match
    //  90 = egg
    target.insert(109); // = lamp
    target.insert(149); // = stamps
    // 161 = cheaters ring
    target.insert(162); // = b. wand
    target.insert(169); // = flute
    target.insert(214); // = map
    // 231 = deathless figure
    // 284 = disc // can't be randomized
    target.insert(323); // = uv light
    target.insert(334); // = yoyo
    target.insert(382); // = mock disc
    // 383 = firecracker
    target.insert(417); // = spring
    target.insert(442); // = pencil
    target.insert(466); // = remote
    target.insert(469); // = S.medal
    // 550.? = bunny
    target.insert(611); // = house key
    target.insert(617); // = office key
    // 627.0 = seahorse flame
    // 627.1 = cat flame
    // 627.1 = lizard flame
    // 627.3 = ostrich flame
    target.insert(634); // = top
    target.insert(637); // = b. ball
    target.insert(643); // = wheel
    target.insert(679); // = E.medal
    target.insert(708); // = bb. wand
    target.insert(711); // = 65th egg
    target.insert(780); // = f.pack

    auto& map = maps[0];

    for(auto& room : map.rooms) {
        for(int y2 = 0; y2 < 22; y2++) {
            for(int x2 = 0; x2 < 40; x2++) {
                auto& tile = room.tiles[0][y2][x2];

                if(target.contains(tile.tile_id)) {
                    items.push_back(tile);
                    locations.emplace_back(room.x * 40 + x2, room.y * 22 + y2);
                }
            }
        }
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::ranges::shuffle(locations, g);

    for(auto item : items) {
        auto loc = locations.back();
        locations.pop_back();

        map.setTile(0, loc.x, loc.y, item);
    }

    updateRender();
}

static void export_exe(bool patch_renderdoc) {
    static std::string lastPath = std::filesystem::current_path().string();
    std::string path;
    auto result = NFD::SaveDialog({{"Game", {"exe"}}}, lastPath.c_str(), path, window);

    if(result == NFD::Result::Error) {
        ErrorDialog.push(NFD::GetError());
        return;
    }
    if(result == NFD::Result::Cancel) {
        return;
    }
    lastPath = path;

    try {
        auto out = rawData;
        auto sections = getSegmentOffsets(out);
        auto assets = std::span((asset_entry*)sections.data.data(), 676);

        bool error = false;

        auto replaceAsset = [&](const std::vector<uint8_t>& data, int id) {
            auto& asset = assets[id];
            auto ptr = sections.rdata.subspan(asset.ptr - sections.rdata_pointer_offset, asset.length);

            if(((uint8_t)asset.type & 192) == 64) {
                int key = 0; // 193, 212, 255, 300
                if(id == 30 || id == 52) key = 1;
                else if(id == 222 || id == 277 || id == 377) key = 2;

                auto enc = encrypt(data, key);

                if(enc.size() > asset.length) {
                    error = true;
                    ErrorDialog.pushf("Failed to save asset %i because it was too big", id);
                } else {
                    // asset.length = enc.size(); // keep default value cause game doesn't really use length anyway
                    std::memcpy(ptr.data(), enc.data(), enc.size());
                }
            } else {
                if(data.size() > asset.length) {
                    error = true;
                    ErrorDialog.pushf("Failed to save asset %i because it was too big", id);
                } else {
                    std::memcpy(ptr.data(), data.data(), data.size());
                }
            }
        };

        for(size_t i = 0; i < 5; i++) {
            replaceAsset(maps[i].save(), mapIds[i]);
        }
        replaceAsset(save_uvs(uvs), 254);

        /*if(patch_steam) {
            // patch steam restart
            out[0xEFE6] = 0x48; // MOV AL, 0
            out[0xEFE7] = 0xc6;
            out[0xEFE8] = 0xc0;
            out[0xEFE9] = 0x00;

            out[0xEFEA] = 0x48; // NOP
            out[0xEFEB] = 0x90;
        }*/

        if(patch_renderdoc) {
            const char renderDocPattern[14] = "renderdoc.dll";
            auto res = std::search(out.begin(), out.end(), std::begin(renderDocPattern), std::end(renderDocPattern));

            if(res != out.end()) {
                *res = 'l'; // replace first letter with 'l'
            }
        }

        if(!error) {
            std::ofstream file(path, std::ios::binary);
            file.write(out.data(), out.size());
        }
    } catch(std::exception& e) {
        ErrorDialog.push(e.what());
    }
}

// Tips: Use with ImGuiDockNodeFlags_PassthruCentralNode!
// The limitation with this call is that your window won't have a menu bar.
// Even though we could pass window flags, it would also require the user to be able to call BeginMenuBar() somehow meaning we can't Begin/End in a single function.
// But you can also use BeginMainMenuBar(). If you really want a menu bar inside the same window as the one hosting the dockspace, you will need to copy this code somewhere and tweak it.
ImGuiID DockSpaceOverViewport() {
    auto viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    if(dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        host_window_flags |= ImGuiWindowFlags_NoBackground;

    char label[32];
    ImFormatString(label, IM_ARRAYSIZE(label), "DockSpaceViewport_%08X", viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(label, NULL, host_window_flags);
    ImGui::PopStyleVar(3);

    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Load Game")) {
                load_game_dialog();
            }
            if(ImGui::MenuItem("Dump assets")) {
                dump_assets();
            }

            ImGui::Separator();
            static bool patch_renderdoc = false;
            ImGui::Checkbox("patch renderdoc", &patch_renderdoc);
            if(ImGui::MenuItem("Export exe")) {
                export_exe(patch_renderdoc);
            }

            ImGui::Separator();

            if(ImGui::MenuItem("Load Map")) {
                static std::string lastPath = std::filesystem::current_path().string();
                std::string path;
                auto result = NFD::OpenDialog({{"Map", {"map"}}}, lastPath.c_str(), path, window);

                if(result == NFD::Result::Error) {
                    ErrorDialog.push(NFD::GetError());
                } else if(result == NFD::Result::Okay) {
                    lastPath = path;
                    try {
                        auto data = readFile(path.c_str());
                        auto map = Map(std::span((uint8_t*)data.data(), data.size()));

                        if(map.coordinate_map == maps[selectedMap].coordinate_map) {
                            maps[selectedMap] = map;
                            updateRender();
                        } else {
                            ErrorDialog.push("Map structure differs from currently loaded map.\nTry loading from a different slot.");
                        }
                    } catch(std::exception& e) {
                        ErrorDialog.push(e.what());
                    }
                }
            }
            if(ImGui::MenuItem("Export Map")) {
                static std::string lastPath = std::filesystem::current_path().string();
                std::string path;
                auto result = NFD::SaveDialog({{"Map", {"map"}}}, std::filesystem::current_path().string().c_str(), path, window);

                if(result == NFD::Result::Error) {
                    ErrorDialog.push(NFD::GetError());
                } else if(result == NFD::Result::Okay) {
                    lastPath = path;
                    auto data = maps[selectedMap].save();
                    try {
                        std::ofstream file(path, std::ios::binary);
                        file.write((char*)data.data(), data.size());
                        file.close();
                    } catch(std::exception& e) {
                        ErrorDialog.push(e.what());
                    }
                }
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Display")) {
            if(ImGui::Combo("Map", &selectedMap, mapNames, 5)) {
                updateRender();
            }

            ImGui::Checkbox("Foreground Tiles", &show_fg);
            ImGui::ColorEdit4("fg tile color", &fg_color.r);
            ImGui::Checkbox("Background Tiles", &show_bg);
            ImGui::ColorEdit4("bg tile color", &bg_color.r);
            ImGui::Checkbox("Background Texture", &show_bg_tex);
            ImGui::ColorEdit4("bg Texture color", &bg_tex_color.r);
            ImGui::Checkbox("Apply lighting", &do_lighting);
            ImGui::Checkbox("Show Room Grid", &room_grid);

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Tools")) {
            if(ImGui::MenuItem("Randomize items")) {
                randomize();
            }
            if(ImGui::MenuItem("Make everything transparent")) {
                for(auto& uv : uvs) {
                    uv.blocks_light = false;
                }
            }

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);

    static auto first_time = true;
    if(first_time) {
        first_time = false;

        ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
        ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        auto right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);

        // we now dock our windows into the docking node we made above
        ImGui::DockBuilderDockWindow("Sprite Viewer", right);
        ImGui::DockBuilderDockWindow("Tile Viewer", right);
        ImGui::DockBuilderDockWindow("Tile List", right);
        ImGui::DockBuilderDockWindow("Search", right);
        ImGui::DockBuilderDockWindow("Properties", right);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();

    return dockspace_id;
}

static void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if(ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static void DrawPreviewWindow() {
    ImGui::Begin("Properties");
    auto& io = ImGui::GetIO();
    // ImGui::Text("fps %f", ImGui::GetIO().Framerate);

    static const char* modes[] = {"Select", "Place"};
    static int mode = 0;
    ImGui::Combo("Mode", &mode, modes, sizeof(modes) / sizeof(char*));

    auto mp = glm::vec4(((mousePos - screenSize / 2.0f) / screenSize) * 2.0f, 0, 1);
    mp.y = -mp.y;
    auto mouse_world_pos = glm::ivec2(glm::inverse(MVP) * mp) / 8;
    const auto room_size = glm::ivec2(40, 22);

    if(mode == 0) {
        ImGui::SameLine();
        HelpMarker("Right click to select a tile.\nEsc to unselect.");
        static glm::ivec2 selectedPos = glm::vec2(-1, -1);
        if(!io.WantCaptureMouse && glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
            selectedPos = mouse_world_pos;
        }
        if(!io.WantCaptureKeyboard && glfwGetKey(window, GLFW_KEY_ESCAPE)) {
            selectedPos = glm::ivec2(-1, -1);
        }

        glm::ivec2 asdasd;
        if(selectedPos == glm::ivec2(-1, -1)) {
            if(io.WantCaptureMouse) {
                asdasd = glm::ivec2(-1, -1);
            } else {
                asdasd = mouse_world_pos;
            }
        } else {
            asdasd = selectedPos;
        }

        auto room_pos = asdasd / room_size;
        auto room = maps[selectedMap].getRoom(room_pos.x, room_pos.y);

        ImGui::Text("world pos %i %i", asdasd.x, asdasd.y);

        if(room != nullptr) {
            ImGui::SeparatorText("Room Data");
            ImGui::Text("position %i %i", room->x, room->y);
            ImGui::InputScalar("water level", ImGuiDataType_U8, &room->waterLevel);
            uint8_t bg_min = 0, bg_max = 18;
            if(ImGui::SliderScalar("background id", ImGuiDataType_U8, &room->bgId, &bg_min, &bg_max)) {
                vertecies_bg_tex = renderBgs(maps[selectedMap]);
                VBO_bg_tex->BufferData(vertecies_bg_tex.data(), vertecies_bg_tex.size() * sizeof(float) * 5);
            }

            ImGui::InputScalar("pallet_index", ImGuiDataType_U8, &room->pallet_index);
            ImGui::InputScalar("idk1", ImGuiDataType_U8, &room->idk1);
            ImGui::InputScalar("idk2", ImGuiDataType_U8, &room->idk2);
            ImGui::InputScalar("idk3", ImGuiDataType_U8, &room->idk3);

            auto tp = glm::ivec2(glm::mod(glm::vec2(asdasd), glm::vec2(room_size)));
            auto tile = room->tiles[0][tp.y][tp.x];
            int tile_layer = 0;

            if(show_fg && tile.tile_id != 0) {
                tile_layer = 0;
            } else {
                tile = room->tiles[1][tp.y][tp.x];
                tile_layer = 1;
                if(!show_bg || tile.tile_id == 0) {
                    tile = {};
                    tile_layer = 2;
                }
            }

            ImGui::SeparatorText("Room Tile Data");
            ImGui::Text("position %i %i %s", tp.x, tp.y, tile_layer == 0 ? "Foreground" : tile_layer == 1 ? "Background" : "N/A");
            ImGui::Text("id %i", tile.tile_id);
            ImGui::Text("param %i", tile.param);

            if(ImGui::BeginTable("tile_flags_table", 2)) {
                int flags = tile.flags;

                if(tile_layer == 2) ImGui::BeginDisabled();

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::CheckboxFlags("horizontal_mirror", &flags, 1);
                ImGui::TableNextColumn(); ImGui::CheckboxFlags("vertical_mirror", &flags, 2);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::CheckboxFlags("rotate_90", &flags, 4);
                ImGui::TableNextColumn(); ImGui::CheckboxFlags("rotate_180", &flags, 8);

                if(tile_layer == 2) ImGui::EndDisabled();

                if(flags != tile.flags) {
                    room->tiles[tile_layer][tp.y][tp.x].flags = flags;
                    updateRender();
                }

                ImGui::EndTable();
            }

            auto& uv = uvs[tile.tile_id];

            ImGui::SeparatorText("Tile Data");
            if(tile_layer == 2) ImGui::BeginDisabled();
            DrawUvFlags(uv);
            if(tile_layer == 2) ImGui::EndDisabled();

            auto pos = glm::vec2(asdasd) * 8.0f;

            if(sprites.contains(tile.tile_id)) {
                auto sprite = sprites[tile.tile_id];
                // todo: lazy implementation could be improved with map
                auto spriteId = std::ranges::find(spriteMapping, tile.tile_id, [](const TileMapping t) { return t.tile_id; })->internal_id;

                ImGui::NewLine();
                ImGui::Text("Sprite id %i", spriteId);
                if(ImGui::Button("Open in Sprite Viewer")) {
                    SpriteViewer.select_from_tile(tile.tile_id);
                    SpriteViewer.focus();
                }

                auto bb_max = pos + glm::vec2(8, 8);

                int composition_id = 0;
                for(int j = 0; j < sprite.layer_count; ++j) {
                    auto subsprite_id = sprite.compositions[composition_id * sprite.layer_count + j];
                    if(subsprite_id >= sprite.subsprite_count)
                        continue;

                    auto& subsprite = sprite.sub_sprites[subsprite_id];

                    auto ap = pos + glm::vec2(subsprite.composite_pos);
                    if(tile.vertical_mirror) {
                        ap.y = pos.y + (sprite.composite_size.y - (subsprite.composite_pos.y + subsprite.size.y));
                    }
                    if(tile.horizontal_mirror) {
                        ap.x = pos.x + (sprite.composite_size.x - (subsprite.composite_pos.x + subsprite.size.x));
                    }

                    auto end = ap + glm::vec2(subsprite.size);
                    bb_max.x = std::max(bb_max.x, end.x);
                    bb_max.y = std::max(bb_max.y, end.y);

                    auto& layer = sprite.layers[j];
                    if(layer.is_normals1 || layer.is_normals2 || !layer.is_visible) continue;

                    DrawRect(selectionVerts, ap, glm::vec2(subsprite.size), {1, 1, 1, 1}, 0.5f);
                    // ImGui::Image((ImTextureID)atlas->id.value, toImVec(size), )
                }

                DrawRect(selectionVerts, pos, bb_max - pos, {1, 1, 1, 0.8}, 1);
            } else {
                if(tile.tile_id == 0) {
                    DrawRect(selectionVerts, pos, glm::vec2(8), {1, 1, 1, 1}, 1);
                } else {
                    DrawRect(selectionVerts, pos, glm::vec2(uv.size), {1, 1, 1, 1}, 1);
                }
            }
            if(!room_grid) DrawRect(selectionVerts, room_pos * room_size * 8, glm::ivec2(40, 22) * 8, {1, 1, 1, 0.5}, 1);
            if (room->waterLevel < 176) {
                auto water_min = room_pos * room_size * 8 + glm::ivec2(0, 176);
                auto water_max = water_min + glm::ivec2(room_size.x * 8, (176-room->waterLevel) * -1);
                DrawFilledRect(selectionVerts, water_min, water_max, {0, 0, 1, 0.3});
            }
        }
    } else if(mode == 1) {
        static MapTile placing;
        static bool background;

        ImGui::SameLine();
        HelpMarker("Middle click to copy a tile.\nRight click to place a tile.");
        ImGui::Text("world pos %i %i", mouse_world_pos.x, mouse_world_pos.y);

        auto room_pos = mouse_world_pos / room_size;
        auto room = maps[selectedMap].getRoom(room_pos.x, room_pos.y);
        if(!io.WantCaptureMouse && room != nullptr) {
            if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE)) {
                auto tp = glm::ivec2(mouse_world_pos.x % room_size.x, mouse_world_pos.y % room_size.y);
                auto tile = room->tiles[0][tp.y][tp.x];

                if(show_fg && tile.tile_id != 0) {
                    placing = tile;
                    background = false;
                } else {
                    tile = room->tiles[1][tp.y][tp.x];
                    if(show_bg && tile.tile_id != 0) {
                        placing = tile;
                        background = true;
                    } else {
                        placing = {};
                    }
                }
            }
            if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
                auto tp = glm::ivec2(mouse_world_pos.x % room_size.x, mouse_world_pos.y % room_size.y);
                auto tile_layer = room->tiles[background ? 1 : 0];
                auto tile = tile_layer[tp.y][tp.x];
                if(tile != placing) {
                    tile_layer[tp.y][tp.x] = placing;
                    updateRender();
                }
            }
        }

        ImGui::NewLine();
        ImGui::Checkbox("background layer", &background);
        ImGui::NewLine();
        ImGui::InputScalar("id", ImGuiDataType_U16, &placing.tile_id);
        ImGui::InputScalar("param", ImGuiDataType_U8, &placing.param);

        if(ImGui::BeginTable("tile_flags_table", 2)) {
            int flags = placing.flags;

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Horizontal mirror", &flags, 1);
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Vertical mirror", &flags, 2);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Rotate 90", &flags, 4);
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Rotate 180", &flags, 8);

            placing.flags = flags;

            ImGui::EndTable();
        }

        auto& uv = uvs[placing.tile_id];

        auto pos = glm::vec2(uv.pos);
        auto size = glm::vec2(uv.size);
        auto atlas_size = glm::vec2(atlas->width, atlas->height);

        ImGui::Text("preview");
        ImGui::Image((ImTextureID)atlas->id.value, toImVec(size * 8.0f), toImVec(pos / atlas_size), toImVec((pos + size) / atlas_size));

        if(room != nullptr) {
            DrawRect(selectionVerts, glm::vec2(mouse_world_pos) * 8.0f, glm::vec2(8), {1, 1, 1, 1}, 1);
            if(!room_grid) DrawRect(selectionVerts, room_pos * room_size * 8, glm::ivec2(40, 22) * 8, {1, 1, 1, 0.5}, 1);
        }
    }

    ImGui::End();
}

static void draw_room_grid() {
    if(!room_grid) {
        return;
    }

    const auto& map = maps[selectedMap];

    for (size_t i = 0; i <= map.size.x; i++) {
        auto x = (map.offset.x + i) * 40 * 8;
        DrawLine(selectionVerts, {x, map.offset.y * 22 * 8}, {x, (map.offset.y + map.size.y) * 22 * 8}, {1, 1, 1, 0.75}, 1 / gScale);
    }
    for (size_t i = 0; i <= map.size.y; i++) {
        auto y = (map.offset.y + i) * 22 * 8;
        DrawLine(selectionVerts, {map.offset.x * 40 * 8, y}, {(map.offset.x + map.size.x) * 40 * 8, y}, {1, 1, 1, 0.75}, 1 / gScale);
    }
}

// Main code
int runViewer() {
#pragma region glfw/opengl setup
    glfwSetErrorCallback(glfw_error_callback);
    if(!glfwInit())
        return 1;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

    // Create window with graphics context
    window = glfwCreateWindow(1280, 720, "Animal Well map viewer", nullptr, nullptr);
    if(window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    int version = gladLoadGL(glfwGetProcAddress);
    if(version == 0) {
        printf("Failed to initialize OpenGL context\n");
        return -1;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetFramebufferSizeCallback(window, onResize);
    onResize(window, 1280, 720);
#pragma endregion

#pragma region imgui setup
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;   // Enable Keyboard Controls
    // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad; // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;       // Enable Docking
    // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

    // Load Fonts
    // - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
    // - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
    // - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
    // io.Fonts->AddFontDefault();
    // io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
    // io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
    // ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
    // IM_ASSERT(font != nullptr);
#pragma endregion

#pragma region opgenl buffer setup
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ShaderProgram tile_shader ("src/shaders/tile.vs" , "src/shaders/tile.fs");
    ShaderProgram bg_shader   ("src/shaders/bg.vs"   , "src/shaders/bg.fs");
    ShaderProgram light_shader("src/shaders/light.vs", "src/shaders/light.fs");
    ShaderProgram merge_shader("src/shaders/merge.vs", "src/shaders/merge.fs");

    merge_shader.Use();
    merge_shader.setInt("tiles", 0);
    merge_shader.setInt("lights", 1);

    VAO VAO_fg;
    VBO_fg = std::make_unique<VBO>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    VAO_fg.Bind();
    VBO_fg->Bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    VAO VAO_bg;
    VBO_bg = std::make_unique<VBO>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    VAO_bg.Bind();
    VBO_bg->Bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    VAO VAO_bg_tex;
    VBO_bg_tex = std::make_unique<VBO>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    VAO_bg_tex.Bind();
    VBO_bg_tex->Bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));

    VAO VAO_light;
    VBO_light = std::make_unique<VBO>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    VAO_light.Bind();
    VBO_light->Bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(2 * sizeof(float)));

    Textured_Framebuffer light_fb(1280, 720, GL_RGBA16F);
    Textured_Framebuffer tile_fb(1280, 720, GL_RGBA16F);

    framebuffers.push_back(&light_fb);
    framebuffers.push_back(&tile_fb);

    VAO VAO_selction;
    VBO_selection = std::make_unique<VBO>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    VAO_selction.Bind();
    VBO_selection->Bind();
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(2 * sizeof(float)));

#pragma endregion

    if(!load_game("C:/Program Files (x86)/Steam/steamapps/common/Animal Well/Animal Well.exe")) {
        load_game("./Animal Well.exe");
    }
    ErrorDialog.clear();

    // Main loop
    while(!glfwWindowShouldClose(window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        selectionVerts.clear();

        MVP = projection * view;

        DockSpaceOverViewport();
        ErrorDialog.draw();
        // ImGui::ShowDemoWindow();

        // skip rendering if no data is loaded
        if(!rawData.empty()) {
            SearchWindow.draw();
            TileList.draw();
            TileViewer.draw();
            SpriteViewer.draw();
            DrawPreviewWindow();

            draw_room_grid();

            // 1. light pass
            if(do_lighting) {
                glClearColor(0.0, 0.0, 0.0, 1.0);
                light_fb.Bind();
                glClear(GL_COLOR_BUFFER_BIT);

                light_shader.Use();
                light_shader.setMat4("MVP", MVP);
                VAO_light.Bind();
                glDrawArrays(GL_TRIANGLES, 0, vertecies_light.size());
            } else {
                glClearColor(1.0, 1.0, 1.0, 1.0);
                light_fb.Bind();
                glClear(GL_COLOR_BUFFER_BIT);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, 0);

            // 2. main pass
            tile_fb.Bind();
            glClearColor(0.0, 0.0, 0.0, 0.0);
            glClear(GL_COLOR_BUFFER_BIT);

            if(show_bg_tex) {
                bg_shader.Use();
                bg_shader.setMat4("MVP", MVP);
                bg_shader.setVec4("color", bg_tex_color);

                glActiveTexture(GL_TEXTURE0);
                bg_tex->Bind();

                VAO_bg_tex.Bind();
                glDrawArrays(GL_TRIANGLES, 0, vertecies_bg_tex.size());
            }

            tile_shader.Use();
            tile_shader.setMat4("MVP", MVP);

            glActiveTexture(GL_TEXTURE0);
            atlas->Bind();

            if(show_bg) {
                tile_shader.setVec4("color", bg_color);
                VAO_bg.Bind();
                glDrawArrays(GL_TRIANGLES, 0, vertecies_bg.size());
            }
            if(show_fg) {
                tile_shader.setVec4("color", fg_color);
                VAO_fg.Bind();
                glDrawArrays(GL_TRIANGLES, 0, vertecies_fg.size());
            }

            // 3. final merge pass
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);

            merge_shader.Use();
            glActiveTexture(GL_TEXTURE0);
            tile_fb.tex.Bind();
            glActiveTexture(GL_TEXTURE1);
            light_fb.tex.Bind();

            RenderQuad();

            // 4. draw selection vertices over that
            light_shader.Use();
            light_shader.setMat4("MVP", MVP);
            VAO_selction.Bind();
            VBO_selection->BufferData(selectionVerts.data(), selectionVerts.size() * sizeof(float) * 6);
            glDrawArrays(GL_TRIANGLES, 0, selectionVerts.size());
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

int main(int, char**) {
    runViewer();
    return 0;
}

#ifdef _WIN32
#include <Windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    return main(__argc, __argv);
}
#endif
