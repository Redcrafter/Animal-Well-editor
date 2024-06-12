#pragma once

#include <unordered_map>
#include <span>

#include "glStuff.hpp"

#include "structures/map.hpp"
#include "structures/sprite.hpp"
#include "structures/tile.hpp"

void renderMap(const Map& map, std::span<const uv_data> uvs, std::unordered_map<uint32_t, SpriteData>& sprites, Mesh& mesh, int layer, bool accurate_vines);

void renderBgs(const Map& map, Mesh& mesh);
