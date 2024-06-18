#include "image.hpp"

#include <cstring>
#include <stdexcept>

#pragma warning disable
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma warning restore

Image::Image(std::span<const uint8_t> data) {
    data_ = (uint32_t*)stbi_load_from_memory(data.data(), data.size(), &width_, &height_, nullptr, 4);
    if(data_ == nullptr) {
        throw std::runtime_error("failed to load image");
    }
}

Image::Image(const std::string& path) {
    data_ = (uint32_t*)stbi_load(path.c_str(), &width_, &height_, nullptr, 4);
    if(data_ == nullptr) {
        throw std::runtime_error("failed to load image");
    }
}

Image::Image(int width, int height) : width_(width), height_(height) {
    data_ = (uint32_t*)malloc(width * height * 4);
    std::memset(data_, 0, width * height * 4);
}

Image::Image(const Image& other) {
    width_ = other.width_;
    height_ = other.height_;
    data_ = (uint32_t*)malloc(width_ * height_ * 4);
    std::memcpy(data_, other.data_, width_ * height_ * 4);
}

Image& Image::operator=(const Image& other) {
    if(this != &other) {
        free(data_);
        width_ = other.width_;
        height_ = other.height_;
        data_ = (uint32_t*)malloc(width_ * height_ * 4);
        std::memcpy(data_, other.data_, width_ * height_ * 4);
    }
    return *this;
}

Image::Image(Image&& other) noexcept {
    data_ = other.data_;
    width_ = other.width_;
    height_ = other.height_;

    other.data_ = nullptr;
    other.width_ = 0;
    other.height_ = 0;
}

Image::~Image() {
    free(data_);
}

void Image::save_png(const std::string& path) const {
    stbi_write_png(path.c_str(), width_, height_, 4, data_, width_ * 4);
}

void Image::copy_to(Image& other, int x, int y) const {
    assert(x >= 0 && y >= 0 && x + width_ <= other.width_ && y + height_ <= other.height_);
    for(int y1 = 0; y1 < height_; ++y1) {
        for(int x1 = 0; x1 < width_; ++x1) {
            other(x + x1, y + y1) = (*this)(x1, y1);
        }
    }
}

Image Image::slice(int x, int y, int width, int height) const {
    assert(x >= 0 && y >= 0 && x + width <= this->width_ && y + height <= this->height_);
    Image result(width, height);
    for(int y1 = 0; y1 < height; ++y1) {
        for(int x1 = 0; x1 < width; ++x1) {
            result(x1, y1) = (*this)(x + x1, y + y1);
        }
    }
    return result;
}

void Image::fill(int x, int y, int width, int height, uint32_t color) {
    assert(x >= 0 && y >= 0 && x + width <= this->width_ && y + height <= this->height_);

    for(int y1 = 0; y1 < height; ++y1) {
        for(int x1 = 0; x1 < width; ++x1) {
            (*this)(x + x1, y + y1) = color;
        }
    }
}
