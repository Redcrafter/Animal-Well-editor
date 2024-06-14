#include "image.hpp"

#include <cstring>
#include <span>
#include <stdexcept>
#include <string>

#pragma warning disable
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#pragma warning restore

Image::Image(std::span<const uint8_t> data) {
    data_ = (uint32_t*)stbi_load_from_memory(data.data(), data.size(), &width, &height, nullptr, 4);
    if(data_ == nullptr) {
        throw std::runtime_error("failed to load image");
    }
}

Image::Image(const std::string& path) {
    data_ = (uint32_t*)stbi_load(path.c_str(), &width, &height, nullptr, 4);
    if(data_ == nullptr) {
        throw std::runtime_error("failed to load image");
    }
}

Image::Image(int width, int height) : width(width), height(height) {
    data_ = (uint32_t*)malloc(width * height * 4);
    std::memset(data_, 0, width * height * 4);
}

Image::Image(Image&& other) noexcept {
    data_ = other.data_;
    width = other.width;
    height = other.height;

    other.data_ = nullptr;
    other.width = 0;
    other.height = 0;
}

Image::~Image() {
    free(data_);
}

void Image::save_png(const std::string& path) {
    stbi_write_png(path.c_str(), width, height, 4, data_, width * 4);
}
