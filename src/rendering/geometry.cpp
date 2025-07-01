#include "geometry.hpp"
#include "pipeline.hpp"
#include "renderData.hpp"

#include <glm/ext/matrix_clip_space.hpp>
#include <imgui.h>
#include <numbers>

constexpr bool isVine(uint16_t tile_id) {
    return tile_id == 0xc1 || tile_id == 0xf0 || tile_id == 0x111 || tile_id == 0x138;
}
constexpr bool isLamp(uint16_t tile_id) {
    return tile_id == 46 || tile_id == 202 || tile_id == 548 || tile_id == 554 || tile_id == 561 || tile_id == 624 || tile_id == 731;
}

static void renderVine(int x, int y, int layer, const uv_data& uv, const Room& room) {
    if(y > 0 && isVine(room.tiles[layer][y - 1][x].tile_id)) return; // is not the first tile

    const auto tile_id = room.tiles[layer][y][x].tile_id;
    const auto room_pos = glm::vec2(room.x * 40 * 8, room.y * 22 * 8);

    int segments = 4;
    for(size_t i = y + 1; i < 22; i++) {
        if(!isVine(room.tiles[layer][i][x].tile_id)) break;
        segments += 2;
    }
    const auto strand_count = (tile_id == 273) ? 2 : 3;

    for(int i = 0; i < strand_count; i++) {
        const auto x_pos = i * ((tile_id == 273) + 3) + (x * 8);
        auto y_pos = y * 8;

        int8_t x_offset = 0;
        bool has_flower = false;

        const auto x_hash = (int)std::truncf(fabsf(std::remainderf(sinf(x_pos * 17.5362300872802734375f + y_pos * 105.61455535888671875f) * 43758.546875f, 1.0f)) * 4.0f);

        for(int j = 1; j < segments; j++) {
            const auto y_hash = (int)std::truncf(fabsf(std::remainderf(sinf(x_pos * 649.49005126953125f + y_pos * 3911.650146484375f) * 43758.546875f, 1)) * 6.0f);

            int segment_length;
            if((j & 1) == 0) {
                segment_length = 7 - y_hash;
            } else {
                segment_length = y_hash + 1;
            }

            constexpr char lookup[4] = {0, -1, 0, 1};
            x_offset = lookup[(x_hash + j - 1) & 3];
            if(j == 1) x_offset = 0; // segments always use offsets from previous index for some reason?

            auto t = glm::vec2(x_pos + x_offset, y_pos) + room_pos;
            render_data->add_face(t, t + glm::vec2(1, segment_length), {}, {}, IM_COL32(69, 255, 145, 255));

            y_pos += segment_length;

            if(j == segments - 2) {
                has_flower = ((x_pos + y_pos * 13) & 3) == 1;
            }
        }

        if(has_flower) {
            // uvs for tile 312
            auto uv_ = glm::vec2(uv.pos);
            auto size = glm::vec2(uv.size);

            auto t = glm::vec2(x_pos + x_offset - 1, y_pos) + room_pos;
            render_data->add_face(t, t + size, uv_, uv_ + size);
        }
    }
}

void renderWaterfall(int x, int y, int layer, const Room& room, RoomBuffers& buf) {
    if(x > 0 && room(layer, x - 1, y).tile_id == 0x156) { // is not the first tile
        return;
    }

    int width = 1;
    while(x + width < 40 && room(layer, x + width, y).tile_id == 0x156) {
        width++;
    }

    int height = (24 - y) * 8; // down to water level or screen edge
    if(room.waterLevel != 180) {
        height = room.waterLevel - y * 8;
    }

    x *= 8;
    y *= 8;
    if(y == 0) {
        y -= 2; // if at top cut off top part
    }

    buf.waterfalls.push_back({{x, y}, {width * 8, height}, layer});
}

static void render_tile(MapTile tile, glm::ivec2 tile_pos, int layer, const Map& map, const GameData& game_data, uint32_t color = IM_COL32_WHITE) {
    auto& uvs = game_data.uvs;

    auto uv = uvs[tile.tile_id];
    auto right = glm::vec2(uv.size.x, 0);
    auto down = glm::vec2(0, uv.size.y);

    if(uv.flags & (contiguous | self_contiguous)) {
        auto l_ = map.getTile(layer, tile_pos.x - 1, tile_pos.y);
        auto r_ = map.getTile(layer, tile_pos.x + 1, tile_pos.y);
        auto u_ = map.getTile(layer, tile_pos.x, tile_pos.y - 1);
        auto d_ = map.getTile(layer, tile_pos.x, tile_pos.y + 1);

        bool l, r, u, d;
        if(uv.flags & self_contiguous) {
            l = l_.has_value() && l_.value().tile_id == tile.tile_id;
            r = r_.has_value() && r_.value().tile_id == tile.tile_id;
            u = u_.has_value() && u_.value().tile_id == tile.tile_id;
            d = d_.has_value() && d_.value().tile_id == tile.tile_id;
        } else {
            constexpr auto mask = collides_left | collides_right | collides_up | collides_down | obscures | blocks_light;
            l = !l_.has_value() || uvs[l_.value().tile_id].flags & mask;
            r = !r_.has_value() || uvs[r_.value().tile_id].flags & mask;
            u = !u_.has_value() || uvs[u_.value().tile_id].flags & mask;
            d = !d_.has_value() || uvs[d_.value().tile_id].flags & mask;
        }

        if(l && r) {
            uv.pos.x += 8;
        } else if(l) {
            uv.pos.x += 16;
        } else if(r) {
            uv.pos.x += 0;
        } else {
            uv.pos.x += 24;
        }

        if(u && d) {
            uv.pos.y += 8;
        } else if(u) {
            uv.pos.y += 16;
        } else if(d) {
            uv.pos.y += 0;
        } else {
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

    auto [mesh, tex] = render_data->get_current();
    auto atlasSize = glm::vec2(tex.width, tex.height);

    glm::vec2 world_pos = tile_pos * 8;

    mesh.data.emplace_back(world_pos, uvp / atlasSize, color); // tl
    mesh.data.emplace_back(world_pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize, color); // tr
    mesh.data.emplace_back(world_pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize, color);  // bl

    mesh.data.emplace_back(world_pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize, color);   // tr
    mesh.data.emplace_back(world_pos + glm::vec2(uv.size), (uvp + down + right) / atlasSize, color); // br
    mesh.data.emplace_back(world_pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize, color);    // bl

    if(layer == 1 && uv.flags & has_normals) {
        auto& mesh = render_data->bg_normals;

        auto off = glm::vec2(0, uv.size.y);
        if(uv.flags & (contiguous | self_contiguous)) {
            off.y *= 4;
        }

        mesh.data.emplace_back(world_pos, (uvp + off) / atlasSize, color); // tl
        mesh.data.emplace_back(world_pos + glm::vec2(uv.size.x, 0), (uvp + right + off) / atlasSize, color); // tr
        mesh.data.emplace_back(world_pos + glm::vec2(0, uv.size.y), (uvp + down + off) / atlasSize, color);  // bl

        mesh.data.emplace_back(world_pos + glm::vec2(uv.size.x, 0), (uvp + right + off) / atlasSize, color);   // tr
        mesh.data.emplace_back(world_pos + glm::vec2(uv.size), (uvp + down + right + off) / atlasSize, color); // br
        mesh.data.emplace_back(world_pos + glm::vec2(0, uv.size.y), (uvp + down + off) / atlasSize, color);    // bl
    }
}

static void render_lamp(MapTile tile, glm::ivec2 pos, int layer, const Map& map, const GameData& game_data) {
    int height = 0;
    while(true) {
        auto t = map.getTile(layer, pos.x, pos.y - height - 1);
        if(!t.has_value() || (game_data.uvs[t->tile_id].flags & collides_down)) {
            break;
        }
        height++;
    }

    render_data->push_type(BufferType::midground);

    render_tile(tile, pos, layer, map, game_data);
    if(height > 0) {
        render_tile({44, 0, {{tile.horizontal_mirror, false, false, false}}}, {pos.x, pos.y - height}, layer, map, game_data);

        MapTile tile_ {45, 0, {{tile.horizontal_mirror, false, false, false}}};
        for(int i = 1; i < height; ++i) {
            render_tile(tile_, {pos.x, pos.y - i}, layer, map, game_data);
        }
    }
    render_data->pop_type();
}

void renderMap(const Map& map, const GameData& game_data) {
    auto& rd = *render_data;

    rd.fg_tiles.clear();
    rd.mg_tiles.clear();
    rd.bg_tiles.clear();
    rd.bg_normals.clear();
    rd.bunny.clear();
    rd.time_capsule.clear();

    for(size_t i = 0; i < map.rooms.size(); i++) {
        auto& room = map.rooms[i];
        auto& buff = rd.room_buffers[i];
        buff.waterfalls.clear();

        const int yellow_sources = room.count_yellow();

        for(int layer = 0; layer < 2; layer++) {
            rd.push_type(layer == 0 ? BufferType::fg_tile : BufferType::bg_tile);

            for(int y2 = 0; y2 < 22; y2++) {
                for(int x2 = 0; x2 < 40; x2++) {
                    auto tile = room.tiles[layer][y2][x2];
                    if(tile.tile_id == 0 || tile.tile_id >= 0x400) continue;

                    // lamp rope / mount
                    if(tile.tile_id == 45 || tile.tile_id == 44) continue;

                    if(rd.accurate_vines && layer == 0 && isVine(tile.tile_id)) {
                        rd.push_type(BufferType::midground);
                        renderVine(x2, y2, layer, game_data.uvs[312], room); // uv for
                        rd.pop_type();
                        continue;
                    }
                    if(tile.tile_id == 0x156) {
                        renderWaterfall(x2, y2, layer, room, buff);
                    }

                    auto pos = glm::ivec2(x2 + room.x * 40, y2 + room.y * 22);
                    if(isLamp(tile.tile_id)) {
                        render_lamp(tile, pos, layer, map, game_data);
                        continue;
                    }
                    if(tile.tile_id == 17) {
                        render_tile(tile, pos, layer, map, game_data, IM_COL32(69, 255, 145, 255));
                        continue;
                    }

                    if(game_data.sprites.contains(tile.tile_id)) {
                        render_sprite_custom([&](glm::ivec2 pos_, glm::u16vec2 size, glm::ivec2 uv_pos, glm::ivec2 uv_size) {
                            pos_ += pos * 8;
                            render_data->add_face(pos_, pos_ + glm::ivec2(size), uv_pos, uv_pos + uv_size);

                            if(layer == 1) {
                                auto& mesh = render_data->bg_normals;

                                auto& tex = render_data->textures.atlas;
                                glm::vec2 tex_size {tex.width, tex.height};

                                mesh.AddRectFilled(pos_, pos_ + glm::ivec2(size), glm::vec2(uv_pos) / tex_size, glm::vec2(uv_pos + uv_size) / tex_size, IM_COL32_WHITE);
                            }
                        }, tile, game_data, yellow_sources);
                    } else {
                        render_tile(tile, pos, layer, map, game_data);
                    }
                }
            }
            rd.pop_type();
        }
    }

    rd.fg_tiles.Buffer();
    rd.mg_tiles.Buffer();
    rd.bg_tiles.Buffer();
    rd.bg_normals.Buffer();
    rd.bunny.Buffer();
    rd.time_capsule.Buffer();
}

void renderBgs(const Map& map) {
    constexpr auto texSize = glm::vec2(320 * 4, 180 * 4);

    constexpr glm::vec2 roomUvs[] = {
        {0, 0},
        glm::vec2(320 * 3, 180 * 0) / texSize,
        glm::vec2(320 * 3, 180 * 2) / texSize,
        glm::vec2(320 * 3, 180 * 2) / texSize,
        glm::vec2(320 * 0, 180 * 2) / texSize,
        glm::vec2(320 * 0, 180 * 2) / texSize,
        glm::vec2(320 * 0, 180 * 1) / texSize,
        glm::vec2(320 * 2, 180 * 0) / texSize,
        glm::vec2(320 * 2, 180 * 0) / texSize,
        glm::vec2(320 * 1, 180 * 1) / texSize,
        glm::vec2(320 * 2, 180 * 1) / texSize,
        glm::vec2(320 * 1, 180 * 1) / texSize,
        glm::vec2(320 * 2, 180 * 3) / texSize,
        glm::vec2(320 * 0, 180 * 0) / texSize,
        glm::vec2(320 * 1, 180 * 0) / texSize,
        glm::vec2(320 * 1, 180 * 2) / texSize,
        glm::vec2(320 * 3, 180 * 1) / texSize,
        glm::vec2(320 * 0, 180 * 3) / texSize,
        glm::vec2(320 * 1, 180 * 3) / texSize,
        glm::vec2(320 * 2, 180 * 2) / texSize,
    };

    auto& mesh = render_data->bg_text;
    mesh.clear();

    for(auto& room : map.rooms) {
        auto rp = glm::vec2(room.x * 40 * 8, room.y * 22 * 8);

        if(room.bgId != 0) {
            auto uv = roomUvs[room.bgId];
            mesh.AddRectFilled(rp, rp + glm::vec2(320, 176), uv, uv + glm::vec2(320, 176) / texSize); // maybe 320x180?
        }
    }

    mesh.Buffer();
}

void render_visibility(const Map& map, std::span<const uv_data> uvs) {
    auto& vis = render_data->visibility;
    vis.clear();

    std::vector<float> lightmap(22 * 40);
    // std::vector<float> lightmap2(22 * 40);

    for(auto&& room : map.rooms) {
        for(int y = 0; y < 22; y++) {
            for(int x = 0; x < 40; x++) {
                const auto tile = room.tiles[0][y][x];
                if(tile.tile_id == 0 || tile.tile_id >= 0x400) {
                    lightmap[x + y * 40] = 0;
                    continue;
                }

                lightmap[x + y * 40] = (uvs[tile.tile_id].flags & blocks_light) ? 1 : 0;
            }
        }

        for(int i = 0; i < 4; i++) {
            for(int x = 0; x < 40; x++) {
                for(int y = 0; y < 22; y++) {
                    auto v = lightmap[x + y * 40];
                    if(v == 0) continue;

                    float n = 0;

                    if(y < 21) n = lightmap[x + (y + 1) * 40];
                    else n += 1;

                    if(y > 0) n += lightmap[x + (y - 1) * 40];
                    else n += 1;

                    if(x > 0) n += lightmap[x - 1 + y * 40];
                    else n += 1;

                    if(x < 39) n += lightmap[x + 1 + y * 40];
                    else n += 1;

                    lightmap[x + y * 40] = (v * 3.0f + n) * 0.1428571f;
                }
            }
        }

        for(int y = 0; y < 22; y++) {
            for(int x = 0; x < 40; x++) {
                auto v = (int)(255 - lightmap[x + y * 40] * 255);
                if(v == 0xFF) continue;

                auto pos = glm::ivec2(x + room.x * 40, y + room.y * 22) * 8;

                vis.AddRectFilled(pos, pos + glm::ivec2(8, 8), {}, {}, IM_COL32(v, 0, 0, 255));
            }
        }
    }

    vis.Buffer();
}

constexpr auto zero = glm::vec2(0, 0);

void makeLight(Mesh& mesh, glm::u8vec4 color, int radius) {
    auto inner_col = glm::vec3(color) * (color.w / 255.0f);
    auto outer_col = inner_col * glm::vec3(0.35, 0.49, 0.7);

    auto outer_c = IM_COL32(outer_col.r, outer_col.g, outer_col.b, 255);
    auto inner_c = IM_COL32(inner_col.r, inner_col.g, inner_col.b, 255);

    constexpr auto step = std::numbers::pi * 2 / 24;
    constexpr auto center = glm::vec2(64, 64);
    double angle = 0;

    auto p0 = glm::vec2(1, 0);

    for(int i = 0; i < 24; i++) {
        auto o1 = (p0 * (float)(radius * 8 + 5));
        auto i1 = (p0 * (float)(radius * 8 - 3));

        angle += step;
        p0 = glm::vec2(std::cos(angle), -std::sin(angle));

        auto o2 = (p0 * (float)(radius * 8 + 5));
        auto i2 = (p0 * (float)(radius * 8 - 3));

        mesh.data.emplace_back(center + o1, zero, outer_c);
        mesh.data.emplace_back(center + o2, zero, outer_c);
        mesh.data.emplace_back(center, zero, outer_c);

        mesh.data.emplace_back(center + i1, zero, inner_c);
        mesh.data.emplace_back(center + i2, zero, inner_c);
        mesh.data.emplace_back(center, zero, inner_c);
    }
}

void renderLights(const Map& map, std::span<const uv_data> uvs) {
    auto& mesh = render_data->lights;

    auto& lb = render_data->temp_buffer;
    lb.Bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    render_data->shaders.flat.Use();
    render_data->shaders.flat.setMat4("MVP", glm::ortho<float>(0, 128, 0, 128, 0.0f, 100.0f));

    auto add_shadow = [&](glm::ivec2 d1, glm::ivec2 d2) {
        std::array<glm::vec2, 4> clip;

        clip[0] = 60.0f + glm::vec2(d1) * 100.0f; // tl
        clip[1] = 60.0f + glm::vec2(d2) * 100.0f; // tr
        clip[2] = 60.0f + glm::vec2(d2) * 8.0f; // br
        clip[3] = 60.0f + glm::vec2(d1) * 8.0f; // bl

        mesh.data.emplace_back(clip[0], zero, IM_COL32_BLACK);
        mesh.data.emplace_back(clip[1], zero, IM_COL32_BLACK);
        mesh.data.emplace_back(clip[2], zero, IM_COL32_BLACK);

        mesh.data.emplace_back(clip[0], zero, IM_COL32_BLACK);
        mesh.data.emplace_back(clip[2], zero, IM_COL32_BLACK);
        mesh.data.emplace_back(clip[3], zero, IM_COL32_BLACK);
    };

    for(size_t i = 0; i < map.rooms.size(); i++) {
        auto&& room = map.rooms[i];
        auto& buff = render_data->room_buffers[i];
        buff.lights.clear();

        for(int layer = 0; layer < 2; layer++) {
            for(int y = 0; y < 22; y++) {
                for(int x = 0; x < 40; x++) {
                    auto tile = room.tiles[layer][y][x];

                    glm::u8vec4 color;
                    int radius;
                    if(tile.tile_id == 0x2E) {
                        // IM_COL32(73, 84, 80, 255), IM_COL32(210 - 73, 172 - 84, 115 - 80, 255)
                        color = {0xff, 0xd2, 0x8c, 0xd2};
                        radius = 6;
                    } else if(tile.tile_id == 0xCA) {
                        // IM_COL32(27, 80, 140, 255), IM_COL32(78 - 27, 164 - 80, 200 - 140, 255)
                        color = {100, 0xd2, 0xff, 200};
                        radius = 6;
                    } else if(tile.tile_id == 0x224) {
                        color = {0x22, 0x8a, 0x30, 0xee};
                        radius = 4;
                    } else if(tile.tile_id == 0x22a) {
                        color = {0xe8, 0x80, 0xa4, 238};
                        radius = 5;
                    } else if(tile.tile_id == 0x231) {
                        color = {0, 50, 0xff, 0xff};
                        radius = 3;
                    } else if(tile.tile_id == 0x270) {
                        color = {250, 45, 70, 0xff};
                        radius = 3;
                    } else if(tile.tile_id == 0x2DB) {
                        color = {250, 200, 70, 0xff};
                        radius = 3;
                    } else {
                        continue;
                    }

                    mesh.clear();
                    makeLight(mesh, color, radius);
                    buff.lights.push_back(glm::vec4(x * 8 + 4, y * 8 + 4, (radius + 1) * 8, 0));

                    auto pos = glm::ivec2(x + room.x * 40, y + room.y * 22);
                    for(int y1 = -6; y1 <= 6; ++y1) {
                        for(int x1 = -6; x1 <= 6; ++x1) {
                            auto t = map.getTile(0, pos.x + x1, pos.y + y1);
                            if(!t.has_value() || t->tile_id == 0 || !(uvs[t->tile_id].flags & blocks_light))
                                continue;

                            // todo some tiles with special light blocking eg. 605

                            if(y1 > 0) { // top of tile hit
                                // auto t1 = map.getTile(0, x + x1, y + y1 - 1);
                                // if(!t1.has_value() || t1->tile_id == 0 || !(uvs[t1->tile_id].flags & blocks_light))
                                add_shadow({x1, y1}, {x1 + 1, y1});
                            } else if(y1 < 0) { // bottom of tile hit
                                // auto t1 = map.getTile(0, x + x1, y + y1 + 1);
                                // if(!t1.has_value() || t1->tile_id == 0 || !(uvs[t1->tile_id].flags & blocks_light))
                                add_shadow({x1 + 1, y1 + 1}, {x1, y1 + 1});
                            }
                            if(x1 > 0) { // left of tile hit
                                // auto t1 = map.getTile(0, x + x1 - 1, y + y1);
                                // if(!t1.has_value() || t1->tile_id == 0 || !(uvs[t1->tile_id].flags & blocks_light))
                                add_shadow({x1, y1 + 1}, {x1, y1});
                            } else if(x1 < 0) { // right of tile hit
                                // auto t1 = map.getTile(0, x + x1 + 1, y + y1);
                                // if(!t1.has_value() || t1->tile_id == 0 || !(uvs[t1->tile_id].flags & blocks_light))
                                add_shadow({x1 + 1, y1}, {x1 + 1, y1 + 1});
                            }
                        }
                    }

                    mesh.Buffer();

                    render_data->small_light_buffer.Bind();
                    glViewport(0, 0, 128, 128);

                    glClearColor(0, 0, 0, 0);
                    glClear(GL_COLOR_BUFFER_BIT);

                    render_data->shaders.flat.Use();
                    glDisable(GL_BLEND);
                    mesh.Draw();
                    glEnable(GL_BLEND);

                    lb.Bind();
                    glViewport(0, 0, lb.tex.width, lb.tex.height);

                    render_data->shaders.textured.Use();
                    render_data->small_light_buffer.tex.Bind();
                    glBlendFunc(GL_ONE, GL_ONE);
                    pos = pos * 8 + 4;
                    RenderQuad(pos - 64, pos + 64);
                    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                }
            }
        }
    }
}
