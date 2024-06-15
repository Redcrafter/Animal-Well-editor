#include "image.hpp"

#include <algorithm>
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

Image pack(const std::vector<Image>& images, int width, int height) {
    Image result(width, height);
    std::vector<glm::ivec2> locations(images.size());

    // if x > 0 stores max possible size at position
    // if x < 0 stores next free location in x and start of used area in y
    std::vector<glm::ivec2> avalible(width * height);

    // stores images sorted by size
    std::vector<const Image*> sorted(images.size());
    for(size_t i = 0; i < images.size(); i++)
        sorted[i] = &images[i];
    std::stable_sort(sorted.begin(), sorted.end(), [](const Image* a, const Image* b) { return a->size().x * a->size().y > b->size().x * b->size().y; });

    for(int y = 0; y < height; ++y) {
        for(int x = 0; x < width; ++x) {
            avalible[x + y * width] = {width - x, height - y};
        }
    }

    glm::ivec2 lastSize = {0, 0};
    int x = 0;
    int y = 0;

    for(int i = 0; i < sorted.size(); i++) {
        const auto size = sorted[i]->size();

        // continue from same position if size has not changed
        if(size != lastSize) {
            x = 0;
            y = 0;
        }
        lastSize = size;

        while(y + size.y <= height) {
            if(x + size.x > width) {
                x = 0;
                y++;
                continue;
            }

            const auto v = avalible[x + y * width];
            if(v.x < 0) { // skip over used section
                x = -v.x;
                continue;
            }
            if(size.x > v.x) { // skip to next section
                x += v.x;
                continue;
            }
            if(size.y > v.y) {
                // search backwards to take reasonably big steps
                auto step = size.x;
                while(size.y <= avalible[(x + step - 1) + y * width].y) {
                    step = step / 2;
                }
                x += step;
                continue;
            }

            // store image location
            locations[i] = {x, y};

            // set bottom row to stop up scan
            for(int x1 = 0; x1 < size.x; x1++) {
                avalible[(x + x1) + (y + size.y - 1) * width] = {-(x + size.x), x};
            }

            for(int y1 = 0; y1 < size.y; y1++) {
                int l = x;
                int r = x + size.x;

                if(x > 0) {
                    const auto el = avalible[(x - 1) + (y + y1) * width];
                    if(el.x < 0) {
                        // left section is already used. set new left to start of that section
                        l = el.y;
                    } else {
                        // go left and update new max size
                        for(int x1 = x - 1; x1 >= 0; x1--) {
                            if(avalible[x1 + (y + y1) * width].x < 0) break;
                            avalible[x1 + (y + y1) * width].x = x - x1;
                        }
                    }
                }

                if(r < width) {
                    const auto el = avalible[r + (y + y1) * width];
                    if(el.x < 0) {
                        // right section is already used. set new right to end of that section
                        r = -el.x;
                    }
                }

                // update left and right pointers to increase skipping
                avalible[(l) + (y + y1) * width].x = -r;
                avalible[(r - 1) + (y + y1) * width].y = l;
            }

            // go up and update new max size
            for(int x1 = 0; x1 < size.x; x1++) {
                for(int y1 = y - 1; y1 >= 0; y1--) {
                    if(avalible[(x + x1) + y1 * width].x < 0) break;
                    avalible[(x + x1) + y1 * width].y = y - y1;
                }
            }

            x += size.x;
            break;
        }
    }

    // copy images to result
    for(int i = 0; i < sorted.size(); i++) {
        const auto& img = *sorted[i];
        auto size = img.size();
        auto pos = locations[i];

        // copy img to result
        for(int y1 = 0; y1 < size.y; y1++) {
            for(int x1 = 0; x1 < size.x; x1++) {
                // assert(result(x + x1, y + y1) == 0);
                result(pos.x + x1, pos.y + y1) = img(x1, y1);
            }
        }
    }

    return result;
}
