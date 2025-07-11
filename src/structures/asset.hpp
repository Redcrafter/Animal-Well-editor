#pragma once

#include <cstdint>
#include <span>
#include <vector>

enum class AssetType : uint8_t {
    Text = 0,
    MapData = 1,
    Png = 2,
    Ogg = 3,
    SpriteData = 5,
    Shader = 7,
    Font = 8,

    Encrypted_Text = 64, // only used for .xps
    Encrypted_MapData = 65,
    Encrypted_Png = 66,
    Encrypted_Ogg = 67
};

struct asset_entry {
    AssetType type;
    uint8_t unknown1[7];

    uint64_t ptr;
    uint32_t length;

    uint32_t unknown2;
    uint64_t unknown3;

    uint8_t unknown4[16];
};
static_assert(sizeof(asset_entry) == 0x30);

bool tryDecrypt(const asset_entry& item, std::span<const uint8_t> data, std::vector<uint8_t>& out);

std::vector<uint8_t> encrypt(std::span<const uint8_t> data, int keyNum);
