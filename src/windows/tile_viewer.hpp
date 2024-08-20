#pragma once

#include "../game_data.hpp"
#include "../rendering.hpp"

void ImGui_draw_tile(uint16_t tile_id, const GameData& game_data, int frame);

bool DrawUvFlags(uv_data& uv);

class TileViewer {
  public:
    int selected_tile = 0;

  private:
    // variables for sprite animation
    int selected_animation = 0;
    int selected_frame = 0;

    bool playing = false;
    int frame_step = 0;
    int direction = 1;
    int speed = 5;

  public:
    void draw(GameData& game_data, bool& should_update);

    void focus();
};

inline TileViewer tile_viewer;
