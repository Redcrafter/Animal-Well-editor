#pragma once

#include <glm/glm.hpp>
#include <optional>

#include "../glStuff.hpp"
#include "../image.hpp"

glm::u16vec2 calc_tile_size(int i);

class TextureImporter {
    const int scale = 4;

    bool open_ = false;
    int selected_tile = 840;
    glm::ivec2 insert_pos = {0, 0};
    glm::ivec2 last_insert_pos;
    glm::ivec2 last_scroll;

    Image image;
    std::optional<Texture> image_texture = std::nullopt;

  public:
    void open(int tile_id);
    void draw();

  private:
    void apply();
    void drawCanvas();
    void LoadImage();
};

inline TextureImporter texture_importer;
