#include <algorithm>
#include <array>
#include <codecvt>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <random>
#include <set>
#include <span>
#include <unordered_map>

#include "glStuff.hpp" // has to be included before glfw
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "structures/asset.hpp"
#include "structures/map.hpp"
#include "structures/sprite.hpp"
#include "structures/tile.hpp"

#include "dos_parser.hpp"
#include "history.hpp"
#include "map_slice.hpp"
#include "rendering.hpp"

#include "windows/errors.hpp"

#include "nfd.h"

// TODO:
// reduce global state
// move windows into separate files
// add tile names/descriptions

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

std::vector<Textured_Framebuffer*> framebuffers;
std::unique_ptr<Mesh> fg_tiles, bg_tiles, bg_text, overlay;

int selectedMap = 0;

std::vector<char> rawData;

static const char* mapNames[5] = {"Overworld", "CE temple", "Space", "Bunny temple", "Time Capsule"};
constexpr int mapIds[5] = {300, 157, 193, 52, 222};

std::array<Map, 5> maps {};
std::unordered_map<uint32_t, SpriteData> sprites;
std::vector<uv_data> uvs;

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

    for(const auto fb : framebuffers) {
        fb->resize(width, height);
    }
}

static void updateRender() {
    renderMap(maps[selectedMap], uvs, sprites, *fg_tiles, 0, accurate_vines);
    renderMap(maps[selectedMap], uvs, sprites, *bg_tiles, 1, accurate_vines);
    renderBgs(maps[selectedMap], *bg_text);
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

        selection_buffer.copy(maps[selectedMap], orig_pos, _size);

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
        selection_buffer.swap(maps[selectedMap], glm::ivec3(start_pos, from)); // put original data back
        selection_buffer.swap(maps[selectedMap], glm::ivec3(start_pos, to));   // store underlying
        updateRender();
    }

    void move(glm::ivec2 delta) {
        if(delta == glm::ivec2(0, 0)) return;
        selection_buffer.swap(maps[selectedMap], glm::ivec3(start_pos, selectLayer)); // put original data back

        // move to new pos
        start_pos += delta;

        selection_buffer.swap(maps[selectedMap], glm::ivec3(start_pos, selectLayer)); // store underlying

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

    auto area = el->apply(maps[selectedMap]);
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

    auto area = el->apply(maps[selectedMap]);
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

static void LoadAtlas(std::span<const uint8_t> data) {
    int width, height, n;
    auto* dat = stbi_load_from_memory(data.data(), data.size(), &width, &height, &n, 4);
    if(dat == nullptr) {
        throw std::runtime_error("failed to load texture atlas");
    }

    atlas = std::make_unique<Texture>();
    // chroma key cyan and replace with alpha

    auto vptr = (uint32_t*)dat;
    for(int i = 0; i < width * height; ++i) {
        if(vptr[i] == 0xFFFFFF00) {
            vptr[i] = 0;
        }
    }

    atlas->Bind();
    atlas->width = width;
    atlas->height = height;

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, dat);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(dat);
};

static bool load_game(const std::string& path) {
    if(!std::filesystem::exists(path)) {
        return false;
    }
    rawData = readFile(path.c_str());

    try {
        auto sections = getSegmentOffsets(rawData);

        assert(sections.data.size() >= sizeof(asset_entry) * 676);
        auto assets = std::span((asset_entry*)sections.data.data(), 676);

        std::vector<uint8_t> decryptBuffer;

        auto get_asset = [&](int id) {
            assert(id >= 0 && id < 676);
            auto& asset = assets[id];
            auto dat = sections.get_rdata_ptr(asset.ptr, asset.length);

            if(tryDecrypt(asset, dat, decryptBuffer)) {
                return std::span(decryptBuffer);
            }
            return dat;
        };

        LoadAtlas(get_asset(255));

        if(bg_tex == nullptr) {
            bg_tex = std::make_unique<Texture>();
        }

        bg_tex->Bind();
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 320 * 4, 180 * 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        bg_tex->LoadSubImage(320 * 0, 180 * 0, get_asset(11)); // 13
        bg_tex->LoadSubImage(320 * 1, 180 * 0, get_asset(12)); // 14
        bg_tex->LoadSubImage(320 * 2, 180 * 0, get_asset(13)); // 7, 8
        bg_tex->LoadSubImage(320 * 3, 180 * 0, get_asset(14)); // 1

        bg_tex->LoadSubImage(320 * 0, 180 * 1, get_asset(15)); // 6
        bg_tex->LoadSubImage(320 * 1, 180 * 1, get_asset(16)); // 9, 11
        bg_tex->LoadSubImage(320 * 2, 180 * 1, get_asset(17)); // 10
        bg_tex->LoadSubImage(320 * 3, 180 * 1, get_asset(18)); // 16

        bg_tex->LoadSubImage(320 * 0, 180 * 2, get_asset(19)); // 4, 5
        bg_tex->LoadSubImage(320 * 1, 180 * 2, get_asset(20)); // 15
        bg_tex->LoadSubImage(320 * 2, 180 * 2, get_asset(21)); // 19
        bg_tex->LoadSubImage(320 * 3, 180 * 2, get_asset(22)); // 2, 3

        bg_tex->LoadSubImage(320 * 0, 180 * 3, get_asset(23)); // 17
        bg_tex->LoadSubImage(320 * 1, 180 * 3, get_asset(24)); // 18
        bg_tex->LoadSubImage(320 * 2, 180 * 3, get_asset(26)); // 12

        for(size_t i = 0; i < 5; i++) {
            maps[i] = Map(get_asset(mapIds[i]));
        }

        for(auto el : spriteMapping) {
            sprites[el.tile_id] = parse_sprite(get_asset(el.asset_id));
        }

        uvs = parse_uvs(get_asset(254));

        undo_buffer.clear();
        redo_buffer.clear();
        updateRender();
        return true;
    } catch(std::exception& e) {
        ErrorDialog.push(e.what());
    }

    return false;
}

static void load_game_dialog() {
    static std::string lastPath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Animal Well";
    std::string path;
    auto result = NFD::OpenDialog({{"Game", {"exe"}}}, lastPath.c_str(), path, window);
    if(result == NFD::Result::Error) {
        ErrorDialog.push(NFD::GetError());
    } else if(result == NFD::Result::Okay) {
        lastPath = path;
        load_game(path);
    }
}

static void DrawUvFlags(uv_data& uv) {
    if(ImGui::BeginTable("uv_flags_table", 2)) {
        uint32_t flags = uv.flags;

        // clang-format off
        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides left", &flags, 1 << 0); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Hidden", &flags, 1 << 10);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides right", &flags, 1 << 1); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Blocks Light", &flags, 1 << 8);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides up", &flags, 1 << 2); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("obscures", &flags, 1 << 6);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides down", &flags, 1 << 3); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Contiguous", &flags, 1 << 7); // correct

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Not Placeable", &flags, 1 << 4);
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Self Contiguous", &flags, 1 << 9); // correct

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Additive", &flags, 1 << 5);
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Dirt", &flags, 1 << 11);

        ImGui::TableNextRow();
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("Has Normals", &flags, 1 << 12); // correct
        ImGui::TableNextColumn(); ImGui::CheckboxFlags("UV Light", &flags, 1 << 13); // correct
        // clang-format on

        if(flags != uv.flags) {
            uv.flags = flags;
            updateRender();
        }

        ImGui::EndTable();
    }
}

static void ImGui_draw_sprite(const SpriteData& sprite, int frame, glm::u16vec2 uv) {
    auto atlasSize = glm::vec2(atlas->width, atlas->height);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    auto pos = glm::vec2(p.x, p.y);

    for(size_t j = 0; j < sprite.layers.size(); ++j) {
        auto subsprite_id = sprite.compositions[frame * sprite.layers.size() + j];
        if(subsprite_id >= sprite.sub_sprites.size())
            continue;

        auto& layer = sprite.layers[j];
        if(layer.is_normals1 || layer.is_normals2 || !layer.is_visible) continue;

        auto& subsprite = sprite.sub_sprites[subsprite_id];

        auto aUv = glm::vec2(uv + subsprite.atlas_pos);
        auto size = glm::vec2(subsprite.size);
        auto ap = pos + glm::vec2(subsprite.composite_pos) * 8.0f;

        draw_list->AddImage((ImTextureID)atlas->id.value, toImVec(ap), toImVec(ap + glm::vec2(subsprite.size) * 8.0f), toImVec(aUv / atlasSize), toImVec((aUv + size) / atlasSize));
    }
}

class {
    int selected_animation = 0;
    bool playing = false;
    int frame_step = 0;
    int selected_frame = 0;
    int selected_sprite = 0;

  public:
    void draw() {
        if(!ImGui::Begin("Sprite Viewer")) {
            ImGui::End();
            return;
        }
        if(ImGui::InputInt("Id", &selected_sprite)) {
            select(selected_sprite);
        }

        auto tile_id = spriteMapping[selected_sprite].tile_id;
        auto& sprite = sprites[tile_id];
        auto& uv = uvs[tile_id];

        ImGui::Text("Composite size %i %i", sprite.size.x, sprite.size.y);
        ImGui::Text("Layer count %zu", sprite.layers.size());
        ImGui::Text("Subsprite count %zu", sprite.sub_sprites.size());

        if(!sprite.animations.empty()) {
            ImGui::NewLine();
            if(ImGui::SliderInt("animation", &selected_animation, 0, std::max(0, (int)sprite.animations.size() - 1)) && playing) {
                selected_frame = sprite.animations[selected_animation].start;
            }
            auto& anim = sprite.animations[selected_animation];
            ImGui::InputScalar("start", ImGuiDataType_U16, &anim.start);
            ImGui::InputScalar("end", ImGuiDataType_U16, &anim.end);
            ImGui::InputScalar("frame_delay", ImGuiDataType_U16, &anim.frame_delay);

            if(playing) {
                frame_step++;
                if(frame_step / 5 > anim.frame_delay) {
                    selected_frame++;
                    if(selected_frame > anim.end) {
                        selected_frame = anim.start;
                    }
                    frame_step = 0;
                }

                if(ImGui::Button("Pause")) playing = false;
            } else {
                if(ImGui::Button("Play")) {
                    playing = true;
                    selected_frame = anim.start;
                    frame_step = 0;
                }
            }
        }

        if(sprite.composition_count > 1) {
            ImGui::NewLine();
            ImGui::SliderInt("frame", &selected_frame, 0, sprite.composition_count - 1);
        }

        ImGui_draw_sprite(sprite, selected_frame, uv.pos);

        ImGui::End();
    }

    void select(int id) {
        selected_sprite = std::clamp(id, 0, 157);
        selected_animation = 0;
        selected_frame = 0;
        playing = false;
        frame_step = 0;
    }

    void select_from_tile(int tile_id) {
        // todo: lazy implementation could be improved with map
        auto spriteId = std::ranges::find(spriteMapping, tile_id, [](const TileMapping t) { return t.tile_id; })->internal_id;
        select(spriteId);
    }

    void focus() {
        ImGui::FocusWindow(ImGui::FindWindowByName("Sprite Viewer"));
    }
} SpriteViewer;

class {
  public:
    int selected_tile;

    void draw() {
        if(!ImGui::Begin("Tile Viewer")) {
            ImGui::End();
            return;
        }

        ImGui::InputInt("Id", &selected_tile);
        selected_tile = std::clamp(selected_tile, 0, (int)uvs.size() - 1);

        auto& uv = uvs[selected_tile];

        auto pos = glm::vec2(uv.pos);
        auto size = glm::vec2(uv.size);
        auto atlas_size = glm::vec2(atlas->width, atlas->height);

        ImGui::Text("preview");
        ImGui::Image((ImTextureID)atlas->id.value, toImVec(size * 8.0f), toImVec(pos / atlas_size), toImVec((pos + size) / atlas_size));

        DrawUvFlags(uv);

        ImGui::InputScalarN("UV", ImGuiDataType_U16, &uv.pos, 2);
        ImGui::InputScalarN("UV Size", ImGuiDataType_U16, &uv.size, 2);

        if(sprites.contains(selected_tile)) {
            auto spriteId = std::ranges::find(spriteMapping, selected_tile, [](const TileMapping t) { return t.tile_id; })->internal_id;

            ImGui::NewLine();
            ImGui::Text("Sprite id %i", spriteId);
            if(ImGui::Button("Open in Sprite Viewer")) {
                SpriteViewer.select(spriteId);
                SpriteViewer.focus();
            }
        }

        ImGui::End();
    }

    void focus() {
        ImGui::FocusWindow(ImGui::FindWindowByName("Tile Viewer"));
    }
} TileViewer;

class {
    int tile_id;
    std::vector<glm::ivec3> results;
    ImGuiTableFlags flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;
    bool show_on_map;

  public:
    void draw() {
        auto& map = maps[selectedMap];

        if(ImGui::Begin("Search")) {
            ImGui::InputInt("tile_id", &tile_id);

            if(ImGui::Button("Search")) {
                results.clear();

                for(auto& room : map.rooms) {
                    for(int y2 = 0; y2 < 22; y2++) {
                        for(int x2 = 0; x2 < 40; x2++) {
                            auto tile1 = room.tiles[0][y2][x2];
                            if(tile1.tile_id == tile_id) {
                                results.emplace_back(room.x * 40 + x2, room.y * 22 + y2, 0);
                            }

                            auto tile2 = room.tiles[1][y2][x2];
                            if(tile2.tile_id == tile_id) {
                                results.emplace_back(room.x * 40 + x2, room.y * 22 + y2, 1);
                            }
                        }
                    }
                }
            }
            if(ImGui::Button("Clear")) {
                results.clear();
            }
            ImGui::Checkbox("Highlight", &show_on_map);

            const float TEXT_BASE_HEIGHT = ImGui::GetTextLineHeightWithSpacing();
            // When using ScrollX or ScrollY we need to specify a size for our table container!
            // Otherwise by default the table will fit all available space, like a BeginChild() call.
            ImVec2 outer_size = ImVec2(0.0f, TEXT_BASE_HEIGHT * 8);

            ImGui::Text("%zu results", results.size());
            if(ImGui::BeginTable("search_results", 3, flags, outer_size)) {
                ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible
                ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_None);
                ImGui::TableSetupColumn("y", ImGuiTableColumnFlags_None);
                ImGui::TableSetupColumn("layer", ImGuiTableColumnFlags_None);
                ImGui::TableHeadersRow();

                // Demonstrate using clipper for large vertical lists
                ImGuiListClipper clipper;
                clipper.Begin(results.size());
                while(clipper.Step()) {
                    for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                        ImGui::TableNextRow();

                        auto el = results[row];

                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%i", el.x);
                        ImGui::TableSetColumnIndex(1);
                        ImGui::Text("%i", el.y);
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%i", el.z);
                    }
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();

        if(show_on_map) {
            for(auto result : results) {
                auto tile = map.getTile(result.z, result.x, result.y);
                if(!tile.has_value())
                    continue;

                auto uv = uvs[tile->tile_id];

                auto p = glm::vec2(result) * 8.0f;
                overlay->AddRect(p, p + glm::vec2(uv.size), IM_COL32(255, 0, 0, 204), 8 / gScale);
            }
        }
    }

} SearchWindow;

class {
    std::vector<std::tuple<int, glm::vec2, glm::vec2>> tiles_;

  public:
    void draw() {
        if(!ImGui::Begin("Tile List")) {
            ImGui::End();
            return;
        }

        if(tiles_.empty()) {
            auto atlas_size = glm::vec2(atlas->width, atlas->height);

            for(size_t i = 0; i < uvs.size(); ++i) {
                auto uv = uvs[i];
                if(uv.size == glm::u16vec2(0)) {
                    continue;
                }
                tiles_.emplace_back(i, glm::vec2(uv.pos) / atlas_size, glm::vec2(uv.size) / atlas_size);
            }

            std::ranges::stable_sort(tiles_, {}, [](const auto el) { return std::get<2>(el).x * std::get<2>(el).y; });
        }

        auto width = ImGui::GetCurrentWindow()->WorkRect.GetWidth();
        auto pad = ImGui::GetStyle().FramePadding.x;

        for(const auto& [id, pos, size] : tiles_) {
            ImGui::PushID(id);
            if(ImGui::ImageButton((ImTextureID)atlas->id.value, ImVec2(32, 32), toImVec(pos), toImVec(pos + size))) {
                TileViewer.selected_tile = id;
                TileViewer.focus();
            }
            ImGui::SetItemTooltip("%i", id);
            ImGui::PopID();
            ImGui::SameLine();

            auto x = ImGui::GetCursorPosX();
            if(x + 32 + pad >= width) {
                ImGui::NewLine();
            }
        }

        ImGui::End();
    }
} TileList;

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

        auto sections = getSegmentOffsets(rawData);
        auto assets = std::span((asset_entry*)sections.data.data(), 676);

        std::vector<uint8_t> decrypted;

        for(size_t i = 0; i < assets.size(); ++i) {
            auto& item = assets[i];

            std::string ext = ".bin";
            switch(item.type) {
                case AssetType::Text:
                    ext = ".txt";
                    break;
                case AssetType::MapData:
                case AssetType::Encrypted_MapData:
                    ext = ".map";
                    break;
                case AssetType::Png:
                case AssetType::Encrypted_Png:
                    ext = ".png";
                    break;
                case AssetType::Ogg:
                case AssetType::Encrypted_Ogg:
                    ext = ".ogg";
                    break;
                case AssetType::SpriteData:
                    ext = ".sprite";
                    break;
                case AssetType::Shader:
                    ext = ".shader";
                    break;
                case AssetType::Font:
                    ext = ".font";
                    break;
                case AssetType::Encrypted_XPS:
                    ext = ".xps";
                    break;
            }

            auto dat = sections.get_rdata_ptr(item.ptr, item.length);

            std::ofstream file(path + "/" + std::to_string(i) + ext, std::ios::binary);
            if(tryDecrypt(item, dat, decrypted)) {
                file.write((char*)decrypted.data(), decrypted.size());
            } else {
                file.write((char*)dat.data(), dat.size());
            }
            file.close();
        }
    } catch(std::exception& e) {
        ErrorDialog.push(e.what());
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

    auto& map = maps[0];

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
            ErrorDialog.push(NFD::GetError());
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
            auto out = rawData;
            auto sections = getSegmentOffsets(out);
            auto assets = std::span((asset_entry*)sections.data.data(), 676);

            bool error = false;

            auto replaceAsset = [&](const std::vector<uint8_t>& data, int id) {
                auto& asset = assets[id];
                auto ptr = sections.get_rdata_ptr(asset.ptr, asset.length);

                if(((uint8_t)asset.type & 192) == 64) {
                    int key = 0; // 193, 212, 255, 300
                    if(id == 30 || id == 52) key = 1;
                    else if(id == 222 || id == 277 || id == 377) key = 2;

                    auto enc = encrypt(data, key);

                    if(enc.size() > asset.length) {
                        error = true;
                        ErrorDialog.pushf("Failed to save asset %i because it was too big", id);
                    } else {
                         // asset.length = enc.size(); // keep default value cause game doesn't really use length anyway
                        std::memcpy(ptr.data(), enc.data(), enc.size());
                    }
                } else {
                    if(data.size() > asset.length) {
                        error = true;
                        ErrorDialog.pushf("Failed to save asset %i because it was too big", id);
                    } else {
                        std::memcpy(ptr.data(), data.data(), data.size());
                    }
                }
            };

            for(size_t i = 0; i < 5; i++) {
                replaceAsset(maps[i].save(), mapIds[i]);
            }
            replaceAsset(save_uvs(uvs), 254);

            /*if(patch_steam) {
                // patch steam restart
                out[0xEFE6] = 0x48; // MOV AL, 0
                out[0xEFE7] = 0xc6;
                out[0xEFE8] = 0xc0;
                out[0xEFE9] = 0x00;

                out[0xEFEA] = 0x48; // NOP
                out[0xEFEB] = 0x90;
            }*/

            if(patch_renderdoc) {
                constexpr char pattern[] = "renderdoc.dll";
                auto res = std::search(out.begin(), out.end(), std::begin(pattern), std::end(pattern));
                if(res != out.end()) {
                    *res = 'l'; // replace first letter with 'l'
                }
            }

            {
                const char* default_save = (const char*)(L"AnimalWell.sav");
                auto res = std::search(out.begin(), out.end(), default_save, default_save + 30);

                if(res != out.end()) {
                    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
                    auto wstr = converter.from_bytes(save_name);

                    for(int i = 0; i < wstr.size() + 1; ++i) {
                        auto c = wstr[i];
                        *res = c & 0xFF;
                        ++res;
                        *res = c >> 8;
                        ++res;
                    }
                }
            }

            if(!error) {
                std::ofstream file(export_path, std::ios::binary);
                file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
                file.write(out.data(), out.size());

                has_exported = true;
            }
        } catch(std::exception& e) {
            ErrorDialog.push(e.what());
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
            ErrorDialog.push(NFD::GetError());
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
            auto data = maps[selectedMap].save();
            std::ofstream file(export_path, std::ios::binary);
            file.exceptions(std::ofstream::failbit | std::ofstream::badbit);
            file.write((char*)data.data(), data.size());

            has_exported = true;
        } catch(std::exception& e) {
            ErrorDialog.push(e.what());
        }
    }
} map_exporter;

// Tips: Use with ImGuiDockNodeFlags_PassthruCentralNode!
// The limitation with this call is that your window won't have a menu bar.
// Even though we could pass window flags, it would also require the user to be able to call BeginMenuBar() somehow meaning we can't Begin/End in a single function.
// But you can also use BeginMainMenuBar(). If you really want a menu bar inside the same window as the one hosting the dockspace, you will need to copy this code somewhere and tweak it.
static ImGuiID DockSpaceOverViewport() {
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

            ImGui::BeginDisabled(rawData.empty());
            exe_exporter.draw_options();
            ImGui::EndDisabled();

            ImGui::Separator();

            if(ImGui::MenuItem("Load Map")) {
                static std::string lastPath = std::filesystem::current_path().string();
                std::string path;
                auto result = NFD::OpenDialog({{"Map", {"map"}}}, lastPath.c_str(), path, window);

                if(result == NFD::Result::Error) {
                    ErrorDialog.push(NFD::GetError());
                } else if(result == NFD::Result::Okay) {
                    lastPath = path;
                    try {
                        auto data = readFile(path.c_str());
                        auto map = Map(std::span((uint8_t*)data.data(), data.size()));

                        if(map.coordinate_map != maps[selectedMap].coordinate_map) {
                            ErrorDialog.push("Warning map structure differs from previously loaded map.\nMight break things so be careful.");
                        }

                        maps[selectedMap] = map;

                        undo_buffer.clear();
                        redo_buffer.clear();
                        updateRender();
                    } catch(std::exception& e) {
                        ErrorDialog.push(e.what());
                    }
                }
            }

            ImGui::BeginDisabled(rawData.empty());
            map_exporter.draw_options();
            ImGui::EndDisabled();

            ImGui::Separator();

            ImGui::BeginDisabled(rawData.empty());
            if(ImGui::MenuItem("Dump assets")) {
                dump_assets();
            }
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
            if(ImGui::MenuItem("Randomize items")) {
                randomize();
            }
            if(ImGui::MenuItem("Make everything transparent")) {
                for(auto& uv : uvs) {
                    uv.blocks_light = false;
                }
            }

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
        return glm::ivec2(0, -1);
    }
    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) {
        return glm::ivec2(0, 1);
    }
    if(ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) {
        return glm::ivec2(-1, 0);
    }
    if(ImGui::IsKeyPressed(ImGuiKey_RightArrow)) {
        return glm::ivec2(1, 0);
    }
    return glm::ivec2(0);
}

static void handle_input() {
    if(rawData.empty()) return; // no map loaded

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
                if(GetKeyDown(ImGuiKey_C)) clipboard.copy(maps[selectedMap], selection_handler.start(), selection_handler.size());
                if(GetKeyDown(ImGuiKey_X)) {
                    selection_handler.apply();
                    clipboard.cut(maps[selectedMap], selection_handler.start(), selection_handler.size());
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
            clipboard.paste(maps[selectedMap], {mouse_world_pos, selectLayer});
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
        auto room = maps[selectedMap].getRoom(room_pos.x, room_pos.y);

        ImGui::Text("world pos %i %i", tile_location.x, tile_location.y);

        if(room != nullptr) {
            ImGui::SeparatorText("Room Data");
            ImGui::Text("position %i %i", room->x, room->y);
            ImGui::InputScalar("water level", ImGuiDataType_U8, &room->waterLevel);
            uint8_t bg_min = 0, bg_max = 18;
            if(ImGui::SliderScalar("background id", ImGuiDataType_U8, &room->bgId, &bg_min, &bg_max)) {
                renderBgs(maps[selectedMap], *bg_text);
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
            DrawUvFlags(uvs[tile.tile_id]);
            ImGui::EndDisabled();

            if(sprites.contains(tile.tile_id)) {
                // todo: lazy implementation could be improved with map
                auto spriteId = std::ranges::find(spriteMapping, tile.tile_id, [](const TileMapping t) { return t.tile_id; })->internal_id;

                ImGui::NewLine();
                ImGui::Text("Sprite id %i", spriteId);
                if(ImGui::Button("Open in Sprite Viewer")) {
                    SpriteViewer.select_from_tile(tile.tile_id);
                    SpriteViewer.focus();
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
        auto room = maps[selectedMap].getRoom(room_pos.x, room_pos.y);

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

        auto& uv = uvs[placing.tile_id];

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

    const Map& map = maps[selectedMap];

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
        auto room = maps[selectedMap].getRoom(room_pos.x, room_pos.y);

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

            if(sprites.contains(tile.tile_id)) {
                auto sprite = sprites[tile.tile_id];
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
                overlay->AddRect(pos, pos + glm::vec2(uvs[tile.tile_id].size), IM_COL32_WHITE, 1);
            } else {
                overlay->AddRect(pos, pos + glm::vec2(8), IM_COL32_WHITE, 1);
            }
            if(!room_grid)
                overlay->AddRect(room_pos * room_size * 8, room_pos * room_size * 8 + glm::ivec2(40, 22) * 8, IM_COL32(255, 255, 255, 127), 1);
        }
    } else if(mouse_mode == 1) {
        auto mouse_world_pos = screen_to_world(mousePos);
        auto room_pos = mouse_world_pos / room_size;
        auto room = maps[selectedMap].getRoom(room_pos.x, room_pos.y);

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
        const auto& map = maps[selectedMap];

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
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable Multi-Viewport / Platform Windows
    // io.ConfigViewportsNoAutoMerge = true;
    // io.ConfigViewportsNoTaskBarIcon = true;

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
        DockSpaceOverViewport();
        ErrorDialog.draw();
        exe_exporter.draw_popup();

        // skip rendering if no data is loaded
        if(!rawData.empty()) {
            SearchWindow.draw();
            TileList.draw();
            TileViewer.draw();
            SpriteViewer.draw();
            DrawPreviewWindow();

            draw_overlay();
            draw_water_level();

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
