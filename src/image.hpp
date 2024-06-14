#pragma once

#include <span>
#include <string>

#include <glm/glm.hpp>

class Image {
    uint32_t* data_ = nullptr;
    int width = 0, height = 0;

  public:
    Image() {}
    Image(std::span<const uint8_t> data);
    Image(const std::string& path);
    Image(int width, int height);
    Image(glm::ivec2 size) : Image(size.x, size.y) {}

    Image(Image&& other) noexcept;
    Image(const Image&) = delete;
    Image& operator=(const Image&) = delete;

    ~Image();

    void save_png(const std::string& path);

    glm::ivec2 size() const { return {width, height}; }
    uint32_t* data() { return data_; }
    const uint32_t* data() const { return data_; }

    uint32_t operator()(int x, int y) const {
        assert(x >= 0 && x < width && y >= 0 && y < height);
        return data_[x + y * width];
    }
    uint32_t& operator()(int x, int y) {
        assert(x >= 0 && x < width && y >= 0 && y < height);
        return data_[x + y * width];
    }
};
