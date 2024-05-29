#include "rendering.hpp"

#include <numbers>

#include "glm/glm.hpp"


void renderMap(const Map& map, std::span<const uv_data> uvs, std::unordered_map<uint32_t, SpriteData>& sprites, Mesh& mesh, int layer) {
    auto atlasSize = glm::vec2(1024, 2048);

    mesh.clear();

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
        glm::vec2(320 * 2, 180 * 2) / texSize
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

void renderLights(const Map& map, std::span<const uv_data> uvs, Mesh & mesh) {
    std::vector<glm::ivec2> lights; // not really needed. i'll remove it later

    mesh.clear();

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

    uint32_t col1 = IM_COL32(27, 80, 140, 255);
    uint32_t col2 = IM_COL32(78, 164, 200, 255);
    constexpr auto z = glm::vec2(0, 0);

    auto add_shadow = [&](glm::vec2 pos, glm::ivec2 d1, glm::ivec2 d2) {
        d1 -= 4.0f;
        d2 -= 4.0f;

        auto bl = pos + glm::vec2(d1);
        auto br = pos + glm::vec2(d2);
        auto tl = pos + glm::normalize(glm::vec2(d1)) * 55.0f;
        auto tr = pos + glm::normalize(glm::vec2(d2)) * 55.0f;

        mesh.data.emplace_back(tl, z, IM_COL32_BLACK);
        mesh.data.emplace_back(tr, z, IM_COL32_BLACK);
        mesh.data.emplace_back(bl, z, IM_COL32_BLACK);

        mesh.data.emplace_back(tr, z, IM_COL32_BLACK);
        mesh.data.emplace_back(br, z, IM_COL32_BLACK);
        mesh.data.emplace_back(bl, z, IM_COL32_BLACK);
    };

    for(auto light : lights) {
        auto lp = glm::vec2(light * 8) + 4.0f;

        const auto step = std::numbers::pi * 2 / 24;
        double angle = 0;
        auto p0 = glm::round(glm::vec2(std::cos(angle), std::sin(angle)) * 53.0f);

        for(int i = 0; i < 24; i++) {
            mesh.data.emplace_back(lp, z, col1);
            mesh.data.emplace_back(lp + p0, z, col1);
            angle += step;
            p0 = glm::round(glm::vec2(std::cos(angle), std::sin(angle)) * 53.0f);
            mesh.data.emplace_back(lp + p0, z, col1);
        }

        angle = 0;
        p0 = glm::round(glm::vec2(std::cos(angle), std::sin(angle)) * 45.0f);

        for(int i = 0; i < 24; i++) {
            mesh.data.emplace_back(lp, z, col2);
            mesh.data.emplace_back(lp + p0, z, col2);
            angle += step;
            p0 = glm::round(glm::vec2(std::cos(angle), std::sin(angle)) * 45.0f);
            mesh.data.emplace_back(lp + p0, z, col2);
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

    mesh.Buffer();
}
