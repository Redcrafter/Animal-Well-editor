#pragma once

#include <cassert>
#include <cstring>
#include <string>
#include <vector>

#include "stb_image.h"
#include "stb_image_write.h"

class Texture {
   private:
    int *data;
    int width;
    int height;
    int channels = 4;  // should be 4

   public:
    Texture(int width, int height) : width(width), height(height) {
        data = (int *)std::malloc(width * height * 4);
        std::memset(data, 0, width * height * 4);
    }
    Texture(const std::vector<uint8_t> &data) : Texture((const char *)data.data(), data.size()) {}
    Texture(const char *ptr, int length) {
        data = (int *)stbi_load_from_memory((const stbi_uc *)ptr, length, &width, &height, &channels, 4);
        assert(channels == 4);
    }
    Texture(Texture &&) = delete;
    ~Texture() {
        free(data);
    }

    void save(const std::string &path) const {
        save(path.c_str());
    }
    void save(const char *path) const {
        stbi_write_png(path, width, height, channels, data, 0);
    }

    int &operator()(int x, int y) {
        return data[x + y * width];
    }
    int operator()(int x, int y) const {
        if (x > width || y > height) return 0;
        return data[x + y * width];
    }

    int get(int x, int y) const {
        if (x > width || y > height) return 0;
        return data[x + y * width];
    }
    void set(int x, int y, int value) {
        if (x > width || y > height) return;
        data[x + y * width] = value;
    }
};
