#pragma once

#include "map_slice.hpp"
#include <glm/glm.hpp>

#include <deque>
#include <memory>
#include <vector>

class HistoryItem {
  public:
    virtual ~HistoryItem() = default;

    // apply changes to map and invert state, so that it can be redone
    // returns position and size of changed area
    virtual void apply() = 0;
};

class AreaMove final : public HistoryItem {
    glm::ivec3 dest;
    glm::ivec3 src;

    MapSlice dest_data;
    MapSlice src_data;

  public:
    AreaMove(glm::ivec3 dest, glm::ivec3 src, MapSlice dest_data, MapSlice src_data) : dest(dest), src(src), dest_data(std::move(dest_data)), src_data(std::move(src_data)) {}

    void apply() override;
};

class AreaChange final : public HistoryItem {
    glm::ivec3 position;
    MapSlice tiles;

  public:
    AreaChange(glm::ivec3 position, MapSlice tiles) : position(position), tiles(std::move(tiles)) {}

    void apply() override;
};

class SingleChange final : public HistoryItem {
    glm::ivec3 position;
    MapTile tile;

  public:
    SingleChange(glm::ivec3 position, MapTile tile) : position(position), tile(tile) {}

    void apply() override;
};

class MapClear final : public HistoryItem {
    std::vector<Room> rooms;

  public:
    MapClear(std::vector<Room> rooms) : rooms(std::move(rooms)) {}

    void apply() override;
};

class SwitchLayer final : public HistoryItem {
    int from;

  public:
    SwitchLayer(int from) : from(from) {}

    void apply() override;
};

class HistoryManager {
  private:
    // copying the entire map takes ~1MB so 1000 entries is totally fine
    static constexpr size_t max_undo_size = 1000;
    // needs insertion/deletion at both sides due to overflow protection
    std::deque<std::unique_ptr<HistoryItem>> undo_buffer;
    std::vector<std::unique_ptr<HistoryItem>> redo_buffer;

  public:
    // push new action to history
    void push_action(std::unique_ptr<HistoryItem> item);

    // undo most recent item
    void undo();
    void redo();
    void clear();
};

inline HistoryManager history;
