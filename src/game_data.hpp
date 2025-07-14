#pragma once
#include <array>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "structures/ambient.hpp"
#include "structures/asset.hpp"
#include "structures/map.hpp"
#include "structures/sprite.hpp"
#include "structures/tile.hpp"

#include "image.hpp"
#include "dos_parser.hpp"

constexpr const char* mapNames[5] = {"Overworld", "CE temple", "Space", "Bunny temple", "Time Capsule"};
constexpr int mapIds[5] = {300, 157, 193, 52, 222};

class GameData {
  private:
    std::vector<uint8_t> raw;
    SegmentData sections;

  public:
    std::span<const asset_entry> assets;

    std::array<Map, 5> maps {};
    std::unordered_map<uint32_t, SpriteData> sprites;
    std::vector<uv_data> uvs;
    std::vector<LightingData> ambient;

    Image atlas, bunny, time_capsule;
    std::array<Image, 15> backgrounds;

    bool loaded = false;

    static GameData load_exe(const std::string& path);
    void load_folder(const std::string& path);
    void save_folder(const std::string& path) const;

    std::vector<uint8_t> get_asset(int id);

  private:
    void backup_assets(const std::string& path) const;

    bool testAssetHashes();
    void bufferFromExe();
};

std::vector<uint8_t> readFile(const std::string& path);
