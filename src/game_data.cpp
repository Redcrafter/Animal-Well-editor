#include "game_data.hpp"

#include <algorithm>
#include <codecvt>
#include <fstream>
#include <locale>

GameData GameData::load(const std::string& path) {
    GameData data;
    data.raw = readFile(path.c_str());
    data.sections = getSegmentOffsets(data.raw);

    assert(data.sections.data.size() >= sizeof(asset_entry) * 676);
    data.assets = std::span((asset_entry*)data.sections.data.data(), 676);

    for(size_t i = 0; i < 5; i++) {
        data.maps[i] = Map(data.get_asset(mapIds[i]));
    }

    for(const auto [_, asset_id, tile_id] : spriteMapping) {
        data.sprites[tile_id] = parse_sprite(data.get_asset(asset_id));
    }

    data.uvs = parse_uvs(data.get_asset(254));
    data.ambient = LightingData::parse(data.get_asset(179));

    data.loaded = true;

    // auto test = data;
    // test.apply_changes();

    return data;
}

void GameData::save(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
    file.write(raw.data(), raw.size());
}

std::span<uint8_t> GameData::get_asset(int id) {
    assert(id >= 0 && id < assets.size());
    auto& asset = assets[id];
    auto dat = sections.get_rdata_ptr(asset.ptr, asset.length);

    if(tryDecrypt(asset, dat, decryptBuffer)) {
        return decryptBuffer; // could cause issues when loading multiple resources at the same time
    }
    return dat;
}

void GameData::apply_changes() {
    for(size_t i = 0; i < 5; i++) {
        replace_asset(maps[i].save(), mapIds[i]);
    }
    for(const auto [_, asset_id, tile_id] : spriteMapping) {
        replace_asset(save_sprite(sprites[tile_id]), asset_id);
    }
    replace_asset(save_uvs(uvs), 254);
    replace_asset(LightingData::save(ambient), 179);
}

void GameData::patch_renderdoc(bool patch) {
    constexpr char pattern[] = "enderdoc.dll";
    auto res = std::search(raw.begin(), raw.end(), std::begin(pattern), std::end(pattern));
    if(res == raw.end()) return;

    --res;
    if(patch && *res != 'l') {
        *res = 'l'; // replace first letter with 'l'
    }
    if(!patch && *res != 'r') {
        *res = 'r'; // undo patch
    }
}

void GameData::patch_save_path(const std::string& save_name) {
    const char* default_save = (const char*)(L"AnimalWell.sav");
    auto res = std::search(raw.begin(), raw.end(), default_save, default_save + 30);

    if(res != raw.end()) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        auto wstr = converter.from_bytes(save_name);

        for(size_t i = 0; i < wstr.size() + 1; ++i) {
            auto c = wstr[i];
            *res = c & 0xFF;
            ++res;
            *res = c >> 8;
            ++res;
        }
    }
}

void GameData::replace_asset(std::span<const uint8_t> data, int id) {
    auto& asset = assets[id];
    auto ptr = sections.get_rdata_ptr(asset.ptr, asset.length);

    if(((uint8_t)asset.type & 192) == 64) {
        int key = 0; // 193, 212, 255, 300
        if(id == 30 || id == 52) key = 1;
        else if(id == 222 || id == 277 || id == 377) key = 2;

        auto enc = encrypt(data, key);

        if(enc.size() > asset.length) {
            throw std::runtime_error("Failed to save asset " + std::to_string(id) + " because it was too big");
        }

        // assert(std::memcmp(ptr.data(), enc.data(), enc.size()) == 0);
        // asset.length = enc.size(); // keep default value cause game doesn't really use length anyway
        std::memcpy(ptr.data(), enc.data(), enc.size());
    } else {
        if(data.size() > asset.length) {
            throw std::runtime_error("Failed to save asset " + std::to_string(id) + " because it was too big");
        }

        // assert(std::memcmp(ptr.data(), data.data(), data.size()) == 0);
        std::memcpy(ptr.data(), data.data(), data.size());
    }
}
