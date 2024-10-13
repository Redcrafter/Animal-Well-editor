#pragma once

#include <span>
#include <vector>

#include <glm/glm.hpp>

enum uv_flags : uint16_t {
    collides_left = 1 << 0,
    collides_right = 1 << 1,
    collides_up = 1 << 2,
    collides_down = 1 << 3,
    not_placeable = 1 << 4,
    additive = 1 << 5,
    obscures = 1 << 6,
    contiguous = 1 << 7,
    blocks_light = 1 << 8,
    self_contiguous = 1 << 9,
    hidden = 1 << 10,
    dirt = 1 << 11,
    has_normals = 1 << 12,
    uv_light = 1 << 13,
};

struct uv_data {
    glm::u16vec2 pos;
    glm::u16vec2 size;
    uv_flags flags;

    static auto load(std::span<const uint8_t> data) {
        if(data.size() < 0xC) {
            throw std::runtime_error("invalid uv header size");
        }

        auto magic = *(uint32_t*)data.data();
        if(magic != 0x00B00B00) {
            throw std::runtime_error("invalid uv header");
        }
        auto count = *(uint32_t*)(data.data() + 4);
        assert(*(uint32_t*)(data.data() + 8) == 0); // null in the given asset

        if(data.size() < 0xC + count * sizeof(uv_data)) {
            throw std::runtime_error("invalid uv data size");
        }

        auto ptr = (uv_data*)(data.data() + 0xC);
        return std::vector(ptr, ptr + count);
    }

    static auto save(std::span<const uv_data> uvs) {
        std::vector<uint8_t> data(0xC + uvs.size() * sizeof(uv_data));

        *((uint32_t*)data.data() + 0) = 0x00B00B00;
        *((uint32_t*)data.data() + 1) = uvs.size(); // 1024
        *((uint32_t*)data.data() + 2) = 0;

        std::memcpy(data.data() + 0xC, uvs.data(), uvs.size() * sizeof(uv_data));

        return data;
    }
};
static_assert(sizeof(uv_data) == 10);
