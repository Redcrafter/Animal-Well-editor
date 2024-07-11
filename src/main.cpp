#include <algorithm>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <random>
#include <set>
#include <span>

#include "glStuff.hpp" // has to be included before glfw
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <nfd.h>

#include "game_data.hpp"
#include "history.hpp"
#include "map_slice.hpp"
#include "rendering.hpp"

#include "windows/errors.hpp"
#include "windows/search.hpp"
#include "windows/sprite_viewer.hpp"
#include "windows/tile_list.hpp"
#include "windows/tile_viewer.hpp"

// TODO:
// reduce global state
// add tile names/descriptions
// option to display hex numbers instead of decimal for data values (i.e. tile coords, tile id, room coords)
// fix some special tile rendering (clock, bat, dog, space bunny, time capsule)

GLFWwindow* window;

float gScale = 1;
// Camera matrix
glm::mat4 view = glm::lookAt(
    glm::vec3(0, 0, 3), // Camera is at (0, 0, 3), in World Space
    glm::vec3(0, 0, 0), // and looks at the origin
    glm::vec3(0, 1, 0)  // Head is up
);
glm::mat4 projection;
glm::mat4 MVP;

glm::vec2 mousePos = glm::vec2(-1);
glm::vec2 lastMousePos = glm::vec2(-1);

glm::vec2 screenSize;

std::unique_ptr<Mesh> fg_tiles, bg_tiles, bg_text, overlay;

int selectedMap = 0;

GameData game_data;

std::unique_ptr<Texture> atlas;
std::unique_ptr<Texture> bg_tex;

glm::vec4 bg_color {0.8, 0.8, 0.8, 1};
glm::vec4 fg_color {1, 1, 1, 1};
glm::vec4 bg_tex_color {0.5, 0.5, 0.5, 1};

bool show_fg = true;
bool show_bg = true;
bool show_bg_tex = true;
bool room_grid = false;
bool show_water = false;
bool accurate_vines = true;

constexpr auto room_size = glm::ivec2(40, 22);
constexpr const char* modes[] = {"View", "Edit"};
int mouse_mode = 0;
glm::ivec2 mode0_selection = {-1, -1};

// 0 = forground, 1 = background
bool selectLayer = false;

// stores copied map tiles
MapSlice clipboard;

// copying the entire map takes ~1MB so 1000 entries is totally fine
constexpr int max_undo_size = 1000;
// needs insertion/deletion at both sides due to overflow protection
std::deque<std::unique_ptr<HistoryItem>> undo_buffer;
std::vector<std::unique_ptr<HistoryItem>> redo_buffer;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    mousePos = {xpos, ypos};
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    ImGuiIO& io = ImGui::GetIO();
    if(io.WantCaptureMouse) {
        return;
    }

    float scale = 1;
    if(yoffset > 0) {
        scale = 1.1f;
    } else if(yoffset < 0) {
        scale = 1 / 1.1f;
    }

    auto mp = glm::vec4(((mousePos - screenSize / 2.0f) / screenSize) * 2.0f, 0, 1);
    mp.y = -mp.y;
    auto wp = glm::vec2(glm::inverse(projection * view) * mp);

    gScale *= scale;
    view = glm::translate(view, glm::vec3(wp, 0));
    view = glm::scale(view, glm::vec3(scale, scale, 1));
    view = glm::translate(view, glm::vec3(-wp, 0));
}

static void onResize(GLFWwindow* window, int width, int height) {
    screenSize = glm::vec2(width, height);
    glViewport(0, 0, width, height);
    projection = glm::ortho<float>(0, width, height, 0, 0.0f, 100.0f);
}

static void updateRender() {
    renderMap(game_data.maps[selectedMap], game_data.uvs, game_data.sprites, *fg_tiles, 0, accurate_vines);
    renderMap(game_data.maps[selectedMap], game_data.uvs, game_data.sprites, *bg_tiles, 1, accurate_vines);
    renderBgs(game_data.maps[selectedMap], *bg_text);
}

static void push_undo(std::unique_ptr<HistoryItem> item) {
    undo_buffer.push_back(std::move(item));
    if(undo_buffer.size() > max_undo_size)
        undo_buffer.pop_front();
    redo_buffer.clear();
}

class {
    // stores tiles underneath the current selection data
    MapSlice selection_buffer;
    // location where current selection was originally from
    glm::ivec3 orig_pos {-1, -1, -1};
    // current selection location
    glm::ivec2 start_pos = {-1, -1};
    glm::ivec2 _size = {0, 0};

  public:
    void drag_begin(glm::ivec2 pos) {
        release();
        start_pos = pos;
    }
    void drag_end(glm::ivec2 pos) {
        assert(start_pos != glm::ivec2(-1, -1));
        _size = glm::abs(pos - start_pos) + 1;
        start_pos = glm::min(start_pos, pos); // take min so start is always in the top left

        orig_pos = {start_pos, selectLayer};

        selection_buffer.fill({}, _size);
    }
    // sets area and copies underlying data
    void start_from_paste(glm::ivec2 start, glm::ivec2 size) {
        release();
        _size = size;
        start_pos = start; // take min so start is always in the top left

        orig_pos = {start_pos, selectLayer};

        selection_buffer.copy(game_data.maps[selectedMap], orig_pos, _size);

        push_undo(std::make_unique<AreaChange>(orig_pos, selection_buffer));
    }
    // apply changes without deselecting
    void apply() {
        if(start_pos != glm::ivec2(-1) && orig_pos != glm::ivec3(start_pos, selectLayer)) {
            push_undo(std::make_unique<AreaMove>(glm::ivec3(start_pos, selectLayer), orig_pos, selection_buffer));
        }
        orig_pos = glm::ivec3(start_pos, selectLayer);
    }
    // apply changes and deselect
    void release() {
        apply();
        orig_pos = {-1, -1, -1};
        start_pos = {-1, -1};
        _size = {0, 0};
    }
    // move selection to different layer
    void change_layer(int from, int to) {
        selection_buffer.swap(game_data.maps[selectedMap], glm::ivec3(start_pos, from)); // put original data back
        selection_buffer.swap(game_data.maps[selectedMap], glm::ivec3(start_pos, to));   // store underlying
        updateRender();
    }

    void move(glm::ivec2 delta) {
        if(delta == glm::ivec2(0, 0)) return;
        selection_buffer.swap(game_data.maps[selectedMap], glm::ivec3(start_pos, selectLayer)); // put original data back

        // move to new pos
        start_pos += delta;

        selection_buffer.swap(game_data.maps[selectedMap], glm::ivec3(start_pos, selectLayer)); // store underlying

        updateRender();
    }

    bool selecting() const {
        return start_pos != glm::ivec2(-1, -1) && _size == glm::ivec2(0);
    }
    bool holding() const {
        return start_pos != glm::ivec2(-1, -1) && _size != glm::ivec2(0);
    }
    bool contains(glm::ivec2 pos) const {
        return pos.x >= start_pos.x && pos.y >= start_pos.y &&
               pos.x < (start_pos.x + _size.x) && pos.y < (start_pos.y + _size.y);
    }
    glm::ivec3 start() const { return glm::ivec3(start_pos, selectLayer); }
    glm::ivec2 size() const { return _size; }
} selection_handler;

static void undo() {
    if(undo_buffer.empty()) return;
    selection_handler.release();

    auto el = std::move(undo_buffer.back());
    undo_buffer.pop_back();

    auto area = el->apply(game_data.maps[selectedMap]);
    updateRender();

    // select region that has been undone. only trigger change if actually moved
    selectLayer = area.first.z;
    selection_handler.drag_begin(area.first);
    selection_handler.drag_end(glm::ivec2(area.first) + area.second - 1);

    redo_buffer.push_back(std::move(el));
}
static void redo() {
    if(redo_buffer.empty()) return;
    selection_handler.release();

    auto el = std::move(redo_buffer.back());
    redo_buffer.pop_back();

    auto area = el->apply(game_data.maps[selectedMap]);
    updateRender();

    // selct region that has been redone. only trigger change if actually moved
    selectLayer = area.first.z;
    selection_handler.drag_begin(area.first);
    selection_handler.drag_end(glm::ivec2(area.first) + area.second - 1);

    undo_buffer.push_back(std::move(el));
}

static ImVec2 toImVec(const glm::vec2 vec) {
    return ImVec2(vec.x, vec.y);
}

static bool load_game(const std::string& path) {
    if(!std::filesystem::exists(path)) {
        return false;
    }

    try {
        game_data = GameData::load(path);

        { // load atlas
            if(atlas == nullptr) {
                atlas = std::make_unique<Texture>();
            }

            auto tex = Image(game_data.get_asset(255));
            // chroma key cyan and replace with alpha
            auto vptr = tex.data();
            for(int i = 0; i < tex.width() * tex.height(); ++i) {
                if(vptr[i] == 0xFFFFFF00) {
                    vptr[i] = 0;
                }
            }
            atlas->Load(tex);
        }

        if(bg_tex == nullptr) {
            bg_tex = std::make_unique<Texture>();
        }

        bg_tex->Bind();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 320 * 4, 180 * 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        bg_tex->width = 320 * 4;
        bg_tex->height = 180 * 4;

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        bg_tex->LoadSubImage(320 * 0, 180 * 0, game_data.get_asset(11)); // 13
        bg_tex->LoadSubImage(320 * 1, 180 * 0, game_data.get_asset(12)); // 14
        bg_tex->LoadSubImage(320 * 2, 180 * 0, game_data.get_asset(13)); // 7, 8
        bg_tex->LoadSubImage(320 * 3, 180 * 0, game_data.get_asset(14)); // 1

        bg_tex->LoadSubImage(320 * 0, 180 * 1, game_data.get_asset(15)); // 6
        bg_tex->LoadSubImage(320 * 1, 180 * 1, game_data.get_asset(16)); // 9, 11
        bg_tex->LoadSubImage(320 * 2, 180 * 1, game_data.get_asset(17)); // 10
        bg_tex->LoadSubImage(320 * 3, 180 * 1, game_data.get_asset(18)); // 16

        bg_tex->LoadSubImage(320 * 0, 180 * 2, game_data.get_asset(19)); // 4, 5
        bg_tex->LoadSubImage(320 * 1, 180 * 2, game_data.get_asset(20)); // 15
        bg_tex->LoadSubImage(320 * 2, 180 * 2, game_data.get_asset(21)); // 19
        bg_tex->LoadSubImage(320 * 3, 180 * 2, game_data.get_asset(22)); // 2, 3

        bg_tex->LoadSubImage(320 * 0, 180 * 3, game_data.get_asset(23)); // 17
        bg_tex->LoadSubImage(320 * 1, 180 * 3, game_data.get_asset(24)); // 18
        bg_tex->LoadSubImage(320 * 2, 180 * 3, game_data.get_asset(26)); // 12

        undo_buffer.clear();
        redo_buffer.clear();
        updateRender();
        return true;
    } catch(std::exception& e) {
        error_dialog.push(e.what());
    }

    return false;
}

static void load_game_dialog() {
    static std::string lastPath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Animal Well";
    std::string path;
    auto result = NFD::OpenDialog({{"Game", {"exe"}}}, lastPath.c_str(), path, window);
    if(result == NFD::Result::Error) {
        error_dialog.push(NFD::GetError());
    } else if(result == NFD::Result::Okay) {
        lastPath = path;
        load_game(path);
    }
}

static void dump_assets() {
    static std::string lastPath = std::filesystem::current_path().string();
    std::string path;
    auto result = NFD::PickFolder(lastPath.c_str(), path, window);
    if(result != NFD::Result::Okay) {
        return;
    }
    lastPath = path;

    try {
        std::filesystem::create_directories(path);

        game_data.apply_changes();

        for(size_t i = 0; i < game_data.assets.size(); ++i) {
            auto& item = game_data.assets[i];

            auto dat = game_data.get_asset(i);
            auto ptr = dat.data();
            std::string ext = ".bin";
            if(item.type == AssetType::Text) {
                ext = ".txt";
            } else if(ptr[0] == 'O' && ptr[1] == 'g' && ptr[2] == 'g' && ptr[3] == 'S') {
                assert(item.type == AssetType::Ogg || item.type == AssetType::Encrypted_Ogg);
                ext = ".ogg";
            } else if(ptr[0] == 0x89 && ptr[1] == 'P' && ptr[2] == 'N' && ptr[3] == 'G') {
                assert(item.type == AssetType::Png || item.type == AssetType::Encrypted_Png);
                ext = ".png";
            } else if(ptr[0] == 0xFE && ptr[1] == 0xCA && ptr[2] == 0x0D && ptr[3] == 0xF0) {
                assert(item.type == AssetType::MapData || item.type == AssetType::Encrypted_MapData);
                ext = ".map";
            } else if(ptr[0] == 'D' && ptr[1] == 'X' && ptr[2] == 'B' && ptr[3] == 'C') {
                assert(item.type == AssetType::Shader);
                ext = ".shader";
            } else if(ptr[0] == 0 && ptr[1] == 0x0B && ptr[2] == 0xB0 && ptr[3] == 0) {
                assert(item.type == AssetType::MapData || item.type == AssetType::Encrypted_MapData);
                ext = ".uvs";
            } else if(ptr[0] == 'P' && ptr[1] == 'K' && ptr[2] == 3 && ptr[3] == 4) {
                assert(item.type == AssetType::Encrypted_XPS);
                ext = ".xps";
            } else if(ptr[0] == 0x00 && ptr[1] == 0x0B && ptr[2] == 0xF0 && ptr[3] == 0x00) {
                assert(item.type == AssetType::MapData || item.type == AssetType::Encrypted_MapData);
                ext = ".ambient";
            } else if(ptr[0] == 0x1D && ptr[1] == 0xAC) { // ptr[2] = version 1,2,3
                assert(item.type == AssetType::SpriteData);
                ext = ".sprite";
            } else if(ptr[0] == 'B' && ptr[1] == 'M' && ptr[2] == 'F') {
                assert(item.type == AssetType::Font);
                ext = ".font";
            } else {
                assert(false);
            }

            std::ofstream file(path + "/" + std::to_string(i) + ext, std::ios::binary);
            file.write((char*)dat.data(), dat.size());
            file.close();
        }
    } catch(std::exception& e) {
        error_dialog.push(e.what());
    }
}

static auto calc_tile_size(int i) {
    auto uv = game_data.uvs[i];
    auto size = uv.size;

    switch(i) {
        case 793: // time capsule
            size = {64, 32};
            break;
        case 794: // big bunny
            size = {256, 256};
            break;

        case 610: case 615: case 616: // doors
            size.x += 3;
            break;
        case 150: // sign
        case 151: // snail
        case 6: case 7: case 8: case 9: // small indicator blocks
        case 38: case 46: case 202: case 548: case 554: case 561: case 624: case 731: // lamps
        case 348:
        case 85:
        case 118: case 694: // buttons
        case 167:
            size.x *= 2;
            break;
        case 411: case 412: case 413: case 414: // big flames
        case 628: case 629: case 630: case 631: // small flames
        case 42:
        case 125: // candle
        case 426:
            size.x *= 3;
            break;
        case 224: // heart
        case 226:
        case 164:
        case 128:
            size.x *= 4;
            break;
        case 129:
        case 138: // bear
        case 225:
        case 241: // panda
        case 370:
        case 468: // maybe 5? idk
        case 217:
            size.x *= 6;
            break;
        case 245:
            size.x *= 7;
            break;
        case 278:
        case 351:
        case 343:
        case 783:
        case 218:
            size.x *= 8;
            break;
        case 244:
        case 216:
            size.x *= 10;
            break;
        case 282: // water drop
            size.x *= 13;
            break;

        case 89: case 160: case 277: case 346: case 354: // pipes
            size.y *= 4;
            break;

        case 315: // egg atlas
            size *= 8;
            break;

        case 775: // 65th egg uv. only has uv
            break;

        default:
            if(!game_data.sprites.contains(i)) { // not a sprite
                if(uv.contiguous || uv.self_contiguous) {
                    size.x *= 4;
                    size.y *= 4;
                }
                if(uv.has_normals) {
                    size.y *= 2;
                }
            }
            break;
    }

    return size;
}

static void dump_tile_textures() {
    static std::string lastPath = std::filesystem::current_path().string();
    std::string path;
    auto result = NFD::PickFolder(lastPath.c_str(), path, window);
    if(result != NFD::Result::Okay) {
        return;
    }
    lastPath = path;

    Image time_capsule(game_data.get_asset(277));
    Image bunny(game_data.get_asset(30));
    Image atlas(game_data.get_asset(255));

    for(size_t i = 0; i < game_data.uvs.size(); ++i) {
        auto uv = game_data.uvs[i];
        Image* tex;

        auto size = calc_tile_size(i);

        if(i == 793) {
            tex = &time_capsule;
        } else if(i == 794) {
            tex = &bunny;
        } else {
            if(uv.size.x == 0 || uv.size.y == 0) continue;
            tex = &atlas;
        }

        tex->slice(uv.pos.x, uv.pos.y, size.x, size.y).save_png(path + "/" + std::to_string(i) + ".png");
    }
}

static void randomize() {
    std::vector<MapTile> items;
    std::vector<glm::ivec2> locations;

    std::set<int> target;

    //  16 = save_point
    //  39 = telephone
    target.insert(40); // = key
    target.insert(41); // = match
    //  90 = egg
    target.insert(109); // = lamp
    target.insert(149); // = stamps
    // 161 = cheaters ring
    target.insert(162); // = b. wand
    target.insert(169); // = flute
    target.insert(214); // = map
    // 231 = deathless figure
    // 284 = disc // can't be randomized
    target.insert(323); // = uv light
    target.insert(334); // = yoyo
    target.insert(382); // = mock disc
    // 383 = firecracker
    target.insert(417); // = spring
    target.insert(442); // = pencil
    target.insert(466); // = remote
    target.insert(469); // = S.medal
    // 550.? = bunny
    target.insert(611); // = house key
    target.insert(617); // = office key
    // 627.0 = seahorse flame
    // 627.1 = cat flame
    // 627.1 = lizard flame
    // 627.3 = ostrich flame
    target.insert(634); // = top
    target.insert(637); // = b. ball
    target.insert(643); // = wheel
    target.insert(679); // = E.medal
    target.insert(708); // = bb. wand
    target.insert(711); // = 65th egg
    target.insert(780); // = f.pack

    auto& map = game_data.maps[0];

    for(auto& room : map.rooms) {
        for(int y2 = 0; y2 < 22; y2++) {
            for(int x2 = 0; x2 < 40; x2++) {
                auto& tile = room.tiles[0][y2][x2];

                if(target.contains(tile.tile_id)) {
                    items.push_back(tile);
                    locations.emplace_back(room.x * 40 + x2, room.y * 22 + y2);
                }
            }
        }
    }

    std::random_device rd;
    std::mt19937 g(rd());
    std::ranges::shuffle(locations, g);

    for(auto item : items) {
        auto loc = locations.back();
        locations.pop_back();

        map.setTile(0, loc.x, loc.y, item);
    }

    updateRender();
}

class {
    std::string export_path = std::filesystem::current_path().string() + "/Animal Well.exe";
    char save_name[15] = "AnimalWell.sav";
    bool has_exported = false;
    bool patch_renderdoc = false;

    bool should_open = false;

  public:
    void draw_options() {
        if(has_exported) {
            const auto fileName = std::filesystem::path(export_path).filename().string();
            auto name = "Export to " + fileName;
            if(ImGui::MenuItem(name.c_str(), "Ctrl+S")) {
                export_exe();
            }
        } else {
            if(ImGui::MenuItem("Export...", "Ctrl+S")) {
                export_explicit();
            }
        }

        if(ImGui::MenuItem("Export As...", "Ctrl+Shift+S")) {
            export_explicit();
        }
    }

    void draw_popup() {
        if(should_open) {
            ImGui::OpenPopup("Export options");
            should_open = false;
        }

        if(ImGui::BeginPopupModal("Export options")) {
            ImGui::InputText("path", &export_path);
            ImGui::Checkbox("patch renderdoc", &patch_renderdoc);

            ImGui::InputText("save name", save_name, sizeof(save_name));

            if(ImGui::Button("Export")) {
                export_exe();
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel")) {
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }
    }

    void export_explicit() {
        std::string path;
        auto result = NFD::SaveDialog({{"Game", {"exe"}}}, export_path.c_str(), path, window);

        if(result == NFD::Result::Error) {
            error_dialog.push(NFD::GetError());
            return;
        }
        if(result == NFD::Result::Cancel) {
            return;
        }

        export_path = path;

        // options_popup();
        should_open = true;
    }
    void export_implicit() {
        if(has_exported) {
            export_exe();
        } else {
            export_explicit();
        }
    }

  private:
    void export_exe() {
        try {
            game_data.apply_changes();

            // copying invalidates asset span
            auto out = game_data; // make copy for exporting

            out.patch_renderdoc(patch_renderdoc);
            out.patch_save_path(save_name);
            out.save(export_path);
        } catch(std::exception& e) {
            error_dialog.push(e.what());
        }
    }
} exe_exporter;

class {
    std::string export_path = std::filesystem::current_path().string() + "/map.map";
    bool has_exported = false;

  public:
    void draw_options() {
        if(has_exported) {
            const auto fileName = std::filesystem::path(export_path).filename().string();
            auto name = "Export to " + fileName;
            if(ImGui::MenuItem(name.c_str(), "Ctrl+E")) {
                export_map();
            }
        } else {
            if(ImGui::MenuItem("Export...", "Ctrl+E")) {
                export_explicit();
            }
        }

        if(ImGui::MenuItem("Export As...", "Ctrl+Shift+E")) {
            export_explicit();
        }
    }

    void export_explicit() {
        std::string path;
        auto result = NFD::SaveDialog({{"Map", {"map"}}}, export_path.c_str(), path, window);

        if(result == NFD::Result::Error) {
            error_dialog.push(NFD::GetError());
            return;
        }
        if(result == NFD::Result::Cancel) {
            return;
        }

        export_path = path;
        export_map();
    }
    void export_implicit() {
        if(has_exported) {
            export_map();
        } else {
            export_explicit();
        }
    }

  private:
    void export_map() {
        try {
            auto data = game_data.maps[selectedMap].save();

            std::ofstream file(export_path, std::ios::binary);
            file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
            file.write((char*)data.data(), data.size());

            has_exported = true;
        } catch(std::exception& e) {
            error_dialog.push(e.what());
        }
    }
} map_exporter;

void full_map_screenshot(ShaderProgram& textured_shader) {
    static std::string export_path = std::filesystem::current_path().string() + "/map.png";
    std::string path;
    auto result = NFD::SaveDialog({{"png", {"png"}}}, export_path.c_str(), path, window);

    if(result == NFD::Result::Error) {
        error_dialog.push(NFD::GetError());
        return;
    }
    if(result == NFD::Result::Cancel) {
        return;
    }
    export_path = path;

    auto& map = game_data.maps[selectedMap];
    auto size = glm::ivec2(map.size.x, map.size.y) * room_size * 8;

    glViewport(0, 0, size.x, size.y);

    Textured_Framebuffer fb(size.x, size.y);
    fb.Bind();

    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    glm::mat4 MVP = glm::ortho<float>(0, size.x, 0, size.y, 0.0f, 100.0f) *
                    glm::lookAt(
                        glm::vec3(map.offset * room_size * 8, 3),
                        glm::vec3(map.offset * room_size * 8, 0),
                        glm::vec3(0, 1, 0));

    textured_shader.Use();
    textured_shader.setMat4("MVP", MVP);

    if(show_bg_tex) { // draw background textures
        textured_shader.setVec4("color", bg_tex_color);
        bg_tex->Bind();
        bg_text->Draw();
    }

    atlas->Bind();
    if(show_bg) { // draw background tiles
        textured_shader.setVec4("color", bg_color);
        bg_tiles->Draw();
    }
    if(show_fg) { // draw foreground tiles
        textured_shader.setVec4("color", fg_color);
        fg_tiles->Draw();
    }

    // draw overlay (selection, water level)
    // flat_shader.Use();
    // flat_shader.setMat4("MVP", MVP);
    // overlay->Buffer();
    // overlay->Draw();

    Image img(size.x, size.y);
    fb.tex.Bind();
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
    img.save_png(path);

    // restore viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
}

// Tips: Use with ImGuiDockNodeFlags_PassthruCentralNode!
// The limitation with this call is that your window won't have a menu bar.
// Even though we could pass window flags, it would also require the user to be able to call BeginMenuBar() somehow meaning we can't Begin/End in a single function.
// But you can also use BeginMainMenuBar(). If you really want a menu bar inside the same window as the one hosting the dockspace, you will need to copy this code somewhere and tweak it.
static ImGuiID DockSpaceOverViewport(ShaderProgram& textured_shader) {
    auto viewport = ImGui::GetMainViewport();

    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

    ImGuiWindowFlags host_window_flags = 0;
    host_window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking;
    host_window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
    if(dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        host_window_flags |= ImGuiWindowFlags_NoBackground;

    char label[32];
    ImFormatString(label, IM_ARRAYSIZE(label), "DockSpaceViewport_%08X", viewport->ID);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin(label, NULL, host_window_flags);
    ImGui::PopStyleVar(3);

    if(ImGui::BeginMenuBar()) {
        if(ImGui::BeginMenu("File")) {
            if(ImGui::MenuItem("Load Game")) {
                load_game_dialog();
            }

            ImGui::BeginDisabled(!game_data.loaded);
            exe_exporter.draw_options();
            ImGui::EndDisabled();

            ImGui::Separator();

            if(ImGui::MenuItem("Load Map")) {
                static std::string lastPath = std::filesystem::current_path().string();
                std::string path;
                auto result = NFD::OpenDialog({{"Map", {"map"}}}, lastPath.c_str(), path, window);

                if(result == NFD::Result::Error) {
                    error_dialog.push(NFD::GetError());
                } else if(result == NFD::Result::Okay) {
                    lastPath = path;
                    try {
                        auto data = readFile(path.c_str());
                        auto map = Map(std::span((uint8_t*)data.data(), data.size()));

                        if(map.coordinate_map != game_data.maps[selectedMap].coordinate_map) {
                            error_dialog.push("Warning map structure differs from previously loaded map.\nMight break things so be careful.");
                        }

                        game_data.maps[selectedMap] = map;

                        undo_buffer.clear();
                        redo_buffer.clear();
                        updateRender();
                    } catch(std::exception& e) {
                        error_dialog.push(e.what());
                    }
                }
            }

            ImGui::BeginDisabled(!game_data.loaded);
            map_exporter.draw_options();
            ImGui::EndDisabled();

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Display")) {
            if(ImGui::Combo("Map", &selectedMap, mapNames, 5)) {
                undo_buffer.clear();
                redo_buffer.clear();
                updateRender();
            }

            ImGui::Checkbox("Foreground Tiles", &show_fg);
            ImGui::ColorEdit4("fg tile color", &fg_color.r);
            ImGui::Checkbox("Background Tiles", &show_bg);
            ImGui::ColorEdit4("bg tile color", &bg_color.r);
            ImGui::Checkbox("Background Texture", &show_bg_tex);
            ImGui::ColorEdit4("bg Texture color", &bg_tex_color.r);
            ImGui::Checkbox("Show Room Grid", &room_grid);
            ImGui::Checkbox("Show Water Level", &show_water);
            if(ImGui::Checkbox("Accurate vines", &accurate_vines)) {
                updateRender();
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Tools")) {
            ImGui::BeginDisabled(!game_data.loaded);

            if(ImGui::MenuItem("Randomize items")) {
                randomize();
            }
            if(ImGui::MenuItem("Make everything transparent")) {
                for(auto& uv : game_data.uvs) {
                    uv.blocks_light = false;
                }
            }
            if(ImGui::MenuItem("Export Full Map Screenshot")) {
                full_map_screenshot(textured_shader);
            }
            if(ImGui::MenuItem("Dump assets")) {
                dump_assets();
            }
            if(ImGui::MenuItem("Dump tile textures")) {
                dump_tile_textures();
            }

            ImGui::EndDisabled();

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);

    static auto first_time = true;
    if(first_time) {
        first_time = false;

        ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
        ImGui::DockBuilderAddNode(dockspace_id, dockspace_flags | ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockspace_id, viewport->Size);

        auto right = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Right, 0.3f, nullptr, &dockspace_id);

        // we now dock our windows into the docking node we made above
        ImGui::DockBuilderDockWindow("Sprite Viewer", right);
        ImGui::DockBuilderDockWindow("Tile Viewer", right);
        ImGui::DockBuilderDockWindow("Tile List", right);
        ImGui::DockBuilderDockWindow("Search", right);
        ImGui::DockBuilderDockWindow("Properties", right);
        ImGui::DockBuilderFinish(dockspace_id);
    }

    ImGui::End();

    return dockspace_id;
}

static void HelpMarker(const char* desc) {
    ImGui::TextDisabled("(?)");
    if(ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

static glm::ivec2 screen_to_world(glm::vec2 pos) {
    auto mp = glm::vec4((pos / screenSize) * 2.0f - 1.0f, 0, 1);
    mp.y = -mp.y;
    return glm::ivec2(glm::inverse(MVP) * mp) / 8;
}

// Returns true while the user holds down the key.
static bool GetKey(ImGuiKey key) {
    return ImGui::GetKeyData(key)->Down;
}
// Returns true during the frame the user starts pressing down the key.
static bool GetKeyDown(ImGuiKey key) {
    return ImGui::GetKeyData(key)->DownDuration == 0.0f;
}
// Returns true during the frame the user releases the key.
static bool GetKeyUp(ImGuiKey key) {
    auto dat = ImGui::GetKeyData(key);
    return !dat->Down && dat->DownDurationPrev != -1;
}

static glm::ivec2 arrow_key_dir() {
    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)) {
        return {0, -1};
    }
    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        return {0, 1};
    }
    if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        return {-1, 0};
    }
    if(ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        return {1, 0};
    }
    return {0, 0};
}

static void handle_input() {
    if(!game_data.loaded) return; // no map loaded

    ImGuiIO& io = ImGui::GetIO();

    const auto delta = lastMousePos - mousePos;
    auto lastWorldPos = screen_to_world(lastMousePos);
    lastMousePos = mousePos;

    if(io.WantCaptureMouse) return;

    if(mouse_mode == 0) {
        if(GetKey(ImGuiKey_MouseLeft)) {
            view = glm::translate(view, glm::vec3(-delta / gScale, 0));
        }
        if(GetKey(ImGuiKey_MouseRight)) {
            mode0_selection = screen_to_world(mousePos);
        }

        if(mode0_selection != glm::ivec2(-1, -1)) {
            mode0_selection += arrow_key_dir();
        }

        if(GetKey(ImGuiKey_Escape)) {
            mode0_selection = glm::ivec2(-1, -1);
        }
    } else if(mouse_mode == 1) {
        auto mouse_world_pos = screen_to_world(mousePos);

        const auto holding = selection_handler.holding();
        auto selecting = selection_handler.selecting();

        if(!selecting && GetKey(ImGuiKey_MouseLeft) && GetKey(ImGuiMod_Shift)) { // select start
            selection_handler.drag_begin(mouse_world_pos);
            selecting = true;
        }
        if(selecting && !GetKey(ImGuiKey_MouseLeft)) { // select end
            selection_handler.drag_end(mouse_world_pos);
        }

        if(holding) {
            if(GetKey(ImGuiKey_Escape)) {
                selection_handler.release();
            }
            if(GetKey(ImGuiMod_Ctrl)) {
                if(GetKeyDown(ImGuiKey_C)) clipboard.copy(game_data.maps[selectedMap], selection_handler.start(), selection_handler.size());
                if(GetKeyDown(ImGuiKey_X)) {
                    selection_handler.apply();
                    clipboard.cut(game_data.maps[selectedMap], selection_handler.start(), selection_handler.size());
                    push_undo(std::make_unique<AreaChange>(selection_handler.start(), clipboard));
                    updateRender();
                }
            }
            selection_handler.move(arrow_key_dir());
        }

        if(GetKey(ImGuiMod_Ctrl) && GetKeyDown(ImGuiKey_V)) {
            // put old data in copy buffer
            selection_handler.start_from_paste(mouse_world_pos, clipboard.size());

            // put copied tiles down
            clipboard.paste(game_data.maps[selectedMap], {mouse_world_pos, selectLayer});
            // selection_handler will push undo data once position is finalized
            updateRender();
        }

        if(GetKey(ImGuiMod_Ctrl) && GetKeyDown(ImGuiKey_Z)) {
            undo();
        }
        if(GetKey(ImGuiMod_Ctrl) && GetKeyDown(ImGuiKey_Y)) {
            redo();
        }

        if(holding && selection_handler.contains(lastWorldPos)) {
            // holding & inside rect
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

            if(GetKey(ImGuiKey_MouseLeft)) {
                selection_handler.move(mouse_world_pos - lastWorldPos);
            }
        } else if(!selecting && GetKey(ImGuiKey_MouseLeft)) {
            view = glm::translate(view, glm::vec3(-delta / gScale, 0));
        }
    }

    if(GetKey(ImGuiKey_ModCtrl)) {
        if(GetKey(ImGuiKey_ModShift)) {
            if(GetKeyDown(ImGuiKey_S)) exe_exporter.export_explicit();
            if(GetKeyDown(ImGuiKey_E)) map_exporter.export_explicit();
        } else {
            if(GetKeyDown(ImGuiKey_S)) exe_exporter.export_implicit();
            if(GetKeyDown(ImGuiKey_E)) map_exporter.export_implicit();
        }
    }
}

static void DrawPreviewWindow() {
    ImGui::Begin("Properties");
    auto& io = ImGui::GetIO();
    // ImGui::Text("fps %f", ImGui::GetIO().Framerate);

    if(ImGui::Combo("Mode", &mouse_mode, modes, sizeof(modes) / sizeof(char*))) {
        mode0_selection = glm::ivec2(-1, -1);
        selection_handler.release();
    }

    if(mouse_mode == 0) {
        ImGui::SameLine();
        HelpMarker("Right click to select a tile.\nEsc to unselect.");

        auto tile_location = mode0_selection;
        if(tile_location == glm::ivec2(-1)) {
            if(io.WantCaptureMouse) {
                tile_location = glm::ivec2(-1, -1);
            } else {
                tile_location = screen_to_world(mousePos);
            }
        }

        auto room_pos = tile_location / room_size;
        auto room = game_data.maps[selectedMap].getRoom(room_pos.x, room_pos.y);

        ImGui::Text("world pos %i %i", tile_location.x, tile_location.y);

        if(room != nullptr) {
            ImGui::SeparatorText("Room Data");
            ImGui::Text("position %i %i", room->x, room->y);
            ImGui::InputScalar("water level", ImGuiDataType_U8, &room->waterLevel);
            uint8_t bg_min = 0, bg_max = 18;
            if(ImGui::SliderScalar("background id", ImGuiDataType_U8, &room->bgId, &bg_min, &bg_max)) {
                renderBgs(game_data.maps[selectedMap], *bg_text);
            }

            ImGui::InputScalar("pallet_index", ImGuiDataType_U8, &room->pallet_index);
            ImGui::InputScalar("idk1", ImGuiDataType_U8, &room->idk1);
            ImGui::InputScalar("idk2", ImGuiDataType_U8, &room->idk2);
            ImGui::InputScalar("idk3", ImGuiDataType_U8, &room->idk3);

            auto tp = glm::ivec2(tile_location.x % 40, tile_location.y % 22);
            auto tile = room->tiles[0][tp.y][tp.x];
            int tile_layer = 0;
            if(!show_fg || tile.tile_id == 0) {
                tile_layer = 1;
                tile = room->tiles[1][tp.y][tp.x];
                if(!show_bg || tile.tile_id == 0) {
                    tile = {};
                    tile_layer = 2;
                }
            }

            ImGui::SeparatorText("Room Tile Data");
            ImGui::Text("position %i %i %s", tp.x, tp.y, tile_layer == 0 ? "Foreground" : tile_layer == 1 ? "Background"
                                                                                                          : "N/A");
            ImGui::Text("id %i", tile.tile_id);
            ImGui::Text("param %i", tile.param);

            if(ImGui::BeginTable("tile_flags_table", 2)) {
                int flags = tile.flags;

                ImGui::BeginDisabled(tile_layer == 2);
                // clang-format off
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::CheckboxFlags("horizontal_mirror", &flags, 1);
                ImGui::TableNextColumn(); ImGui::CheckboxFlags("vertical_mirror", &flags, 2);

                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::CheckboxFlags("rotate_90", &flags, 4);
                ImGui::TableNextColumn(); ImGui::CheckboxFlags("rotate_180", &flags, 8);
                // clang-format on
                ImGui::EndDisabled();

                if(flags != tile.flags) {
                    room->tiles[tile_layer][tp.y][tp.x].flags = flags;
                    updateRender();
                }

                ImGui::EndTable();
            }

            ImGui::SeparatorText("Tile Data");
            ImGui::BeginDisabled(tile_layer == 2);
            if(DrawUvFlags(game_data.uvs[tile.tile_id])) {
                updateRender();
            }
            ImGui::EndDisabled();

            if(game_data.sprites.contains(tile.tile_id)) {
                // todo: lazy implementation could be improved with map
                auto spriteId = std::ranges::find(spriteMapping, tile.tile_id, [](const TileMapping t) { return t.tile_id; })->internal_id;

                ImGui::NewLine();
                ImGui::Text("Sprite id %i", spriteId);
                if(ImGui::Button("Open in Sprite Viewer")) {
                    sprite_viewer.select_from_tile(tile.tile_id);
                    sprite_viewer.focus();
                }
            }
        }

    } else if(mouse_mode == 1) {
        static MapTile placing;

        auto mouse_world_pos = screen_to_world(mousePos);

        ImGui::SameLine();
        HelpMarker("Middle click to copy a tile.\n\
Right click to place a tile.\n\
Shift + Left click to select an area.\n\
Move selected area with Left click.\n\
Esc to deselect.\n\
ctrl + c to copy the selected area.\n\
ctrl + x to cut selected area.\n\
ctrl + v to paste at mouse position.\n\
ctrl + z to undo.\n\
ctrl + y to redo.");

        ImGui::Text("world pos %i %i", mouse_world_pos.x, mouse_world_pos.y);

        auto room_pos = mouse_world_pos / room_size;
        auto room = game_data.maps[selectedMap].getRoom(room_pos.x, room_pos.y);

        if(!io.WantCaptureMouse && room != nullptr) {
            if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE)) {
                auto tp = glm::ivec2(mouse_world_pos.x % room_size.x, mouse_world_pos.y % room_size.y);
                auto tile = room->tiles[0][tp.y][tp.x];

                if(show_fg && tile.tile_id != 0) {
                    placing = tile;
                    selectLayer = false;
                } else {
                    tile = room->tiles[1][tp.y][tp.x];
                    if(show_bg && tile.tile_id != 0) {
                        placing = tile;
                        selectLayer = true;
                    } else {
                        placing = {};
                    }
                }
            }
            if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)) {
                selection_handler.release();

                auto tp = glm::ivec2(mouse_world_pos.x % room_size.x, mouse_world_pos.y % room_size.y);
                auto tile_layer = room->tiles[selectLayer];
                auto tile = tile_layer[tp.y][tp.x];
                if(tile != placing) {
                    push_undo(std::make_unique<SingleChange>(glm::ivec3(mouse_world_pos, selectLayer), tile));
                    tile_layer[tp.y][tp.x] = placing;
                    updateRender();
                }
            }
        }

        ImGui::NewLine();
        if(ImGui::Checkbox("background layer", &selectLayer) && selection_handler.holding()) {
            selection_handler.change_layer(!selectLayer, selectLayer);
        }
        ImGui::NewLine();
        ImGui::InputScalar("id", ImGuiDataType_U16, &placing.tile_id);
        ImGui::InputScalar("param", ImGuiDataType_U8, &placing.param);

        if(ImGui::BeginTable("tile_flags_table", 2)) {
            int flags = placing.flags;
            // clang-format off
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Horizontal mirror", &flags, 1);
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Vertical mirror", &flags, 2);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Rotate 90", &flags, 4);
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Rotate 180", &flags, 8);
            // clang-format on

            placing.flags = flags;

            ImGui::EndTable();
        }

        auto& uv = game_data.uvs[placing.tile_id];

        auto pos = glm::vec2(uv.pos);
        auto size = glm::vec2(uv.size);
        auto atlas_size = glm::vec2(atlas->width, atlas->height);

        ImGui::Text("preview");
        ImGui::Image((ImTextureID)atlas->id.value, toImVec(size * 8.0f), toImVec(pos / atlas_size), toImVec((pos + size) / atlas_size));
    }

    ImGui::End();
}

static void draw_water_level() {
    if(!show_water) {
        return;
    }

    const Map& map = game_data.maps[selectedMap];

    for(const Room& room : map.rooms) {
        if(room.waterLevel >= 176) {
            continue;
        }

        auto bottom_left = glm::vec2 {room.x * 40 * 8, (room.y + 1) * 22 * 8};
        auto size = glm::vec2 {40 * 8, room.waterLevel - 176};

        overlay->AddRectFilled(bottom_left, bottom_left + size, {}, {}, IM_COL32(0, 0, 255, 76));
    }
}

static void draw_overlay() {
    ImGuiIO& io = ImGui::GetIO();

    if(mouse_mode == 0) {
        glm::ivec2 tile_location = mode0_selection;
        if(tile_location == glm::ivec2(-1, -1)) {
            if(io.WantCaptureMouse) {
                tile_location = glm::ivec2(-1, -1);
            } else {
                tile_location = screen_to_world(mousePos);
            }
        }

        auto room_pos = tile_location / room_size;
        auto room = game_data.maps[selectedMap].getRoom(room_pos.x, room_pos.y);

        if(room != nullptr) {
            auto tp = glm::ivec2(tile_location.x % 40, tile_location.y % 22);
            auto tile = room->tiles[0][tp.y][tp.x];

            if(!show_fg || tile.tile_id == 0) {
                tile = room->tiles[1][tp.y][tp.x];
                if(!show_bg || tile.tile_id == 0) {
                    tile = {};
                }
            }

            auto pos = glm::vec2(tile_location) * 8.0f;

            if(game_data.sprites.contains(tile.tile_id)) {
                auto sprite = game_data.sprites[tile.tile_id];
                auto bb_max = pos + glm::vec2(8, 8);

                int composition_id = 0;
                for(size_t j = 0; j < sprite.layers.size(); ++j) {
                    auto subsprite_id = sprite.compositions[composition_id * sprite.layers.size() + j];
                    if(subsprite_id >= sprite.sub_sprites.size())
                        continue;

                    const auto subsprite = sprite.sub_sprites[subsprite_id];
                    const auto layer = sprite.layers[j];

                    auto ap = pos + glm::vec2(subsprite.composite_pos);
                    if(tile.vertical_mirror) {
                        ap.y = pos.y + (sprite.size.y - (subsprite.composite_pos.y + subsprite.size.y));
                    }
                    if(tile.horizontal_mirror) {
                        ap.x = pos.x + (sprite.size.x - (subsprite.composite_pos.x + subsprite.size.x));
                    }

                    auto end = ap + glm::vec2(subsprite.size);
                    bb_max = glm::max(bb_max, end);

                    if(layer.is_normals1 || layer.is_normals2 || !layer.is_visible) continue;

                    overlay->AddRect(ap, end, IM_COL32_WHITE, 0.5f);
                }

                overlay->AddRect(pos, bb_max, IM_COL32(255, 255, 255, 204), 1);
            } else if(tile.tile_id != 0) {
                overlay->AddRect(pos, pos + glm::vec2(game_data.uvs[tile.tile_id].size), IM_COL32_WHITE, 1);
            } else {
                overlay->AddRect(pos, pos + glm::vec2(8), IM_COL32_WHITE, 1);
            }
            if(!room_grid)
                overlay->AddRect(room_pos * room_size * 8, room_pos * room_size * 8 + glm::ivec2(40, 22) * 8, IM_COL32(255, 255, 255, 127), 1);
        }
    } else if(mouse_mode == 1) {
        auto mouse_world_pos = screen_to_world(mousePos);
        auto room_pos = mouse_world_pos / room_size;
        auto room = game_data.maps[selectedMap].getRoom(room_pos.x, room_pos.y);

        if(room != nullptr) {
            if(!room_grid) overlay->AddRect(room_pos * room_size * 8, room_pos * room_size * 8 + glm::ivec2(40, 22) * 8, IM_COL32(255, 255, 255, 127), 1);
            overlay->AddRect(mouse_world_pos * 8, mouse_world_pos * 8 + 8);

            auto start = glm::ivec2(selection_handler.start());

            if(selection_handler.holding()) {
                auto end = start + selection_handler.size();
                overlay->AddRectDashed(start * 8, end * 8, IM_COL32_WHITE, 1, 4);
            } else if(selection_handler.selecting()) {
                auto end = mouse_world_pos;
                if(end.x < start.x) {
                    std::swap(start.x, end.x);
                }
                if(end.y < start.y) {
                    std::swap(start.y, end.y);
                }
                overlay->AddRectDashed(start * 8, end * 8 + 8, IM_COL32_WHITE, 1, 4);
            }
        }
    }

    if(room_grid) {
        const auto& map = game_data.maps[selectedMap];

        for(int i = 0; i <= map.size.x; i++) {
            auto x = (map.offset.x + i) * 40 * 8;
            overlay->AddLine({x, map.offset.y * 22 * 8}, {x, (map.offset.y + map.size.y) * 22 * 8}, IM_COL32(255, 255, 255, 191), 1 / gScale);
        }
        for(int i = 0; i <= map.size.y; i++) {
            auto y = (map.offset.y + i) * 22 * 8;
            overlay->AddLine({map.offset.x * 40 * 8, y}, {(map.offset.x + map.size.x) * 40 * 8, y}, IM_COL32(255, 255, 255, 191), 1 / gScale);
        }
    }
}

static bool UpdateUIScaling() {
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    assert(xscale == yscale);

    static float oldScale = 1.0;
    if(xscale == oldScale) return true;
    oldScale = xscale;

    ImGuiIO& io = ImGui::GetIO();

    // Setup Dear ImGui style
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    style.ScaleAllSizes(xscale);

    io.Fonts->Clear();

    if(xscale == 1.0f) {
        io.Fonts->AddFontDefault();
    } else {
        auto fs = cmrc::resources::get_filesystem();
        auto dat = fs.open("lib/proggyfonts/ProggyVector/ProggyVector-Regular.ttf");

        ImFontConfig config {};
        config.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void*)dat.begin(), dat.size(), std::floor(14.0f * xscale), &config);
    }

    return ImGui_ImplOpenGL3_CreateFontsTexture();
}

// Main code
int runViewer() {
#pragma region glfw/opengl setup
    glfwSetErrorCallback(glfw_error_callback);
    if(!glfwInit())
        return 1;

    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

    // Create window with graphics context
    window = glfwCreateWindow(1280, 720, "Animal Well map viewer", nullptr, nullptr);
    if(window == nullptr)
        return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    int version = gladLoadGL(glfwGetProcAddress);
    if(version == 0) {
        printf("Failed to initialize OpenGL context\n");
        return -1;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetScrollCallback(window, scroll_callback);
    // glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwSetFramebufferSizeCallback(window, onResize);
    onResize(window, 1280, 720);
#pragma endregion

#pragma region imgui setup
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking
    if(glfwGetPlatform() != GLFW_PLATFORM_WAYLAND) // do not enable viewports for wayland
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    // ImGui::StyleColorsLight();

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    ImGuiStyle& style = ImGui::GetStyle();
    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.0f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
#ifdef __EMSCRIPTEN__
    ImGui_ImplGlfw_InstallEmscriptenCanvasResizeCallback("#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

#pragma endregion

#pragma region opgenl buffer setup

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    ShaderProgram flat_shader("src/shaders/flat.vs", "src/shaders/flat.fs");
    ShaderProgram textured_shader("src/shaders/textured.vs", "src/shaders/textured.fs");

    fg_tiles = std::make_unique<Mesh>();
    bg_tiles = std::make_unique<Mesh>();
    bg_text = std::make_unique<Mesh>();
    overlay = std::make_unique<Mesh>();

#pragma endregion

    if(!load_game("C:/Program Files (x86)/Steam/steamapps/common/Animal Well/Animal Well.exe")) {
        load_game("./Animal Well.exe");
    }

    // Main loop
    while(!glfwWindowShouldClose(window)) {
        // Poll and handle events (inputs, window resize, etc.)
        // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
        // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
        // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
        // Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
        glfwPollEvents();

        UpdateUIScaling();

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        overlay->clear();

        MVP = projection * view;

        handle_input();
        DockSpaceOverViewport(textured_shader);
        error_dialog.draw();
        exe_exporter.draw_popup();

        // skip rendering if no data is loaded
        if(game_data.loaded) {
            bool should_update = false;

            search_window.draw(game_data, [](int map, const glm::ivec2 pos) {
                if(map != selectedMap) {
                    selectedMap = map;
                    updateRender();
                }
                // center of screen
                auto center = glm::vec2(glm::inverse(projection * view) * glm::vec4(0, 0, 0, 1));
                auto delta = glm::vec2(pos * 8) + glm::vec2(4, 4) - center;
                view = glm::translate(view, glm::vec3(-delta, 0));
            });
            tile_list.draw(game_data, *atlas);
            tile_viewer.draw(game_data, *atlas, should_update);
            sprite_viewer.draw(game_data, *atlas);
            DrawPreviewWindow();

            if(should_update)
                updateRender();

            draw_overlay();
            draw_water_level();
            search_window.draw_overlay(game_data, selectedMap, *overlay, gScale);

            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);

            textured_shader.Use();
            textured_shader.setMat4("MVP", MVP);

            if(show_bg_tex) { // draw background textures
                textured_shader.setVec4("color", bg_tex_color);
                bg_tex->Bind();
                bg_text->Draw();
            }

            atlas->Bind();
            if(show_bg) { // draw background tiles
                textured_shader.setVec4("color", bg_color);
                bg_tiles->Draw();
            }
            if(show_fg) { // draw foreground tiles
                textured_shader.setVec4("color", fg_color);
                fg_tiles->Draw();
            }

            // draw overlay (selection, water level)
            flat_shader.Use();
            flat_shader.setMat4("MVP", MVP);
            overlay->Buffer();
            overlay->Draw();
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific demo app we could also call glfwMakeContextCurrent(window) directly)
        if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}

int main(int, char**) {
    return runViewer();
}

#ifdef _WIN32
#include <Windows.h>
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    return main(__argc, __argv);
}
#endif
