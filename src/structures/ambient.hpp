#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>

struct LightingData {
    glm::u8vec4 fg_ambient_light_color; // alpha is ignored
    glm::u8vec4 bg_ambient_light_color; // alpha is ignored
    glm::u8vec4 ambient_light_color; // alpha is ignored
    glm::u8vec4 fog_color;
    glm::fvec3 color_gain;
    float color_saturation;
    float far_background_reflectivity; // amount lights affect background texture

    static std::vector<LightingData> parse(std::span<const uint8_t> data) {
        auto ptr = data.data();

        auto magic = *(uint32_t*)ptr;
        ptr += 4;
        if(magic != 0x00F00B00)
            throw std::runtime_error("parsing ambient data failed: invalid header magic");

        auto count = *(uint64_t*)ptr;
        ptr += 8;

        return {(LightingData*)ptr, ((LightingData*)ptr) + count};
    }
    static std::vector<uint8_t> save(const std::span<LightingData>& data) {
        std::vector<uint8_t> res(4 + 8 + data.size_bytes());

        auto ptr = res.data();
        *(uint32_t*)ptr = 0x00F00B00; // magic
        ptr += 4;
        *(uint64_t*)ptr = data.size(); // magic
        ptr += 8;
        std::memcpy(ptr, data.data(), data.size_bytes());

        return res;
    }
};
static_assert(sizeof(LightingData) == 36);
