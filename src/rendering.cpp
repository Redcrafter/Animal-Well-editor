#include "rendering.hpp"

constexpr bool isVine(uint16_t tile_id) {
    return tile_id == 0xc1 || tile_id == 0xf0 || tile_id == 0x111 || tile_id == 0x138;
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

static void render_tile(MapTile tile, glm::ivec2 tile_pos, const Map& map, std::span<const uv_data> uvs, int layer) {
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

    mesh.data.emplace_back(world_pos, uvp / atlasSize); // tl
    mesh.data.emplace_back(world_pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize); // tr
    mesh.data.emplace_back(world_pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);  // bl

    mesh.data.emplace_back(world_pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize);   // tr
    mesh.data.emplace_back(world_pos + glm::vec2(uv.size), (uvp + down + right) / atlasSize); // br
    mesh.data.emplace_back(world_pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);    // bl
}

void renderMap(const Map& map, const GameData& game_data) {
    auto& rd = *render_data;

    rd.fg_tiles.clear();
    rd.bg_tiles.clear();
    rd.bunny.clear();
    rd.time_capsule.clear();

    for(auto&& room : map.rooms) {
        const int yellow_sources = room.count_yellow();

        for(int layer = 0; layer < 2; layer++) {
            rd.push_type(layer == 0 ? BufferType::fg_tile : BufferType::bg_tile);

            for(int y2 = 0; y2 < 22; y2++) {
                for(int x2 = 0; x2 < 40; x2++) {
                    auto tile = room.tiles[layer][y2][x2];
                    if(tile.tile_id == 0 || tile.tile_id >= 0x400) continue;

                    if(rd.accurate_vines && isVine(tile.tile_id)) {
                        renderVine(x2, y2, layer, game_data.uvs[312], room); // uv for
                        continue;
                    }

                    auto pos = glm::ivec2(x2 + room.x * 40, y2 + room.y * 22);

                    if(game_data.sprites.contains(tile.tile_id)) {
                        render_sprite_custom([&](glm::ivec2 pos_, glm::u16vec2 size, glm::ivec2 uv_pos, glm::ivec2 uv_size) {
                            pos_ += pos * 8;
                            render_data->add_face(pos_, pos_ + glm::ivec2(size), uv_pos, uv_pos + uv_size);
                        }, tile, game_data, yellow_sources);
                    } else {
                        render_tile(tile, pos, map, game_data.uvs, layer);
                    }
                }
            }
            rd.pop_type();
        }
    }

    rd.fg_tiles.Buffer();
    rd.bg_tiles.Buffer();
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
