#pragma once

#include "map_slice.hpp"
#include <memory>

class HistoryItem {
  public:
    virtual ~HistoryItem() = default;

    // apply changes to map and invert state so it can be undone
    // returns position and location and size of changed area
    virtual std::pair<glm::ivec3, glm::ivec2> apply(Map& map) = 0;
};

class AreaMove final : public HistoryItem {
    glm::ivec3 dest;
    glm::ivec3 src;
    MapSlice tiles;

  public:
    AreaMove(glm::ivec3 dest, glm::ivec3 src, const MapSlice& tiles) : dest(dest), src(src), tiles(tiles) {}

    std::pair<glm::ivec3, glm::ivec2> apply(Map& map) override {
        tiles.swap(map, dest); // restore dest tiles
        tiles.swap(map, src); // restore src tiles
        std::swap(src, dest);

        return {dest, tiles.size()};
    }
};

class AreaChange final : public HistoryItem {
    glm::ivec3 position;
    MapSlice tiles;

  public:
    AreaChange(glm::ivec3 position, const MapSlice& tiles) : position(position), tiles(tiles) {}

    std::pair<glm::ivec3, glm::ivec2> apply(Map& map) override {
        tiles.swap(map, position);
        return {position, tiles.size()};
    }
};

class SingleChange final : public HistoryItem {
    glm::ivec3 position;
    MapTile tile;

  public:
    SingleChange(glm::ivec3 position, const MapTile& tile) : position(position), tile(tile) {}

    std::pair<glm::ivec3, glm::ivec2> apply(Map& map) override {
        auto t = map.getTile(position.z, position.x, position.y);
        map.setTile(position.z, position.x, position.y, tile);
        tile = t.value();

        return {position, {1, 1}};
    }
};
