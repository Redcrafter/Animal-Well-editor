#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "aes.hpp"

#include <glm/glm.hpp>

enum class AssetType {
    Text = 0,
    MapData = 1,
    Png = 2,
    Ogg = 3,
    SpriteData = 5,
    Shader = 7,
    Font = 8,

    XPS = 9,
    /*
    Encrypted_MapData = 65,
    Encrypted_Png = 66,
    Encrypted_Ogg = 67
    */
};

// starts at 0x020E9A00 or 1420eb000 in ghidra
struct Asset {
    // 0 = txt
    // 1 = map data
    // 2 = png
    // 3 = ogg
    // 5 = tile data
    // 7 = shaders?
    // 8 = BMF font format?
    // 64 = xps
    // 65 = map data
    // 66 = png
    // 67 = ogg
    // encrypted if (type & 192) == 64
    uint8_t type;
    uint8_t unknown1[7];

    uint64_t ptr;  // offset by 0x140001200
    uint32_t length;

    uint32_t unknown2;
    uint64_t unknown3;

    uint8_t unknown4[16];
};

static_assert(sizeof(Asset) == 0x30);

auto readFile(const char *path) {
    std::ifstream testFile(path, std::ios::binary);
    std::vector<char> fileContents((std::istreambuf_iterator<char>(testFile)), std::istreambuf_iterator<char>());
    return fileContents;
}

bool contains(const std::string &str, const char *search) {
    return str.find(search) != std::string::npos;
}

struct MapHeader {
    uint32_t signature1;
    uint16_t roomCount;
    uint8_t idk1;
    uint8_t idk2;
    uint32_t unused;
    uint32_t signature2;
};

struct MapTile {
    uint16_t tile_id;
    uint8_t idk1;

    union {
        struct {
            bool horizontal_mirror : 1;
            bool vertical_mirror : 1;
            bool rotate_90 : 1;
            bool rotate_180 : 1;
        };
        uint8_t flags;
    };
};
static_assert(sizeof(MapTile) == 4);

struct Room {
    uint8_t x;
    uint8_t y;

    uint8_t bgId;
    uint8_t waterLevel;
    uint32_t idk3;

    MapTile tiles[2][22][40];
};

static_assert(sizeof(Room) == 0x1b88);

struct TileMapping {
    int internal_id;
    int asset_id;
    int tile_id;
};

// extracted from game code
TileMapping spriteMapping[] = {
    {0, 0x47, 0x160},
    {1, 0x8c, 0x161},
    {2, 0xcb, 0x16b},
    {3, 0x79, 0x93},
    {4, 0x19, 0x16d},
    {5, 0x67, 0x16f},
    {6, 0xc6, 0x121},
    {7, 199, 0x120},
    {8, 0x99, 0x17b},
    {9, 0x8f, 0x17d},
    {10, 0x9a, 0x184},
    {0xb, 0x9b, 0x186},
    {0xc, 4, 0x18c},
    {0xd, 0xd3, 0x1a0},
    {0xe, 0x90, 0x19f},
    {0xf, 10, 0x1a7},
    {0x10, 0x7e, 0x1a5},
    {0x11, 0x66, 0x1a6},
    {0x12, 0xa5, 0x1a4},
    {0x13, 0x10b, 0x30b},
    {0x14, 0x28, 0x1a8},
    {0x15, 0xdb, 0x1ac},
    {0x16, 0x5d, 0x1dc},
    {0x17, 0x5c, 5},
    {0x18, 0xf8, 0xdb},
    {0x19, 0x31, 0x40},
    {0x1a, 0xae, 0x56},
    {0x1b, 0x44, 0x43},
    {0x1c, 0x54, 0x6a},
    {0x1d, 0x5f, 0x6b},
    {0x1e, 0x5e, 0xed},
    {0x1f, 0x4b, 0x22},
    {0x20, 0x6e, 0xa3},
    {0x21, 0, 0x124},
    {0x22, 0x39, 0x125},
    {0x23, 0x12e, 0x151},
    {0x24, 0xdd, 0x127},
    {0x25, 5, 0x128},
    {0x26, 0x8e, 0x155},
    {0x27, 0x12d, 0x14b},
    {0x28, 0x97, 0x173},
    {0x29, 0x96, 0x179},
    {0x2a, 0xa8, 0x20f},
    {0x2b, 0x128, 0x213},
    {0x2c, 0x48, 0x21c},
    {0x2d, 0xa7, 0x220},
    {0x2e, 0xba, 0x221},
    {0x2f, 0x77, 0x225},
    {0x30, 0x33, 0x226},
    {0x31, 0xd0, 0x227},
    {0x32, 0x10f, 0x228},
    {0x33, 0x110, 0x229},
    {0x34, 0xbe, 0x22c},
    {0x35, 0xfb, 0x22e},
    {0x36, 0xce, 0x22f},
    {0x37, 0xcd, 0x230},
    {0x38, 0x114, 0x319},
    {0x39, 0xb6, 0x136},
    {0x3a, 0xa4, 0xce},
    {0x3b, 0x3c, 0x137},
    {0x3c, 0x3d, 0xc5},
    {0x3d, 0xd1, 0x234},
    {0x3e, 0xb7, 0x236},
    {0x3f, 0xb8, 0x30a},
    {0x40, 0x23, 0x235},
    {0x41, 0x1b, 0x238},
    {0x42, 0x26, 0x239},
    {0x43, 0x6c, 0x23b},
    {0x44, 0x6d, 0x244},
    {0x45, 0xbd, 0x245},
    {0x46, 0xef, 0x75},
    {0x47, 0xca, 0x105},
    {0x48, 0x10c, 0x247},
    {0x49, 0xa1, 0x249},
    {0x4a, 0x9f, 0x254},
    {0x4b, 0xa0, 0x255},
    {0x4c, 0x22, 0x24a},
    {0x4d, 0xd6, 0x24e},
    {0x4e, 0x55, 0x24b},
    {0x4f, 0x56, 0x24c},
    {0x50, 0x57, 0x24d},
    {0x51, 0x58, 0x25d},
    {0x52, 0x59, 0x2cb},
    {0x53, 0x5a, 0x302},
    {0x54, 0x5b, 0x308},
    {0x55, 0x11a, 0x256},
    {0x56, 0x87, 0x100},
    {0x57, 0x3b, 0x259},
    {0x58, 0xed, 0x25e},
    {0x59, 0x24, 0x25f},
    {0x5a, 0x7b, 0x260},
    {0x5b, 0x7a, 0x309},
    {0x5c, 0x2d, 0x264},
    {0x5d, 8, 0x265},
    {0x5e, 0x53, 0x26b},
    {0x5f, 0x7c, 0x26e},
    {0x60, 6, 0x271},
    {0x61, 0x80, 0x273},
    {0x62, 7, 0x279},
    {99, 0x119, 0x27c},
    {100, 0xda, 0x1bf},
    {0x65, 0x98, 0x1dd},
    {0x66, 0x46, 0x10d},
    {0x67, 200, 0x10c},
    {0x68, 0x9c, 0xfb},
    {0x69, 0x71, 0x142},
    {0x6a, 0x117, 0x28e},
    {0x6b, 0x2f, 0x291},
    {0x6c, 0x68, 0x29c},
    {0x6d, 0xaa, 0x2a2},
    {0x6e, 0x6f, 0x2a9},
    {0x6f, 0xf9, 0x2aa},
    {0x70, 0xa9, 0x2b5},
    {0x71, 0x30, 0x2ca},
    {0x72, 0xfc, 0x29b},
    {0x73, 0x92, 0x2cc},
    {0x74, 0x112, 0x2cd},
    {0x75, 0x11b, 0x2d0},
    {0x76, 0xb2, 0x2d1},
    {0x77, 0x1d, 0x31a},
    {0x78, 0x116, 0x2d2},
    {0x79, 0xc5, 0x2d7},
    {0x7a, 0x36, 0x2d8},
    {0x7b, 0xc3, 10},
    {0x7c, 0xc2, 0x8e},
    {0x7d, 0x94, 0x92},
    {0x7e, 0x93, 0xa8},
    {0x7f, 0x9e, 0x2da},
    {0x80, 0xb0, 0x2e7},
    {0x81, 0xfa, 0x2e8},
    {0x82, 0x3a, 0x2e9},
    {0x83, 0x1f, 0x301},
    {0x84, 0x82, 0x2ea},
    {0x85, 0x83, 0x2eb},
    {0x86, 0x84, 0x2ec},
    {0x87, 0xd7, 0x2ee},
    {0x88, 0xbc, 0x2ef},
    {0x89, 0x37, 0x2ed},
    {0x8a, 0x88, 0x2f0},
    {0x8b, 0x85, 0x2f1},
    {0x8c, 0x78, 0x2ff},
    {0x8d, 0x3f, 0x300},
    {0x8e, 0xbb, 0x303},
    {0x8f, 0xa3, 0x44},
    {0x90, 0x70, 0x306},
    {0x91, 0xc9, 0x314},
    {0x92, 0x21, 0x315},
    {0x93, 0x6a, 0x31c},
    {0x94, 0x69, 0x31d},
    {0x95, 0x95, 0x31e},
    {0x96, 0x27, 799},
    {0x97, 0x35, 0x323},
    {0x98, 0x7f, 0x324},
    {0x99, 0x20, 0x32e},
    {0x9a, 0x1c, 0x330},
    {0x9b, 0xa2, 0x333},
    {0x9c, 0xd9, 0x334},
    {0x9d, 0xaf, 0x33e}};

std::optional<std::vector<uint8_t>> tryDecrypt(const Asset &item, const char *data) {
    std::array<uint8_t, 16> keys[3] = {
        {'G', 'o', 'o', 'd', 'L', 'U', 'c', 'K', 'M', 'y', 'F', 'r', 'i', 'E', 'n', 'd'},
        {0xC9, 0x9B, 0x64, 0x96, 0x5C, 0xCE, 0x04, 0xF0, 0xF5, 0xCB, 0x54, 0xCA, 0xC9, 0xAB, 0x62, 0xC6},  // bunny works for 30/52
        {0x11, 0x14, 0x18, 0x14, 0x88, 0x82, 0x42, 0x82, 0x28, 0x24, 0x88, 0x82, 0x11, 0x18, 0x44, 0x11}   // time capsule works for 222/277/377
    };

    if ((item.type & 192) == 64) {
        for (auto &&k : keys) {
            auto res = decrypt(data, item.length, k);
            if (res.has_value())
                return res;
        }
    }

    return std::nullopt;
}

void dumpNormalFiles(const std::vector<char> &input) {
    auto assets = (const Asset *)(input.data() + 0x020E9A00);

    for (int i = 0; i < 676; i++) {
        auto &item = assets[i];

        std::string ext = ".bin";
        if (item.type == 0) {
            ext = ".txt";
        } else if (item.type == 2 || item.type == 66) {
            ext = ".png";
        } else if (item.type == 3 || item.type == 67) {
            ext = ".ogg";
        } else if (item.type == 64) {
            ext = ".xps";
        }
        auto actual_ptr = (const char *)(input.data() + (item.ptr - 0x140001200));

        std::ofstream file("out/" + std::to_string(i) + ext, std::ios::binary);
        if (auto decrypted = tryDecrypt(item, actual_ptr)) {
            file.write((char *)decrypted.value().data(), decrypted.value().size());
        } else {
            file.write(actual_ptr, item.length);
        }
    }
}

typedef glm::vec<2, uint16_t, glm::defaultp> shortVec2;

struct SpriteAnimation {
    uint16_t start;        // First composition in the animation
    uint16_t end;          // Last composition in the animation
    uint16_t frame_delay;  // After waiting this many frames, increment the composition number
};
static_assert(sizeof(SpriteAnimation) == 6);

struct SubSprite {
    shortVec2 atlas_pos;      // Atlas coordinates, relative to the entity's base coordinates
    shortVec2 composite_pos;  // Position of this subsprite on the composited sprite
    shortVec2 size;           // Size of the subsprite
};
static_assert(sizeof(SubSprite) == 12);

struct SpriteLayer {
    uint8_t : 1;
    bool is_normals1 : 1;
    bool is_normals2 : 1;
    bool uv_light : 1;
    bool is_conditional : 1;

    uint8_t alpha;
    bool is_visible;
};
static_assert(sizeof(SpriteLayer) == 3);

struct SpriteData {
    uint32_t magic;
    shortVec2 composite_size;
    uint16_t layer_count;
    uint16_t composition_count;
    uint8_t subsprite_count;
    uint8_t animation_count;

    std::vector<SpriteAnimation> animations;
    std::vector<uint8_t> compositions;
    std::vector<SubSprite> sub_sprites;
    std::vector<SpriteLayer> layers;
};

struct uv_data {
    shortVec2 pos;
    shortVec2 size;

    union {
	    struct {
            /*
            bool collides_left : 1;
            bool collides_right : 1;
            bool collides_up : 1;
            bool collides_down : 1;
            bool not_placeable : 1;
            bool additive : 1;

	    	bool has_normals : 1;
            bool hidden : 1;
            bool blocks_light : 1;
            bool obscures : 1;
            bool contiguous : 1;
            bool self_contiguous : 1;
            bool dirt : 1;
            bool uv_light : 1;
            */
	    };
        uint16_t flags;
    };
};
static_assert(sizeof(uv_data) == 10);

struct OutAsset {
    AssetType type;
    std::vector<uint8_t> data;
};

static auto loadGameAssets() {
    auto input = readFile("c:/Spiele/SteamLibrary/steamapps/common/Animal Well/Animal Well.exe");
    auto assets = (const Asset *)(input.data() + 0x020E9A00);

    std::vector<OutAsset> res;

    for (int i = 0; i < 676; i++) {
        auto &item = assets[i];
        auto actual_ptr = (const char *)(input.data() + (item.ptr - 0x140001200));

        std::vector<uint8_t> data;
        if (auto decrypted = tryDecrypt(item, actual_ptr)) {
            data = decrypted.value();
        } else {
            data.resize(item.length);
            std::memcpy(data.data(), actual_ptr, item.length);
        }
        res.push_back({(AssetType)item.type, std::move(data)});
    }

    return res;
}
