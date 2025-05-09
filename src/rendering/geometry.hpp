#pragma once

#include <memory>
#include <span>
#include <unordered_map>

#include "../game_data.hpp"
#include "../glStuff.hpp"
#include "renderData.hpp"

void renderMap(const Map& map, const GameData& game_data);
void renderBgs(const Map& map);
void render_visibility(const Map& map, std::span<const uv_data> uvs);
void renderLights(const Map& map, std::span<const uv_data> uvs);

template<typename F>
void render_sprite_layer(F& f, MapTile tile, uv_data uv, const SpriteData& sprite, int frame, int layer, glm::ivec2 offset = {0, 0}) {
    assert(layer < sprite.layers.size());
    assert(frame < sprite.frame_count);

    auto subsprite_id = sprite.compositions[frame * sprite.layers.size() + layer];
    if(subsprite_id >= sprite.sub_sprites.size()) return;

    auto& sprite_layer = sprite.layers[layer];
    if(sprite_layer.is_normals1 || sprite_layer.is_normals2 || !sprite_layer.is_visible) return;

    auto& subsprite = sprite.sub_sprites[subsprite_id];

    glm::ivec2 uv_pos = uv.pos + subsprite.atlas_pos;
    glm::ivec2 uv_size = subsprite.size;
    auto pos = glm::ivec2(subsprite.composite_pos);

    if(tile.vertical_mirror) {
        pos.y = sprite.size.y - (subsprite.composite_pos.y + subsprite.size.y);
        uv_pos.y += uv_size.y;
        uv_size.y = -uv_size.y;
    }
    if(tile.horizontal_mirror) {
        pos.x = sprite.size.x - (subsprite.composite_pos.x + subsprite.size.x);
        uv_pos.x += uv_size.x;
        uv_size.x = -uv_size.x;
    }
    assert(!tile.rotate_90 && !tile.rotate_180); // sprite rotation not implemented

    f(pos + offset, subsprite.size, uv_pos, uv_size);
}

template<typename F>
void render_sprite(F&& f, MapTile tile, uv_data uv, const SpriteData& sprite, glm::ivec2 offset = {0, 0}, int frame = 0) {
    for(size_t i = 0; i < sprite.layers.size(); i++) {
        render_sprite_layer(f, tile, uv, sprite, frame, i, offset);
    }
}

// todo custom tile rendering? tile 17 should be green

template<typename F>
void render_sprite_custom(F&& f, MapTile tile, const GameData& game_data, int yellow_sources) {
    constexpr glm::ivec2 zero = {0, 0};

    auto uv = game_data.uvs[tile.tile_id];
    auto& sprite = game_data.sprites.at(tile.tile_id);

    switch(tile.tile_id) {
        case 147: // big fern
            render_sprite(f, tile, uv, sprite, zero, 2);
            break;
        case 237: // clock
            render_sprite_layer(f, tile, uv, sprite, 3, 0); // left pendulum
            render_sprite_layer(f, tile, uv, sprite, 3, 0, {111 * (tile.horizontal_mirror ? -1 : 1), 0}); // right pendulum

            render_sprite_layer(f, tile, uv, sprite, 0, 1); // clock face
            // render_sprite_layer(f, tile, uv, sprite, 0, 2);  // speedrun numbers // too complicated to display

            // render_data->push_type(BufferType::midground);
            render_sprite_layer(f, tile, uv, sprite, 0, 3); // clock body
            tile.horizontal_mirror = !tile.horizontal_mirror;
            render_sprite_layer(f, tile, uv, sprite, 0, 3); // clock body mirrored
            tile.horizontal_mirror = !tile.horizontal_mirror;
            render_sprite_layer(f, tile, uv, sprite, 0, 10); // top door
            // render_data->pop_type();

            render_data->push_type(BufferType::fg_tile);
            render_sprite_layer(f, tile, uv, sprite, 0, 4); // left door platform
            render_sprite_layer(f, tile, uv, sprite, 0, 5); // middle door platform
            render_sprite_layer(f, tile, uv, sprite, 0, 6); // right door platform
            render_data->pop_type();

            render_sprite_layer(f, tile, uv, sprite, 0, 7); // left door
            render_sprite_layer(f, tile, uv, sprite, 0, 8); // middle door
            render_sprite_layer(f, tile, uv, sprite, 0, 9); // right door
            break;
        case 256: // spawn bulb
            render_sprite(f, tile, uv, sprite);
            tile.horizontal_mirror = !tile.horizontal_mirror;
            render_sprite(f, tile, uv, sprite);
            break;
        case 288: { // ostrich // 289
            // TODO
            render_sprite(f, tile, uv, sprite);
            break;
        }
        case 310: // button door indicator
            render_sprite_layer(f, tile, uv, sprite, yellow_sources > 4 ? 0 : yellow_sources, 0);
            break;
        case 341: // big dog statue
            render_sprite_layer(f, tile, uv, sprite, 0, 0);
            tile.horizontal_mirror = !tile.horizontal_mirror;
            render_sprite_layer(f, tile, uv, sprite, 0, 0, {72 * (tile.horizontal_mirror ? 1 : -1), 0});
            break;
        case 363: // peacock
            render_sprite(f, tile, uv, sprite, {0, 0}, 6);
            tile.horizontal_mirror = !tile.horizontal_mirror;
            render_sprite(f, tile, uv, sprite, {1, 0}, 6);
            break;
        case 367: // dog
            render_sprite(f, tile, uv, sprite, zero, 18);
            break;
        case 568: // big bat
            render_sprite_layer(f, tile, uv, sprite, 0, 0);
            tile.horizontal_mirror = !tile.horizontal_mirror;
            render_sprite_layer(f, tile, uv, sprite, 0, 0, {80 * (tile.horizontal_mirror ? 1 : -1), 0});
            break;
        case 597: { // ostrich wheel
            // TODO
            render_sprite(f, tile, uv, sprite);
            break;
        }
        case 627: // flame orbs
            render_sprite(f, tile, uv, sprite);
            if(tile.param < 4) {
                auto uv = game_data.uvs[411 + tile.param];
                f({12, 24}, uv.size, uv.pos, uv.size);
            }
            break;
        case 674: // jellyfish
            render_sprite(f, tile, uv, sprite, zero, tile.param < 4 ? tile.param * 3 : 0);
            break;
        case 730: // groundhog
            render_sprite(f, tile, uv, sprite, zero, 10);
            break;
        case 793: // time capsule
            render_data->push_type(BufferType::time_capsule);
            render_sprite(f, tile, uv, sprite);
            render_data->pop_type();
            break;
        case 794: // space bunny
            render_data->push_type(BufferType::bunny);
            render_sprite(f, tile, uv, sprite);
            render_data->pop_type();
            break;
        default:
            render_sprite(f, tile, uv, sprite);
            break;
    }
}
