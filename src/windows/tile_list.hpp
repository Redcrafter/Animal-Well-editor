#pragma once

#include <vector>

#include "../game_data.hpp"
#include "../rendering.hpp"

class TileList {
    std::vector<std::tuple<int, glm::vec2, glm::vec2>> tiles_;

  public:
    void draw(const GameData& game_data, const Texture& atlas);
};

inline TileList tile_list;
