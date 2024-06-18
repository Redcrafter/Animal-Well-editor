#pragma once

#include <cassert>
#include <span>
#include <string>
#include <utility>

class Image {
    uint32_t* data_ = nullptr;
    int width_ = 0, height_ = 0;

  public:
    Image() = default;
    Image(std::span<const uint8_t> data);
    Image(const std::string& path);
    Image(int width, int height);

    Image(Image&& other) noexcept;
    Image(const Image& other);
    Image& operator=(const Image& other);

    ~Image();

    void save_png(const std::string& path) const;

    // copy this image to another image
    void copy_to(Image& other, int x, int y) const;
    Image slice(int x, int y, int width, int height) const;
    void fill(int x, int y, int width, int height, uint32_t color);

    int width() const { return width_; }
    int height() const { return height_; }
    std::pair<int, int> size() const { return {width_, height_}; }

    uint32_t* data() { return data_; }
    const uint32_t* data() const { return data_; }

    uint32_t operator()(int x, int y) const {
        assert(x >= 0 && x < width_ && y >= 0 && y < height_);
        return data_[x + y * width_];
    }
    uint32_t& operator()(int x, int y) {
        assert(x >= 0 && x < width_ && y >= 0 && y < height_);
        return data_[x + y * width_];
    }

    Image& operator=(Image&& other) noexcept {
        free(data_);

        data_ = other.data_;
        width_ = other.width_;
        height_ = other.height_;

        other.data_ = nullptr;
        other.width_ = 0;
        other.height_ = 0;

        return *this;
    }

    bool operator==(const Image& other) const {
        if(width_ != other.width_ || height_ != other.height_) return false;
        for(int i = 0; i < width_ * height_; ++i) {
            if(data_[i] != other.data_[i]) return false;
        }
        return true;
    }
};
