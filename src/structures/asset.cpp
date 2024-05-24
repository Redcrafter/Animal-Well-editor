#include "asset.hpp"

#include <array>
#include <fstream>

#include "../aes.hpp"
#include "../dos_parser.hpp"

static std::array<uint8_t, 16> keys[3] = {
    {'G', 'o', 'o', 'd', 'L', 'U', 'c', 'K', 'M', 'y', 'F', 'r', 'i', 'E', 'n', 'd'},
    {0xC9, 0x9B, 0x64, 0x96, 0x5C, 0xCE, 0x04, 0xF0, 0xF5, 0xCB, 0x54, 0xCA, 0xC9, 0xAB, 0x62, 0xC6},  // bunny works for 30/52
    {0x11, 0x14, 0x18, 0x14, 0x88, 0x82, 0x42, 0x82, 0x28, 0x24, 0x88, 0x82, 0x11, 0x18, 0x44, 0x11}   // time capsule works for 222/277/377
};

std::vector<char> readFile(const char *path) {
    std::ifstream testFile(path, std::ios::binary);
    return std::vector(std::istreambuf_iterator(testFile), std::istreambuf_iterator<char>());
}

bool tryDecrypt(const asset_entry &item, std::span<const uint8_t> data, std::vector<uint8_t>& out) {
    if (((uint8_t)item.type & 192) == 64) {
        for(auto& k : keys) {
            if (decrypt(data, k, out)) {
                return true;
            }
        }
    }
    return false;
}

std::vector<uint8_t> encrypt(std::span<const uint8_t> data, int keyNum) {
    return encrypt(data, keys[keyNum]);
}
