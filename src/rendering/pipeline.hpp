#pragma once

#include <glm/glm.hpp>
#include "../game_data.hpp"
#include "../glStuff.hpp"

void RenderQuad(glm::vec2 min = {-1, -1}, glm::vec2 max = {1, 1}, glm::vec2 uv_min = {0, 0}, glm::vec2 uv_max = {1, 1});
void doRender(bool updateGeometry, GameData& game_data, int selectedMap, glm::mat4& MVP, Textured_Framebuffer* frameBuffer = nullptr);
