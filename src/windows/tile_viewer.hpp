#pragma once

#include "../game_data.hpp"
#include "../rendering.hpp"

inline bool DrawUvFlags(uv_data& uv);

class TileViewer {
  public:
    int selected_tile;

    void draw(GameData& game_data, const Texture& atlas, bool& should_update);

    void focus();
};

inline TileViewer tile_viewer;
