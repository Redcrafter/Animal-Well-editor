#pragma once

#include <vector>
#include <functional>

#include "../game_data.hpp"

struct SearchResult {
    uint8_t map;
    uint8_t layer;

    glm::ivec2 room_pos;
    glm::ivec2 tile_pos;
};

class SearchWindow {
    int tile_id = 0;
    bool show_on_map = true;

    int searched_tile = 0;
    std::vector<SearchResult> results;

  public:
    void draw(const GameData& game_data, std::function<void(int, glm::ivec2)> goto_callback);
    void draw_overlay(const GameData& game_data, int selectedMap, float gScale);

  private:
    void search(const GameData& game_data);
};

inline SearchWindow search_window;
