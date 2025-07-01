#pragma once
#include <glm/glm.hpp>
#include <vector>

#include "structures/map.hpp"

class MapSlice {
    std::vector<MapTile> data;
    glm::ivec2 _size;

  public:
    void copy(const Map& map, glm::ivec3 pos, glm::ivec2 size) {
        data.clear();
        data.reserve(size.x * size.y);
        this->_size = size;

        for(int y = 0; y < size.y; y++) {
            for(int x = 0; x < size.x; ++x) {
                data.push_back(map.getTile(pos.z, x + pos.x, y + pos.y).value_or(MapTile()));
            }
        }
    }
    void cut(Map& map, glm::ivec3 pos, glm::ivec2 size) {
        data.clear();
        data.reserve(size.x * size.y);
        this->_size = size;

        for(int y = 0; y < size.y; y++) {
            for(int x = 0; x < size.x; ++x) {
                data.push_back(map.getTile(pos.z, x + pos.x, y + pos.y).value_or(MapTile()));
                map.setTile(pos.z, x + pos.x, y + pos.y, {});
            }
        }
    }
    void swap(Map& map, glm::ivec3 pos) {
        for(int y = 0; y < _size.y; ++y) {
            for(int x = 0; x < _size.x; ++x) {
                auto t = map.getTile(pos.z, x + pos.x, y + pos.y);
                if(t.has_value()) {
                    map.setTile(pos.z, x + pos.x, y + pos.y, data[x + y * _size.x]);
                }
                data[x + y * _size.x] = t.value_or(MapTile());
            }
        }
    }
    void paste(Map& map, glm::ivec3 pos) const {
        for(int y = 0; y < _size.y; ++y) {
            for(int x = 0; x < _size.x; ++x) {
                map.setTile(pos.z, x + pos.x, y + pos.y, data[x + y * _size.x]);
            }
        }
    }
    void fill(const MapTile tile) {
        fill(tile, _size);
    }
    void fill(const MapTile tile, glm::ivec2 size) {
        data.clear();
        data.reserve(size.x * size.y);
        this->_size = size;

        for(int i = 0; i < size.x * size.y; i++) {
            data.push_back(tile);
        }
    }
    void clear() {
        data.clear();
        _size = {0, 0};
    }

    glm::ivec2 size() const { return _size; }
};
