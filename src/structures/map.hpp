#pragma once

#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>

#include <glm/glm.hpp>

struct MapHeader {
    uint32_t signature1;
    uint16_t roomCount; // actually capped at 255

    uint8_t world_wrap_x_start;
    uint8_t world_wrap_x_end;
    uint32_t idk3; // always 8

    uint32_t signature2;
};

struct MapTile {
    uint16_t tile_id;
    uint8_t param; // depends on tile_id

    union {
        struct {
            bool horizontal_mirror : 1;
            bool vertical_mirror : 1;
            bool rotate_90 : 1;
            bool rotate_180 : 1;
        };
        uint8_t flags;
    };

    bool operator==(const MapTile& other) const {
        return tile_id == other.tile_id && param == other.param && flags == other.flags;
    }
};
static_assert(sizeof(MapTile) == 4);

struct Room {
    uint8_t x;
    uint8_t y;

    uint8_t bgId;
    uint8_t waterLevel;

    uint8_t lighting_index;
    uint8_t idk1;
    uint8_t idk2;
    uint8_t idk3;

    MapTile tiles[2][22][40];
};

static_assert(sizeof(Room) == 0x1b88);

class Map {
  public:
    uint8_t world_wrap_x_start;
    uint8_t world_wrap_x_end;

    glm::ivec2 offset;
    glm::ivec2 size;
    std::vector<Room> rooms;
    std::unordered_map<uint16_t, int> coordinate_map;

    Map() = default;

    explicit Map(std::span<const uint8_t> data) {
        if(data.size() < sizeof(MapHeader)) {
            throw std::runtime_error("Error parsing map: invalid size");
        }

        auto head = *(MapHeader*)(data.data());
        if(head.signature1 != 0xF00DCAFE || head.signature2 != 0xF0F0CAFE) {
            throw std::runtime_error("Error parsing map: invalid header");
        }
        if(data.size() < sizeof(MapHeader) + head.roomCount * sizeof(Room)) {
            throw std::runtime_error("Error parsing map: invalid size");
        }
        Room* rooms_ = (Room*)(data.data() + sizeof(MapHeader));

        world_wrap_x_start = head.world_wrap_x_start;
        world_wrap_x_end = head.world_wrap_x_end;
        rooms = {rooms_, rooms_ + head.roomCount};

        int x_min = 65535, x_max = 0;
        int y_min = 65535, y_max = 0;

        for(int i = 0; i < head.roomCount; i++) {
            auto& room = rooms[i];
            x_min = std::min(x_min, (int)room.x);
            x_max = std::max(x_max, (int)room.x);
            y_min = std::min(y_min, (int)room.y);
            y_max = std::max(y_max, (int)room.y);

            coordinate_map[room.x | (room.y << 8)] = i;
        }

        int width = x_max - x_min + 1;
        int height = y_max - y_min + 1;

        offset = {x_min, y_min};
        size = {width, height};
    }

    const Room* getRoom(glm::ivec2 pos) const {
        if(pos.x < 0 || pos.x >= 256 || pos.y < 0 || pos.y >= 256)
            return nullptr;
        if(coordinate_map.contains(pos.x | (pos.y << 8))) {
            return &rooms[coordinate_map.at(pos.x | (pos.y << 8))];
        }
        return nullptr;
    }
    Room* getRoom(glm::ivec2 pos) {
        if(pos.x < 0 || pos.x >= 256 || pos.y < 0 || pos.y >= 256)
            return nullptr;
        if(coordinate_map.contains(pos.x | (pos.y << 8))) {
            return &rooms[coordinate_map.at(pos.x | (pos.y << 8))];
        }
        return nullptr;
    }

    std::optional<MapTile> getTile(int layer, int x, int y) const {
        auto rx = x / 40;
        auto ry = y / 22;

        if(rx < 0 || rx >= 256 || ry < 0 || ry >= 256)
            return std::nullopt;

        if(coordinate_map.contains(rx | (ry << 8))) {
            const auto& room = rooms[coordinate_map.at(rx | (ry << 8))];
            return room.tiles[layer][y % 22][x % 40];
        }

        return std::nullopt;
    }

    void setTile(int layer, int x, int y, MapTile tile) {
        auto rx = x / 40;
        auto ry = y / 22;

        if(rx < 0 || rx >= 256 || ry < 0 || ry >= 256)
            return;

        if(coordinate_map.contains(rx | (ry << 8))) {
            auto& room = rooms[coordinate_map.at(rx | (ry << 8))];
            room.tiles[layer][y % 22][x % 40] = tile;
        }
    }

    auto save() const {
        auto bytes = sizeof(MapHeader) + rooms.size() * sizeof(Room);
        std::vector<uint8_t> data(bytes);

        *(MapHeader*)data.data() = {
            0xF00DCAFE,
            (uint16_t)rooms.size(),
            world_wrap_x_start,
            world_wrap_x_end,
            8,
            0xF0F0CAFE,
        };
        std::memcpy(data.data() + sizeof(MapHeader), rooms.data(), rooms.size() * sizeof(Room));

        return data;
    }
};
