#include "game_data.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "windows/errors.hpp"
#include "dos_parser.hpp"

std::unordered_map<int, size_t> knownHashes = {
    { 300, 5286508738600918491 },
    { 157, 5230261472608991529 },
    { 193, 5335982172013659759 },
    { 52, 7588605218054849143 },
    { 222, 10938843732996440403 },
    { 71, 10232361431741057326 },
    { 140, 18195876542649170121 },
    { 203, 7744059127945101238 },
    { 121, 1982974962191440380 },
    { 25, 1642779073074381889 },
    { 103, 16518840249594051165 },
    { 198, 11438456890535868887 },
    { 199, 12596913273590333529 },
    { 153, 15270860835853640192 },
    { 143, 3977768832574926095 },
    { 154, 1546354327620110293 },
    { 155, 4695324776549716864 },
    { 4, 16704420867261234471 },
    { 211, 17399940233443191635 },
    { 144, 4414420444326070208 },
    { 10, 10694532469001086052 },
    { 126, 10334027811173457435 },
    { 102, 868696414028365093 },
    { 165, 13282462228001022799 },
    { 267, 16137730420776985062 },
    { 40, 14571228405254950526 },
    { 219, 3897113761151844417 },
    { 93, 10890795752749026834 },
    { 92, 18438805045270985706 },
    { 248, 14996187180944217906 },
    { 49, 10388377191543627335 },
    { 174, 17305609080884061470 },
    { 68, 16350281252678514769 },
    { 84, 13944671899931500223 },
    { 95, 18185433649740802242 },
    { 94, 7393533698980787811 },
    { 75, 10800456579309619644 },
    { 110, 15461943434058153985 },
    { 0, 280615995088651467 },
    { 57, 10776084522252335481 },
    { 302, 12472522916275584237 },
    { 221, 810834073564742108 },
    { 5, 15664270950351138776 },
    { 142, 72631783794956031 },
    { 301, 17743137360367020774 },
    { 151, 11132630192715811650 },
    { 150, 8763700859862934168 },
    { 168, 17396884670925115809 },
    { 296, 17747839143158954679 },
    { 72, 16509255698638305701 },
    { 167, 13075460782470218565 },
    { 186, 13890260386994271883 },
    { 119, 9264077196129076145 },
    { 51, 12947876777834350350 },
    { 208, 5692618240829850282 },
    { 271, 10543262647682533362 },
    { 272, 1887782844680680727 },
    { 190, 13189659541222952533 },
    { 251, 16161007606787088142 },
    { 206, 6256291871835531479 },
    { 205, 1040734355826792677 },
    { 276, 17347799097355877475 },
    { 182, 9303142023206089983 },
    { 164, 6998087864294379613 },
    { 60, 16437962462050246041 },
    { 61, 15254030604703143217 },
    { 209, 5618994565493409492 },
    { 183, 7392861702259038279 },
    { 184, 11128118633571785412 },
    { 35, 4476454560712818515 },
    { 27, 8972795414962395977 },
    { 38, 14914165203051143878 },
    { 108, 7447696266200228411 },
    { 109, 6355020049135525765 },
    { 189, 13934636668609342263 },
    { 239, 8707347335571919900 },
    { 202, 16612302789085216244 },
    { 268, 13595223802245428581 },
    { 161, 16151826123073778738 },
    { 159, 16151826123073778738 },
    { 160, 28293711356471688 },
    { 34, 12176303476164512064 },
    { 214, 17537253043673507279 },
    { 85, 2306577632761986833 },
    { 86, 10636013142171085637 },
    { 87, 7976115029197224329 },
    { 88, 9470126905862518241 },
    { 89, 17799602810201604089 },
    { 90, 17799602810201604089 },
    { 91, 11346173183972229457 },
    { 282, 13679263975109995991 },
    { 135, 4791894686543775221 },
    { 59, 10269183653452283061 },
    { 237, 9236794479199753463 },
    { 36, 16507008656040608882 },
    { 123, 14893278649659361426 },
    { 122, 18254767120939628913 },
    { 45, 15289332096472164441 },
    { 8, 14660065665821786401 },
    { 83, 2276432239215924492 },
    { 124, 8927099900951650360 },
    { 6, 6410686435498593812 },
    { 128, 6603106236643821153 },
    { 7, 2622862046590626142 },
    { 281, 5627191400625121895 },
    { 218, 14540939221093811187 },
    { 152, 816455460641018501 },
    { 70, 4146772280305526167 },
    { 200, 10386416872941955068 },
    { 156, 6346852931458676495 },
    { 113, 16962313299152533767 },
    { 279, 4189383835178211373 },
    { 47, 16203473433850226682 },
    { 104, 12982172928911533573 },
    { 170, 2109754043398652842 },
    { 111, 15544977803506775432 },
    { 249, 2239993916743206869 },
    { 169, 2930142988492762814 },
    { 48, 11403250436845003084 },
    { 252, 7080679073552575380 },
    { 146, 13795526650101503731 },
    { 274, 15764754574011414200 },
    { 283, 13803088293509542304 },
    { 178, 871800059200800987 },
    { 29, 16804568628529814025 },
    { 278, 728949602536188638 },
    { 197, 18040221321673370808 },
    { 54, 15607683879478190450 },
    { 195, 739032799000195145 },
    { 194, 15913093305535402467 },
    { 148, 11139534942999560688 },
    { 147, 11031604412794528715 },
    { 158, 16645389099261167022 },
    { 176, 6564629261409662377 },
    { 250, 3168105005263113165 },
    { 58, 18371264113304823713 },
    { 31, 3572329693554742272 },
    { 130, 4816202201073365419 },
    { 131, 4816202201073365419 },
    { 132, 4816202201073365419 },
    { 215, 15131504976672207271 },
    { 188, 9817992694422752489 },
    { 55, 17181956214048143195 },
    { 136, 3961958370523927087 },
    { 133, 4816202201073365419 },
    { 120, 3489760928866745750 },
    { 63, 3897503422527611994 },
    { 187, 7773204180526074864 },
    { 163, 16355688661843544526 },
    { 112, 13520791118492966707 },
    { 201, 15484877499534936984 },
    { 33, 8050494817421316406 },
    { 106, 13300137491142406672 },
    { 105, 14457817266875762703 },
    { 149, 5825681437907344792 },
    { 39, 7710664052675440831 },
    { 53, 2406532803729001495 },
    { 127, 3159745524791484470 },
    { 32, 5954906883877334142 },
    { 28, 2729900650244020410 },
    { 162, 6773732642368564158 },
    { 217, 5035524815660143289 },
    { 175, 235842778747612680 },
    { 254, 6191836559123737457 },
    { 179, 15075917697486370393 }
};

std::vector<uint8_t> readFile(const std::string& path) {
    if(!std::filesystem::exists(path))
        throw std::runtime_error("File not found");

    std::ifstream testFile(path, std::ios::binary);
    testFile.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    return std::vector<uint8_t>(std::istreambuf_iterator(testFile), std::istreambuf_iterator<char>());
}

static std::optional<std::vector<uint8_t>> tryReadFile(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path))
        return {};

    std::ifstream testFile(path, std::ios::binary);
    testFile.exceptions(std::ifstream::badbit | std::ifstream::failbit);
    return std::vector<uint8_t>(std::istreambuf_iterator(testFile), std::istreambuf_iterator<char>());
}

static size_t hash(const std::vector<uint8_t>& data) {
    size_t result = 2166136261U;
    for(auto&& el : data) {
        result = (16777619 * result) ^ el;
    }
    return result;
}

template<typename T>
bool equal(const std::vector<T>& a, const std::vector<T>& b) {
    if(a.size() != b.size())
        return false;

    for(size_t i = 0; i < a.size(); i++) {
        if(a[i] != b[i])
            return false;
    }
    return true;
}

// only write the file if the hash has changed
static void writeFileIfChanged(const std::filesystem::path& path, const std::vector<uint8_t>& data, int assetId) {
    // printf(std::format("{} {}\n", assetId, hash(data)).c_str());

    assert(knownHashes.contains(assetId));
    if(hash(data) != knownHashes[assetId]) {
        std::ofstream file(path, std::ios::binary);
        file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
        file.write((char*)data.data(), data.size());
    }
}

GameData GameData::load_exe(const std::string& path) {
    GameData data;
    data.raw = readFile(path);
    data.sections = getSegmentOffsets(data.raw);

    assert(data.sections.data.size() >= sizeof(asset_entry) * 676);
    data.assets = std::span((const asset_entry*)data.sections.data.data(), 676);

    data.bufferFromExe();
    if(!data.testAssetHashes()) {
        error_dialog.warning("Loaded exe differs from unmodified game files.");
    }

    data.loaded = true;

    return data;
}

void GameData::load_folder(const std::string& path) {
    bufferFromExe(); // reload original assets
    auto p = std::filesystem::path(path);

    for(size_t i = 0; i < 5; i++) {
        auto data = tryReadFile(p / std::format("{}.map", mapIds[i]));
        if(data) {
            auto map = Map(*data);
            if(map.coordinate_map != maps[i].coordinate_map) {
                error_dialog.warning("Map structure differs from previously loaded map.\nMight break things so be careful.");
            }
            maps[i] = map;
        }
    }

    for(const auto [_, asset_id, tile_id] : spriteMapping) {
        auto data = tryReadFile(p / std::format("{}.sprite", asset_id));
        if(data) sprites[tile_id] = SpriteData(*data);
    }

    auto uvs_ = tryReadFile(p / "254.uvs");
    if(uvs_) uvs = uv_data::load(*uvs_);

    auto ambient_ = tryReadFile(p / "179.ambient");
    if(ambient_) ambient = LightingData::parse(*ambient_);

    auto atlas_ = tryReadFile(p / "255.png");
    if(atlas_) atlas = Image(*atlas_);
    auto bunny_ = tryReadFile(p / "30.png");
    if(bunny_) bunny = Image(*bunny_);
    auto time_capsule_ = tryReadFile(p / "277.png");
    if(time_capsule_) time_capsule = Image(*time_capsule_);

    for(size_t i = 11; i <= 24; i++) {
        auto bg = tryReadFile(p / std::format("{}.png", i));
        if(bg) backgrounds[i - 11] = Image(*bg);
    }
    // index 25 is skipped for some reason
    auto bg_ = tryReadFile(p / "26.png");
    if(bg_) backgrounds[14] = Image(*bg_);
}

void GameData::save_folder(const std::string& path) {
    auto p = std::filesystem::path(path);

    for(size_t i = 0; i < 5; i++) {
        writeFileIfChanged(p / std::format("{}.map", mapIds[i]), maps[i].save(), mapIds[i]);
    }
    for(const auto [_, asset_id, tile_id] : spriteMapping) {
        writeFileIfChanged(p / std::format("{}.sprite", asset_id), sprites[tile_id].save(), asset_id);
    }

    writeFileIfChanged(p / "254.uvs", uv_data::save(uvs), 254);
    writeFileIfChanged(p / "179.ambient", LightingData::save(ambient), 179);

    // no need to save textures since the editor can't change them anyway
}

std::vector<uint8_t> GameData::get_asset(int id) {
    assert(id >= 0 && id < assets.size());
    auto& asset = assets[id];
    auto dat = sections.get_rdata_ptr(asset.ptr, asset.length);

    std::vector<uint8_t> buffer;
    if(tryDecrypt(asset, dat, buffer)) {
        return buffer;
    }
    return std::vector<uint8_t>(dat.begin(), dat.end());
}

bool GameData::testAssetHashes() {
    for(size_t i = 0; i < 5; i++) {
        if(hash(maps[i].save()) != knownHashes[mapIds[i]])
            return false;
    }
    for(const auto [_, asset_id, tile_id] : spriteMapping) {
        if(hash(sprites[tile_id].save()) != knownHashes[asset_id])
            return false;
    }

    if(hash(uv_data::save(uvs)) != knownHashes[254])
        return false;

    if(hash(uv_data::save(uvs)) != knownHashes[254])
        return false;

    return true;
}


void GameData::bufferFromExe() {
    for(size_t i = 0; i < 5; i++) {
        auto dat = get_asset(mapIds[i]);
        maps[i] = Map(dat);
        assert(i == 4 || equal(dat, maps[i].save())); // map 4 has some random padding at the end
    }

    for(const auto [_, asset_id, tile_id] : spriteMapping) {
        auto dat = get_asset(asset_id);
        sprites[tile_id] = SpriteData(dat);
        assert(equal(dat, sprites[tile_id].save()));
    }

    uvs = uv_data::load(get_asset(254));
    assert(equal(get_asset(254), uv_data::save(uvs)));

    ambient = LightingData::parse(get_asset(179));
    assert(equal(get_asset(179), LightingData::save(ambient)));

    atlas = Image(get_asset(255));
    bunny = Image(get_asset(30));
    time_capsule = Image(get_asset(277));

    for(size_t i = 11; i <= 24; i++) {
        backgrounds[i - 11] = Image(get_asset(i));
    }
    // index 25 is skipped for some reason
    backgrounds[14] = Image(get_asset(26));
}
