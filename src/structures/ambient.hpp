#pragma once

#include <cstdint>

#include <glm/glm.hpp>

struct AmbientData {
    glm::u8vec4 fg_color;
    glm::u8vec4 bg_color;
    glm::u8vec4 ambient_light;
    uint8_t unk1;
    uint8_t unk2;
    uint8_t unk3;
    uint8_t unk4;
    float floats[5];
};
static_assert(sizeof(AmbientData) == 36);
