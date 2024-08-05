#pragma once

#include <cstdint>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

#include <glm/glm.hpp>

struct LightingData {
    glm::u8vec4 fg_ambient_multi; // alpha is ignored
    glm::u8vec4 bg_ambient_multi; // alpha is ignored
    glm::u8vec4 ambient_light; // alpha is ignored
    glm::u8vec4 light_intensity;
    glm::fvec3 dividers; // divides colors
    float saturation; // global saturation
    float bg_tex_light_multi; // amount lights affect background texture

    static std::vector<LightingData> parse(std::span<const uint8_t> data) {
        auto ptr = data.data();

        auto magic = *(uint32_t*)ptr;
        ptr += 4;
        if(magic != 0x00F00B00)
            throw std::runtime_error("Error parsing ambient data: invalid header");

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
