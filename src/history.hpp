#pragma once

#include "map_slice.hpp"
#include <memory>

class HistoryItem {
  public:
    virtual ~HistoryItem() = default;

    // apply changes to map and invert state, so that it can be redone
    // returns position and size of changed area
    virtual std::pair<glm::ivec3, glm::ivec2> apply(Map& map) = 0;
};

class AreaMove final : public HistoryItem {
    glm::ivec3 dest;
    glm::ivec3 src;

    MapSlice dest_data;
    MapSlice src_data;

  public:
    AreaMove(glm::ivec3 dest, glm::ivec3 src, MapSlice dest_data, MapSlice src_data) : dest(dest), src(src), dest_data(std::move(dest_data)), src_data(std::move(src_data)) {}

    std::pair<glm::ivec3, glm::ivec2> apply(Map& map) override {
        dest_data.swap(map, dest);
        src_data.swap(map, src);

        std::swap(src_data, dest_data);
        std::swap(src, dest);

        return {dest, dest_data.size()};
    }
};

class AreaChange final : public HistoryItem {
    glm::ivec3 position;
    MapSlice tiles;

  public:
    AreaChange(glm::ivec3 position, MapSlice tiles) : position(position), tiles(std::move(tiles)) {}

    std::pair<glm::ivec3, glm::ivec2> apply(Map& map) override {
        tiles.swap(map, position);
        return {position, tiles.size()};
    }
};

class SingleChange final : public HistoryItem {
    glm::ivec3 position;
    MapTile tile;

  public:
    SingleChange(glm::ivec3 position, MapTile tile) : position(position), tile(tile) {}

    std::pair<glm::ivec3, glm::ivec2> apply(Map& map) override {
        auto t = map.getTile(position.z, position.x, position.y);
        map.setTile(position.z, position.x, position.y, tile);
        tile = t.value();

        return {position, {1, 1}};
    }
};
