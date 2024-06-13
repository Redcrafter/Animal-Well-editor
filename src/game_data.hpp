#pragma once
#include <array>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "structures/asset.hpp"
#include "structures/map.hpp"
#include "structures/sprite.hpp"
#include "structures/tile.hpp"

#include "dos_parser.hpp"

constexpr const char* mapNames[5] = {"Overworld", "CE temple", "Space", "Bunny temple", "Time Capsule"};
constexpr int mapIds[5] = {300, 157, 193, 52, 222};

class GameData {
  private:
    std::vector<char> raw;

    SegmentData sections;
    std::vector<uint8_t> decryptBuffer;

  public:
    std::span<asset_entry> assets;

    std::array<Map, 5> maps {};
    std::unordered_map<uint32_t, SpriteData> sprites;
    std::vector<uv_data> uvs;

    bool loaded = false;

    static GameData load(const std::string& path);
    void save(const std::string& path) const;

    std::span<uint8_t> get_asset(int id);

    void apply_changes();
    void patch_renderdoc();
    void patch_save_path(const std::string& save_name);
    void replace_asset(std::span<const uint8_t> data, int id);
};
