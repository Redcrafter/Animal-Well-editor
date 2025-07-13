#include "image.hpp"

#include <cstring>
#include <stdexcept>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#pragma warning(push)
#pragma warning(disable : 4996)

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#pragma warning(pop)
#pragma gcc diagnostic pop
#pragma clang diagnostic pop

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

Image Image::copy() const {
    Image res(width_, height_);
    std::memcpy(res.data_, data_, width_ * height_ * 4);
    return res;
}

Image::Image(Image&& other) noexcept {
    data_ = other.data_;
    width_ = other.width_;
    height_ = other.height_;

    other.data_ = nullptr;
    other.width_ = 0;
    other.height_ = 0;
}

Image& Image::operator=(Image&& other) noexcept {
    if(&other == this)
        return *this;

    free(data_);

    data_ = other.data_;
    width_ = other.width_;
    height_ = other.height_;

    other.data_ = nullptr;
    other.width_ = 0;
    other.height_ = 0;

    return *this;
}

Image::~Image() {
    free(data_);
}

void Image::save_png(const std::string& path) const {
    stbi_write_png(path.c_str(), width_, height_, 4, data_, width_ * 4);
}

static void write_func(void* context, void* data, int size) {
    std::vector<uint8_t>& res = *(std::vector<uint8_t>*)context;

    auto ptr = (uint8_t*)data;
    for(size_t i = 0; i < size; i++) {
        res.push_back(ptr[i]);
    }
}

std::vector<uint8_t> Image::save_png() const {
    std::vector<uint8_t> res;
    stbi_write_png_to_func(&write_func, &res, width_, height_, 4, data_, width_ * 4);
    return res;
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

Image Image::scale(int xScale, int yScale) const {
    Image result(width_ * xScale, height_ * yScale);

    for(int y = 0; y < height_; ++y) {
        for(int x = 0; x < width_; ++x) {
            const auto pix = (*this)(x, y);

            for(int y1 = 0; y1 < yScale; ++y1) {
                for(int x1 = 0; x1 < xScale; ++x1) {
                    result(x * xScale + x1, y * yScale + y1) = pix;
                }
            }
        }
    }

    return result;
}
