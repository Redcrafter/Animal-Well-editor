#include "game_data.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "dos_parser.hpp"
#include "windows/errors.hpp"

std::unordered_map<int, size_t> knownHashes = {
    {0, 280615995088651467ull},
    {4, 16704420867261234471ull},
    {5, 15664270950351138776ull},
    {6, 6410686435498593812ull},
    {7, 2622862046590626142ull},
    {8, 14660065665821786401ull},
    {10, 10694532469001086052ull},
    {25, 1642779073074381889ull},
    {27, 8972795414962395977ull},
    {28, 2729900650244020410ull},
    {29, 16804568628529814025ull},
    {31, 3572329693554742272ull},
    {32, 5954906883877334142ull},
    {33, 8050494817421316406ull},
    {34, 12176303476164512064ull},
    {35, 4476454560712818515ull},
    {36, 16507008656040608882ull},
    {38, 14914165203051143878ull},
    {39, 7710664052675440831ull},
    {40, 14571228405254950526ull},
    {45, 15289332096472164441ull},
    {47, 16203473433850226682ull},
    {48, 11403250436845003084ull},
    {49, 10388377191543627335ull},
    {51, 12947876777834350350ull},
    {52, 7588605218054849143ull},
    {53, 2406532803729001495ull},
    {54, 15607683879478190450ull},
    {55, 17181956214048143195ull},
    {57, 10776084522252335481ull},
    {58, 18371264113304823713ull},
    {59, 10269183653452283061ull},
    {60, 16437962462050246041ull},
    {61, 15254030604703143217ull},
    {63, 3897503422527611994ull},
    {68, 16350281252678514769ull},
    {70, 4146772280305526167ull},
    {71, 10232361431741057326ull},
    {72, 16509255698638305701ull},
    {75, 10800456579309619644ull},
    {83, 2276432239215924492ull},
    {84, 13944671899931500223ull},
    {85, 2306577632761986833ull},
    {86, 10636013142171085637ull},
    {87, 7976115029197224329ull},
    {88, 9470126905862518241ull},
    {89, 17799602810201604089ull},
    {90, 17799602810201604089ull},
    {91, 11346173183972229457ull},
    {92, 18438805045270985706ull},
    {93, 10890795752749026834ull},
    {94, 7393533698980787811ull},
    {95, 18185433649740802242ull},
    {102, 868696414028365093ull},
    {103, 16518840249594051165ull},
    {104, 12982172928911533573ull},
    {105, 14457817266875762703ull},
    {106, 13300137491142406672ull},
    {108, 7447696266200228411ull},
    {109, 6355020049135525765ull},
    {110, 15461943434058153985ull},
    {111, 15544977803506775432ull},
    {112, 13520791118492966707ull},
    {113, 16962313299152533767ull},
    {119, 9264077196129076145ull},
    {120, 3489760928866745750ull},
    {121, 1982974962191440380ull},
    {122, 18254767120939628913ull},
    {123, 14893278649659361426ull},
    {124, 8927099900951650360ull},
    {126, 10334027811173457435ull},
    {127, 3159745524791484470ull},
    {128, 6603106236643821153ull},
    {130, 4816202201073365419ull},
    {131, 4816202201073365419ull},
    {132, 4816202201073365419ull},
    {133, 4816202201073365419ull},
    {135, 4791894686543775221ull},
    {136, 3961958370523927087ull},
    {140, 18195876542649170121ull},
    {142, 72631783794956031ull},
    {143, 3977768832574926095ull},
    {144, 4414420444326070208ull},
    {146, 13795526650101503731ull},
    {147, 11031604412794528715ull},
    {148, 11139534942999560688ull},
    {149, 5825681437907344792ull},
    {150, 8763700859862934168ull},
    {151, 11132630192715811650ull},
    {152, 816455460641018501ull},
    {153, 15270860835853640192ull},
    {154, 1546354327620110293ull},
    {155, 4695324776549716864ull},
    {156, 6346852931458676495ull},
    {157, 5230261472608991529ull},
    {158, 16645389099261167022ull},
    {159, 16151826123073778738ull},
    {160, 28293711356471688ull},
    {161, 16151826123073778738ull},
    {162, 6773732642368564158ull},
    {163, 16355688661843544526ull},
    {164, 6998087864294379613ull},
    {165, 13282462228001022799ull},
    {167, 13075460782470218565ull},
    {168, 17396884670925115809ull},
    {169, 2930142988492762814ull},
    {170, 2109754043398652842ull},
    {174, 17305609080884061470ull},
    {175, 235842778747612680ull},
    {176, 6564629261409662377ull},
    {178, 871800059200800987ull},
    {179, 15075917697486370393ull},
    {182, 9303142023206089983ull},
    {183, 7392861702259038279ull},
    {184, 11128118633571785412ull},
    {186, 13890260386994271883ull},
    {187, 7773204180526074864ull},
    {188, 9817992694422752489ull},
    {189, 13934636668609342263ull},
    {190, 13189659541222952533ull},
    {193, 5335982172013659759ull},
    {194, 15913093305535402467ull},
    {195, 739032799000195145ull},
    {197, 18040221321673370808ull},
    {198, 11438456890535868887ull},
    {199, 12596913273590333529ull},
    {200, 10386416872941955068ull},
    {201, 15484877499534936984ull},
    {202, 16612302789085216244ull},
    {203, 7744059127945101238ull},
    {205, 1040734355826792677ull},
    {206, 6256291871835531479ull},
    {208, 5692618240829850282ull},
    {209, 5618994565493409492ull},
    {211, 17399940233443191635ull},
    {214, 17537253043673507279ull},
    {215, 15131504976672207271ull},
    {217, 5035524815660143289ull},
    {218, 14540939221093811187ull},
    {219, 3897113761151844417ull},
    {221, 810834073564742108ull},
    {222, 10938843732996440403ull},
    {237, 9236794479199753463ull},
    {239, 8707347335571919900ull},
    {248, 14996187180944217906ull},
    {249, 2239993916743206869ull},
    {250, 3168105005263113165ull},
    {251, 16161007606787088142ull},
    {252, 7080679073552575380ull},
    {254, 6191836559123737457ull},
    {255, 12559317535332705923ull},
    {267, 16137730420776985062ull},
    {268, 13595223802245428581ull},
    {271, 10543262647682533362ull},
    {272, 1887782844680680727ull},
    {274, 15764754574011414200ull},
    {276, 17347799097355877475ull},
    {278, 728949602536188638ull},
    {279, 4189383835178211373ull},
    {281, 5627191400625121895ull},
    {282, 13679263975109995991ull},
    {283, 13803088293509542304ull},
    {296, 17747839143158954679ull},
    {300, 5286508738600918491ull},
    {301, 17743137360367020774ull},
    {302, 12472522916275584237ull},
};

std::vector<uint8_t> readFile(const std::string& path) {
    if(!std::filesystem::exists(path))
        throw std::runtime_error("File not found");

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

static bool isDigit(char c) {
    return c >= '0' && c <= '9';
}

static int readInt(const std::string& str) {
    if(!isDigit(str[0]))
        return -1;
    int res = 0;
    for (auto &c : str) {
        if(isDigit(c)) {
            res *= 10;
            res += c - '0';
        } else {
            break;
        }
    }
    return res;
}

void GameData::load_folder(const std::string& path) {
    bufferFromExe(); // reload original assets

    std::unordered_map<int, std::string> files;
    for(auto& item : std::filesystem::directory_iterator(path)) {
        if(!item.is_regular_file()) continue;

        int id = readInt(item.path().filename().string());
        if(id == -1) continue;
        files[id] = item.path().string();
    }

    auto getAsset = [&](int id) -> std::optional<std::vector<uint8_t>> {
        if(!files.contains(id)) return std::nullopt;

        std::ifstream testFile(files[id], std::ios::binary);
        testFile.exceptions(std::ifstream::badbit | std::ifstream::failbit);
        return std::vector<uint8_t>(std::istreambuf_iterator(testFile), std::istreambuf_iterator<char>());
    };

    for(size_t i = 0; i < 5; i++) {
        if(auto data = getAsset(mapIds[i])) {
            auto map = Map(*data);
            if(map.coordinate_map != maps[i].coordinate_map) {
                error_dialog.warning("Map structure differs from previously loaded map.\nMight break things so be careful.");
            }
            maps[i] = map;
        }
    }

    for(const auto [_, asset_id, tile_id] : spriteMapping) {
        if(auto data = getAsset(asset_id)) sprites[tile_id] = SpriteData(*data);
    }

    if(auto uvs_ = getAsset(254)) uvs = uv_data::load(*uvs_);
    if(auto ambient_ = getAsset(179)) ambient = LightingData::parse(*ambient_);
    if(auto atlas_ = getAsset(255)) atlas = Image(*atlas_);
    if(auto bunny_ = getAsset(30)) bunny = Image(*bunny_);
    if(auto time_capsule_ = getAsset(277)) time_capsule = Image(*time_capsule_);

    for(size_t i = 11; i <= 24; i++) {
        if(auto bg = getAsset(i)) backgrounds[i - 11] = Image(*bg);
    }
    // index 25 is skipped for some reason
    if(auto bg = getAsset(26)) backgrounds[14] = Image(*bg);
}

void GameData::save_folder(const std::string& path) const {
    auto p = std::filesystem::path(path);
    std::filesystem::create_directories(p);
    backup_assets(path);

    for(size_t i = 0; i < 5; i++) {
        writeFileIfChanged(p / std::format("{}.map", mapIds[i]), maps[i].save(), mapIds[i]);
    }
    for(const auto [_, asset_id, tile_id] : spriteMapping) {
        writeFileIfChanged(p / std::format("{}.sprite", asset_id), sprites.at(tile_id).save(), asset_id);
    }

    writeFileIfChanged(p / "254.tiles", uv_data::save(uvs), 254);
    writeFileIfChanged(p / "179.ambient", LightingData::save(ambient), 179);

    writeFileIfChanged(p / "255.png", atlas.save_png(), 255);
}

void GameData::backup_assets(const std::string& path) const {
    auto p = std::filesystem::path(path);

    const auto now = std::chrono::current_zone()->to_local(std::chrono::system_clock::now());
    auto bp = p / "backups" / std::format("{:%Y-%m-%d %H-%M}", now);

    // backup with same name already exists. keep old backup 
    if(std::filesystem::exists(bp)) return;
    std::filesystem::create_directories(bp);

    for(auto& item : std::filesystem::directory_iterator(path)) {
        if(!item.is_regular_file()) continue;

        int id = readInt(item.path().filename().string());
        if(id == -1) continue;

        std::filesystem::copy_file(item, bp / item.path().filename());
    }
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
