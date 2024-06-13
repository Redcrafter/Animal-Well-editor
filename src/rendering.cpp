#include "rendering.hpp"
#include <math.h>

#include <glm/glm.hpp>

constexpr auto atlasSize = glm::vec2(1024, 2048);

constexpr bool isVine(uint16_t tile_id) {
    return tile_id == 0xc1 || tile_id == 0xf0 || tile_id == 0x111 || tile_id == 0x138;
}

static void renderVine(int x, int y, int layer, const Room& room, Mesh& mesh) {
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
            mesh.AddRectFilled(t, t + glm::vec2(1, segment_length), {}, {}, IM_COL32(69, 255, 145, 255));

            y_pos += segment_length;

            if(j == segments - 2) {
                has_flower = ((x_pos + y_pos * 13) & 3) == 1;
            }
        }

        if(has_flower) {
            // uvs for tile 312
            auto uv = glm::vec2(152, 48);
            auto size = glm::vec2(3, 3);

            auto t = glm::vec2(x_pos + x_offset - 1, y_pos) + room_pos;
            mesh.AddRectFilled(t, t + size, uv / atlasSize, (uv + size) / atlasSize);
        }
    }
}

void renderMap(const Map& map, std::span<const uv_data> uvs, std::unordered_map<uint32_t, SpriteData>& sprites, Mesh& mesh, int layer, bool accurate_vines) {
    mesh.clear();

    for(auto&& room : map.rooms) {
        for(int y2 = 0; y2 < 22; y2++) {
            for(int x2 = 0; x2 < 40; x2++) {
                auto tile = room.tiles[layer][y2][x2];
                // assert((tile & 0xFFFF0000) == 0);
                if(tile.tile_id == 0 || tile.tile_id >= 0x400) continue;

                if(accurate_vines && isVine(tile.tile_id)) {
                    renderVine(x2, y2, layer, room, mesh);
                    continue;
                }

                auto pos = glm::vec2(x2 + room.x * 40, y2 + room.y * 22);
                pos *= 8;
                auto uv = uvs[tile.tile_id];

                if(sprites.contains(tile.tile_id)) {
                    auto& sprite = sprites[tile.tile_id];
                    // __debugbreak();

                    int composition_id = 0;

                    for(size_t j = 0; j < sprite.layers.size(); ++j) {
                        auto subsprite_id = sprite.compositions[composition_id * sprite.layers.size() + j];
                        if(subsprite_id >= sprite.sub_sprites.size()) continue;

                        auto& sprite_layer = sprite.layers[j];
                        if(sprite_layer.is_normals1 || sprite_layer.is_normals2 || !sprite_layer.is_visible) continue;

                        auto& subsprite = sprite.sub_sprites[subsprite_id];

                        auto aUv = glm::vec2(uv.pos + subsprite.atlas_pos);
                        auto size = glm::vec2(subsprite.size);
                        auto ap = pos + glm::vec2(subsprite.composite_pos);

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

                        mesh.AddRectFilled(ap, ap + glm::vec2(subsprite.size), aUv / atlasSize, (aUv + size) / atlasSize);
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

                    // mesh.AddRectFilled(pos, pos + uv.size, uvp, uvp + right + down);

                    mesh.data.emplace_back(pos, uvp / atlasSize);           // tl
                    mesh.data.emplace_back(pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize); // tr
                    mesh.data.emplace_back(pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);  // bl

                    mesh.data.emplace_back(pos + glm::vec2(uv.size.x, 0), (uvp + right) / atlasSize);   // tr
                    mesh.data.emplace_back(pos + glm::vec2(uv.size), (uvp + down + right) / atlasSize); // br
                    mesh.data.emplace_back(pos + glm::vec2(0, uv.size.y), (uvp + down) / atlasSize);    // bl
                }
            }
        }
    }

    mesh.Buffer();
}

void renderBgs(const Map& map, Mesh& mesh) {
    const auto texSize = glm::vec2(320 * 4, 180 * 4);

    mesh.clear();

    static glm::vec2 roomUvs[] = {
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

    for(auto&& room : map.rooms) {
        auto rp = glm::vec2(room.x * 40 * 8, room.y * 22 * 8);

        if(room.bgId != 0) {
            auto uv = roomUvs[room.bgId];
            mesh.AddRectFilled(rp, rp + glm::vec2(320, 176), uv, uv + glm::vec2(320, 176) / texSize); // maybe 320x180?
        }
    }

    mesh.Buffer();
}
