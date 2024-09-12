#include "rendering.hpp"
#include <math.h>

#include <glm/glm.hpp>

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

static void render_tile(MapTile tile, uv_data uv, glm::ivec2 pos, const Map& map, std::span<const uv_data> uvs, int layer) {
    auto right = glm::vec2(uv.size.x, 0);
    auto down = glm::vec2(0, uv.size.y);

    if(uv.contiguous || uv.self_contiguous) {
        auto l_ = map.getTile(layer, pos.x - 1, pos.y);
        auto r_ = map.getTile(layer, pos.x + 1, pos.y);
        auto u_ = map.getTile(layer, pos.x, pos.y - 1);
        auto d_ = map.getTile(layer, pos.x, pos.y + 1);

        bool l, r, u, d;
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

    pos *= 8;
    // render_data->add_face(pos, glm::ivec2(pos) + glm::ivec2(uv.size), uvp, uvp + right + down);
    // mesh.AddRectFilled(pos, glm::vec2(pos) + glm::vec2(uv.size), uvp, uvp + right + down);

    auto [mesh, tex] = render_data->get_current();
    auto atlasSize = glm::vec2(tex.width, tex.height);

    mesh.data.emplace_back(pos, uvp / atlasSize);           // tl
    mesh.data.emplace_back(glm::vec2(pos) + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize); // tr
    mesh.data.emplace_back(glm::vec2(pos) + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);  // bl

    mesh.data.emplace_back(glm::vec2(pos) + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize);   // tr
    mesh.data.emplace_back(glm::vec2(pos) + glm::vec2(uv.size), (uvp + down + right) / atlasSize); // br
    mesh.data.emplace_back(glm::vec2(pos) + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);    // bl
}

static void render_sprite_layer(MapTile tile, uv_data uv, glm::ivec2 pos, const SpriteData& sprite, int composition, int layer) {
    assert(layer < sprite.layers.size());
    assert(composition < sprite.composition_count);

    auto subsprite_id = sprite.compositions[composition * sprite.layers.size() + layer];
    if(subsprite_id >= sprite.sub_sprites.size()) return;

    auto& sprite_layer = sprite.layers[layer];
    if(sprite_layer.is_normals1 || sprite_layer.is_normals2 || !sprite_layer.is_visible) return;

    auto& subsprite = sprite.sub_sprites[subsprite_id];

    glm::ivec2 aUv = uv.pos + subsprite.atlas_pos;
    glm::ivec2 size = subsprite.size;
    auto ap = glm::vec2(pos) + glm::vec2(subsprite.composite_pos);

    if(tile.vertical_mirror) {
        ap.y = pos.y + (sprite.size.y - (subsprite.composite_pos.y + subsprite.size.y));
        aUv.y += size.y;
        size.y = -size.y;
    }
    if(tile.horizontal_mirror) {
        ap.x = pos.x + (sprite.size.x - (subsprite.composite_pos.x + subsprite.size.x));
        aUv.x += size.x;
        size.x = -size.x;
    }
    assert(!tile.rotate_90 && !tile.rotate_180); // sprite rotation not implemented

    render_data->add_face(ap, ap + glm::vec2(subsprite.size), aUv, aUv + size);
}

static void render_sprite(MapTile tile, uv_data uv, glm::ivec2 pos, const SpriteData& sprite) {
    pos = pos * 8;

    for(size_t j = 0; j < sprite.layers.size(); ++j) {
        render_sprite_layer(tile, uv, pos, sprite, 0, j);
    }
}

void renderMap(const Map& map, std::span<const uv_data> uvs, std::unordered_map<uint32_t, SpriteData>& sprites) {
    auto& rd = *render_data;

    rd.fg_tiles.clear();
    rd.bg_tiles.clear();
    rd.bunny.clear();
    rd.time_capsule.clear();

    for(auto&& room : map.rooms) {
        for(int layer = 0; layer < 2; layer++) {
            rd.push_type(layer == 0 ? BufferType::fg_tile : BufferType::bg_tile);

            for(int y2 = 0; y2 < 22; y2++) {
                for(int x2 = 0; x2 < 40; x2++) {
                    auto tile = room.tiles[layer][y2][x2];
                    if(tile.tile_id == 0 || tile.tile_id >= 0x400) continue;

                    if(rd.accurate_vines && isVine(tile.tile_id)) {
                        renderVine(x2, y2, layer, uvs[312], room); // uv for
                        continue;
                    }

                    auto pos = glm::ivec2(x2 + room.x * 40, y2 + room.y * 22);
                    auto uv = uvs[tile.tile_id];

                    if(tile.tile_id == 237) { // clock
                        pos *= 8;
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 3, 0); // left pendulum
                        render_sprite_layer(tile, uv, pos + glm::ivec2(111 * (tile.horizontal_mirror ? -1 : 1), 0), sprites[tile.tile_id], 3, 0); // right pendulum

                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 1); // clock face
                        // render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 2);  // speedrun numbers // too complicated to display
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 3); // clock body
                        
                        tile.horizontal_mirror = !tile.horizontal_mirror;
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 3); // clock body mirrored
                        tile.horizontal_mirror = !tile.horizontal_mirror;

                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 4); // left door platform
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 5); // middle door platform
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 6); // right door platform
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 7); // left door
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 8); // middle door
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 9); // right door
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 10); // top door
                    } else if(tile.tile_id == 341) { // big dog statue
                        pos *= 8;
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 0);
                        tile.horizontal_mirror = !tile.horizontal_mirror;
                        pos.x += 72 * (tile.horizontal_mirror ? 1 : -1);
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 0);
                    } else if(tile.tile_id == 568) { // big bat
                        pos *= 8;
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 0);
                        tile.horizontal_mirror = !tile.horizontal_mirror;
                        pos.x += 80 * (tile.horizontal_mirror ? 1 : -1);
                        render_sprite_layer(tile, uv, pos, sprites[tile.tile_id], 0, 0);
                    } else if(tile.tile_id == 793) { // Time capsule
                        rd.push_type(BufferType::time_capsule);
                        render_sprite(tile, uv, pos, sprites[tile.tile_id]);
                        rd.pop_type();
                    } else if(tile.tile_id == 794) { // Big Space Bunny
                        rd.push_type(BufferType::bunny);
                        render_sprite(tile, uv, pos, sprites[tile.tile_id]);
                        rd.pop_type();
                    } else {
                        if(sprites.contains(tile.tile_id)) {
                            render_sprite(tile, uv, pos, sprites[tile.tile_id]);
                        } else {
                            render_tile(tile, uv, pos, map, uvs, layer);
                        }
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
