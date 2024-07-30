#pragma once

#include "../game_data.hpp"
#include "../glStuff.hpp"

class SpriteViewer {
    int selected_sprite = 0;
    int selected_animation = 0;
    int selected_frame = 0;

    bool playing = false;
    int frame_step = 0;
    int direction = 1;
    int speed = 5;

  public:
    void draw(GameData& game_data, const Texture& atlas);
    void select(int id);
    void select_from_tile(int tile_id);
    void focus();
};

inline SpriteViewer sprite_viewer;
