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
#include "globals.hpp"
#include "history.hpp"
#include "map_slice.hpp"
#include "selection.hpp"

#include "rendering/geometry.hpp"
#include "rendering/pipeline.hpp"
#include "rendering/renderData.hpp"

#include "windows/errors.hpp"
#include "windows/search.hpp"
#include "windows/tile_list.hpp"
#include "windows/tile_viewer.hpp"
#include "windows/texture_importer.hpp"

#ifdef __EMSCRIPTEN__
#include "examples/libs/emscripten/emscripten_mainloop_stub.h"
#endif

GLFWwindow* window;

struct {
    float scale = 1;
    glm::vec2 position;
} camera;

glm::mat4 projection;
glm::mat4 MVP;

glm::vec2 mousePos = glm::vec2(-1);
glm::vec2 lastMousePos = glm::vec2(-1);

glm::vec2 screenSize;

constexpr const char* modes[] = {"Select", "Place"};
int mouse_mode = 0;
glm::ivec2 mode0_selection = {-1, -1};
MapTile mode1_placing;

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
    error_dialog.error("{}: {}", error, description);
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

    auto mp = (mousePos - screenSize / 2.0f) / camera.scale;

    camera.scale *= scale;
    camera.position -= mp - mp / scale;
}

static void onResize(GLFWwindow* window, int width, int height) {
    screenSize = glm::vec2(width, height);
    glViewport(0, 0, width, height);
    projection = glm::ortho<float>(-width / 2, width / 2, height / 2, -height / 2, 0.0f, 100.0f);
}

static void load_data() {
    render_data->textures.update();
    history.clear();
    updateGeometry = true;
}

static bool load_game(const std::string& path) {
    if(!std::filesystem::exists(path)) {
        return false;
    }

    try {
        game_data = GameData::load_exe(path);
        load_data();

        selectedMap = 0;

        selection_handler.release();
        history.clear();
        auto& map = currentMap();
        camera.position = -(map.offset + map.size / 2) * Room::size * 8;

        return true;
    } catch(std::exception& e) {
        error_dialog.error(e.what());
    }

    return false;
}

static void load_game_dialog() {
    static std::string lastPath = "C:\\Program Files (x86)\\Steam\\steamapps\\common\\Animal Well\\";
    std::string path;
    auto result = NFD::OpenDialog({{"Game", {"exe"}}}, lastPath.c_str(), path, window);
    if(result == NFD::Result::Error) {
        error_dialog.error(NFD::GetError());
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
                assert(item.type == AssetType::Encrypted_Text);
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
        error_dialog.error(e.what());
    }
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

    updateGeometry = true;
}

class {
    std::string export_path = std::filesystem::current_path().string();
    bool has_exported = false;

  public:
    void draw_options() {
        if(ImGui::MenuItem("Open...")) {
            loadDialog();
        }

        if(has_exported) {
            const auto fileName = std::filesystem::path(export_path).filename().string();
            auto name = "Save In " + fileName;
            if(ImGui::MenuItem(name.c_str(), "Ctrl+E")) {
                export_map();
            }
        } else {
            if(ImGui::MenuItem("Save...", "Ctrl+E")) {
                export_explicit();
            }
        }
        if(ImGui::MenuItem("Save As...", "Ctrl+Shift+E")) {
            export_explicit();
        }
    }

    void export_explicit() {
        std::string path;
        auto result = NFD::PickFolder(export_path.c_str(), path, window);

        if(result == NFD::Result::Error) {
            error_dialog.error(NFD::GetError());
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
    void loadDialog() {
        std::string path;
        auto result = NFD::PickFolder(export_path.c_str(), path, window);
        if(result == NFD::Result::Error) {
            error_dialog.error(NFD::GetError());
            return;
        }
        if(result == NFD::Result::Cancel) {
            return;
        }

        export_path = path;
        try {
            game_data.load_folder(export_path);
            has_exported = true;
            load_data();

            selection_handler.release();
            history.clear();
        } catch(const std::exception& e) {
            error_dialog.error(e.what());
        }
    }

    void export_map() {
        try {
            game_data.save_folder(export_path);
            has_exported = true;
        } catch(std::exception& e) {
            error_dialog.error(e.what());
        }
    }
} saver;

static void full_map_screenshot() {
    static std::string export_path = std::filesystem::current_path().string() + "/map.png";
    std::string path;
    auto result = NFD::SaveDialog({{"png", {"png"}}}, export_path.c_str(), path, window);

    if(result == NFD::Result::Error) {
        error_dialog.error(NFD::GetError());
        return;
    }
    if(result == NFD::Result::Cancel) {
        return;
    }
    export_path = path;

    auto& map = currentMap();
    auto size = glm::ivec2(map.size.x, map.size.y) * Room::size * 8;

    Textured_Framebuffer fb(size.x, size.y);
    fb.Bind();

    glm::mat4 MVP = glm::ortho<float>(0, size.x, 0, size.y, 0.0f, 100.0f) *
                    glm::lookAt(
                        glm::vec3(map.offset * Room::size * 8, 3),
                        glm::vec3(map.offset * Room::size * 8, 0),
                        glm::vec3(0, 1, 0));

    doRender(true, game_data, selectedMap, MVP, &fb);

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
            ImGui::BeginDisabled(!game_data.loaded);
            saver.draw_options();
            ImGui::EndDisabled();

            ImGui::Separator();

            if(ImGui::MenuItem("Import exe")) {
                load_game_dialog();
            }

            ImGui::BeginDisabled(!game_data.loaded);
            if(ImGui::MenuItem("Import map")) {
                static std::string lastPath = std::filesystem::current_path().string();
                std::string path;
                auto result = NFD::OpenDialog({{"Map", {"map"}}}, lastPath.c_str(), path, window);

                if(result == NFD::Result::Error) {
                    error_dialog.error(NFD::GetError());
                } else if(result == NFD::Result::Okay) {
                    lastPath = path;
                    try {
                        auto data = readFile(path.c_str());
                        auto map = Map(std::span((uint8_t*)data.data(), data.size()));

                        if(map.coordinate_map != game_data.maps[selectedMap].coordinate_map) {
                            error_dialog.warning("Map structure differs from previously loaded map.\nMight break things so be careful.");
                        }

                        game_data.maps[selectedMap] = map;

                        history.clear();
                        updateGeometry = true;
                    } catch(std::exception& e) {
                        error_dialog.error(e.what());
                    }
                }
            }
            ImGui::EndDisabled();

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Display")) {
            int prevMap = selectedMap;
            if(ImGui::Combo("Map", &selectedMap, mapNames, 5)) {
                selection_handler.release();
                history.push_action(std::make_unique<SwitchLayer>(prevMap));
                updateGeometry = true;
            }

            ImGui::MenuItem("Foreground Tiles", nullptr, &render_data->show_fg);
            ImGui::ColorEdit4("fg tile color", &render_data->fg_color.r);
            ImGui::MenuItem("Background Tiles", nullptr, &render_data->show_bg);
            ImGui::ColorEdit4("bg tile color", &render_data->bg_color.r);
            ImGui::MenuItem("Background Texture", nullptr, &render_data->show_bg_tex);
            ImGui::ColorEdit4("bg Texture color", &render_data->bg_tex_color.r);
            ImGui::MenuItem("Show Room Grid", nullptr, &render_data->room_grid);
            ImGui::MenuItem("Show Water Level", nullptr, &render_data->show_water);
            if(ImGui::MenuItem("Accurate vines", nullptr, &render_data->accurate_vines)) {
                updateGeometry = true;
            }
            ImGui::MenuItem("Show Sprite Composition", nullptr, &render_data->sprite_composition);
            if(ImGui::MenuItem("InGame Rendering", "Ctrl+R", &render_data->accurate_render)) {
                updateGeometry = true;
            }
            if(ImGui::BeginItemTooltip()) {
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("Experimental feature");
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }

        if(ImGui::BeginMenu("Tools")) {
            ImGui::BeginDisabled(!game_data.loaded);

            if(ImGui::MenuItem("Randomize items")) {
                selection_handler.release();
                randomize();
            }
            if(ImGui::MenuItem("Export Full Map Screenshot")) {
                full_map_screenshot();
            }
            if(ImGui::MenuItem("Dump assets")) {
                dump_assets();
            }
            if(ImGui::MenuItem("Dump tile textures")) {
                dump_tile_textures();
            }
            if(ImGui::MenuItem("Clear Map")) {
                selection_handler.release();

                auto& map = currentMap();
                history.push_action(std::make_unique<MapClear>(map.rooms));

                for(auto& room : map.rooms) {
                    room.bgId = 0;
                    room.waterLevel = 180;
                    room.lighting_index = 0;
                    std::memset(room.tiles, 0, sizeof(room.tiles));
                }
                updateGeometry = true;
            }

            ImGui::EndDisabled();

            ImGui::EndMenu();
        }

        ImGui::EndMenuBar();
    }

    ImGuiID dockspace_id = ImGui::GetID("DockSpace");
    ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags, nullptr);

    static ImGuiID window_id = ImHashStr("Properties");
    // only run if no settings for Properties window are found, meaning this is the first time the program is started
    if(ImGui::FindWindowSettingsByID(window_id) == nullptr) {
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

void HelpMarker(const char* desc) {
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

// Returns true during the frame the user starts pressing down the key.
static bool GetKeyDown(ImGuiKey key) {
    return ImGui::GetKeyData(key)->DownDuration == 0.0f;
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
        if(ImGui::IsKeyDown(ImGuiKey_MouseLeft)) {
            camera.position -= delta / camera.scale;
        }
        if(ImGui::IsKeyDown(ImGuiKey_MouseRight)) {
            mode0_selection = screen_to_world(mousePos);
        }

        if(mode0_selection != glm::ivec2(-1, -1)) {
            mode0_selection += arrow_key_dir();
        }

        if(GetKeyDown(ImGuiKey_Escape)) {
            mode0_selection = glm::ivec2(-1, -1);
        }
    } else if(mouse_mode == 1) {
        auto mouse_world_pos = screen_to_world(mousePos);

        const auto holding = selection_handler.holding();
        auto selecting = selection_handler.selecting();

        if(!selecting && ImGui::IsKeyDown(ImGuiKey_MouseLeft) && ImGui::IsKeyDown(ImGuiMod_Shift)) { // select start
            selection_handler.drag_begin(mouse_world_pos);
            selecting = true;
        }
        if(selecting && !ImGui::IsKeyDown(ImGuiKey_MouseLeft)) { // select end
            selection_handler.drag_end(mouse_world_pos);
        }

        if((ImGui::IsKeyPressed(ImGuiKey_F) && mode1_layer == 1) ||
           ((ImGui::IsKeyPressed(ImGuiKey_B) || ImGui::IsKeyPressed(ImGuiKey_G)) && mode1_layer == 0)) {
            mode1_layer = !mode1_layer;
            selection_handler.change_layer(!mode1_layer, mode1_layer);
        }

        if(holding) {
            if(GetKeyDown(ImGuiKey_Escape)) selection_handler.release();
            if(GetKeyDown(ImGuiKey_Delete)) selection_handler.erase();
            if(ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
                if(GetKeyDown(ImGuiKey_C)) clipboard.copy(currentMap(), selection_handler.start(), selection_handler.size());
                if(GetKeyDown(ImGuiKey_X)) selection_handler.cut();
            }
            selection_handler.move(arrow_key_dir());
        }

        if(ImGui::IsKeyDown(ImGuiMod_Ctrl) && GetKeyDown(ImGuiKey_V)) {
            selection_handler.start_from_paste(mouse_world_pos, clipboard);

            // selection_handler will push undo data once position is finalized
            updateGeometry = true;
        }

        if(holding && selection_handler.contains(lastWorldPos)) {
            // holding & inside rect
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);

            if(ImGui::IsKeyDown(ImGuiKey_MouseLeft)) {
                selection_handler.move(mouse_world_pos - lastWorldPos);
            }
        } else if(!selecting && ImGui::IsKeyDown(ImGuiKey_MouseLeft)) {
            camera.position -= delta / camera.scale;
        }
    }

    if(ImGui::IsKeyDown(ImGuiMod_Ctrl)) {
        if(ImGui::IsKeyPressed(ImGuiKey_Z)) history.undo();
        if(ImGui::IsKeyPressed(ImGuiKey_Y)) history.redo();

        if(GetKeyDown(ImGuiKey_R)) {
            render_data->accurate_render = !render_data->accurate_render;
            updateGeometry = true;
        }
        if(GetKeyDown(ImGuiKey_S)) {
            if(ImGui::IsKeyDown(ImGuiKey_ModShift))
                saver.export_explicit();
            else
                saver.export_implicit();
        }
    }
}

static void ColorEdit4(const char* label, glm::u8vec4& col, ImGuiColorEditFlags flags = 0) {
    float col4[4] {col.r / 255.0f, col.g / 255.0f, col.b / 255.0f, col.a / 255.0f};

    ImGui::ColorEdit4(label, col4, flags);

    col.r = col4[0] * 255.f;
    col.g = col4[1] * 255.f;
    col.b = col4[2] * 255.f;
    col.a = col4[3] * 255.f;
}

static void DrawPreviewWindow() {
    ImGui::Begin("Properties");
    auto& io = ImGui::GetIO();
    // ImGui::Text("fps %f", ImGui::GetIO().Framerate);

    if(ImGui::Combo("Mode", &mouse_mode, modes, sizeof(modes) / sizeof(char*))) {
        mode0_selection = glm::ivec2(-1, -1);
        selection_handler.release();
    }

    auto& map = currentMap();

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

        auto room_pos = tile_location / Room::size;
        auto room = map.getRoom(room_pos);

        ImGui::Text("world position %i %i", tile_location.x, tile_location.y);

        if(room != nullptr) {
            if(ImGui::CollapsingHeader("Room Data", ImGuiTreeNodeFlags_DefaultOpen)) {
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                    ImGui::SetTooltip("Properties that are stored for each room (40x22 tiles)");
                }
                ImGui::Text("position %i %i", room->x, room->y);
                ImGui::InputScalar("water level", ImGuiDataType_U8, &room->waterLevel);
                const uint8_t bg_min = 0, bg_max = 19;
                if(ImGui::SliderScalar("background id", ImGuiDataType_U8, &room->bgId, &bg_min, &bg_max)) {
                    renderBgs(map);
                }

                const uint8_t pallet_max = game_data.ambient.size() - 1;
                ImGui::SliderScalar("Lighting index", ImGuiDataType_U8, &room->lighting_index, &bg_min, &pallet_max);
                ImGui::InputScalar("idk1", ImGuiDataType_U8, &room->idk1);
                ImGui::InputScalar("idk2", ImGuiDataType_U8, &room->idk2);
                ImGui::InputScalar("idk3", ImGuiDataType_U8, &room->idk3);
            }

            if(ImGui::CollapsingHeader("Lighting Data", ImGuiTreeNodeFlags_DefaultOpen)) {
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                    ImGui::SetTooltip("Properties that determine how the ambient lighting looks.\nCan be shared across multiple rooms by setting the Lighting Index");
                }
                LightingData amb {};
                if(room->lighting_index < game_data.ambient.size())
                    amb = game_data.ambient[room->lighting_index];

                ImGui::BeginDisabled(room->lighting_index >= game_data.ambient.size());

                ColorEdit4("ambient light", amb.ambient_light_color);
                ImGui::SameLine();
                HelpMarker("Alpha channel is unused");

                ColorEdit4("fg ambient light", amb.fg_ambient_light_color);
                ImGui::SameLine();
                HelpMarker("Alpha channel is unused");

                ColorEdit4("bg ambient light", amb.bg_ambient_light_color);
                ImGui::SameLine();
                HelpMarker("Alpha channel is unused");

                ColorEdit4("fog color", amb.fog_color);
                ImGui::DragFloat3("color gain", &amb.color_gain.x);
                ImGui::DragFloat("color saturation", &amb.color_saturation);
                ImGui::DragFloat("far background reflectivity", &amb.far_background_reflectivity);
                ImGui::EndDisabled();

                if(room->lighting_index < game_data.ambient.size())
                    game_data.ambient[room->lighting_index] = amb;
            }

            auto tp = glm::ivec2(tile_location.x % 40, tile_location.y % 22);
            auto tile = room->tiles[0][tp.y][tp.x];
            int tile_layer = 0;
            if(!render_data->show_fg || tile.tile_id == 0) {
                tile_layer = 1;
                tile = room->tiles[1][tp.y][tp.x];
                if(!render_data->show_bg || tile.tile_id == 0) {
                    tile = {};
                    tile_layer = 2;
                }
            }

            if(ImGui::CollapsingHeader("Instance Data", ImGuiTreeNodeFlags_DefaultOpen)) {
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                    ImGui::SetTooltip("Properties that are stored for each instance of a given tile");
                }
                ImGui::Text("position %i %i %s", tp.x, tp.y, tile_layer == 0 ? "Foreground" : tile_layer == 1 ? "Background" : "N/A");

                ImGui::BeginDisabled();

                ImGui::InputScalar("id", ImGuiDataType_U16, &tile.tile_id);
                ImGui::InputScalar("param", ImGuiDataType_U8, &tile.param);

                if(ImGui::BeginTable("tile_flags_table", 2)) {
                    int flags = tile.flags;

                    // clang-format off
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::CheckboxFlags("horizontal_mirror", &flags, 1);
                    ImGui::TableNextColumn(); ImGui::CheckboxFlags("vertical_mirror", &flags, 2);

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn(); ImGui::CheckboxFlags("rotate_90", &flags, 4);
                    ImGui::TableNextColumn(); ImGui::CheckboxFlags("rotate_180", &flags, 8);
                    // clang-format on

                    ImGui::EndTable();
                }

                ImGui::EndDisabled();
            }

            if(ImGui::CollapsingHeader("Tile Data", ImGuiTreeNodeFlags_DefaultOpen)) {
                if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                    ImGui::SetTooltip("Properties that are shared between all tiles with the same id");
                }
                ImGui::BeginDisabled(tile_layer == 2);
                auto& uv = game_data.uvs[tile.tile_id];

                updateGeometry |= DrawUvFlags(uv);
                updateGeometry |= ImGui::InputScalarN("UV", ImGuiDataType_U16, &uv.pos, 2);
                updateGeometry |= ImGui::InputScalarN("UV Size", ImGuiDataType_U16, &uv.size, 2);
                ImGui::EndDisabled();
            }
        }

    } else if(mouse_mode == 1) {
        auto mouse_world_pos = screen_to_world(mousePos);

        ImGui::SameLine();
        HelpMarker("Middle click to copy a tile.\n\
Right click to place a tile.\n\
Shift + Left click to select an area.\n\
Move selected area with Left click.\n\
Del to delete selection.\n\
Esc to deselect.\n\
ctrl + c to copy the selected area.\n\
ctrl + x to cut selected area.\n\
ctrl + v to paste at mouse position.\n\
ctrl + z to undo.\n\
ctrl + y to redo.\n\
f to move to foreground layer.\n\
b/g to move to background layer.");

        ImGui::Text("world position %i %i", mouse_world_pos.x, mouse_world_pos.y);

        auto room_pos = mouse_world_pos / Room::size;
        auto room = map.getRoom(room_pos);

        if(!io.WantCaptureMouse && room != nullptr) {
            if(ImGui::IsKeyDown(ImGuiKey_MouseMiddle)) {
                auto tp = glm::ivec2(mouse_world_pos.x % Room::size.x, mouse_world_pos.y % Room::size.y);
                auto tile = room->tiles[0][tp.y][tp.x];

                auto lastLayer = mode1_layer;

                if(render_data->show_fg && tile.tile_id != 0) {
                    mode1_placing = tile;
                    mode1_layer = 0;
                } else {
                    tile = room->tiles[1][tp.y][tp.x];
                    if(render_data->show_bg && tile.tile_id != 0) {
                        mode1_placing = tile;
                        mode1_layer = 1;
                    } else {
                        mode1_placing = {};
                    }
                }

                if(lastLayer != mode1_layer) {
                    selection_handler.change_layer(lastLayer, mode1_layer);
                }
            }
            if(ImGui::IsKeyDown(ImGuiKey_MouseRight)) {
                selection_handler.release();

                auto tp = glm::ivec2(mouse_world_pos.x % Room::size.x, mouse_world_pos.y % Room::size.y);
                auto tile_layer = room->tiles[mode1_layer];
                auto tile = tile_layer[tp.y][tp.x];
                if(tile != mode1_placing) {
                    history.push_action(std::make_unique<SingleChange>(glm::ivec3(mouse_world_pos, mode1_layer), tile));
                    tile_layer[tp.y][tp.x] = mode1_placing;
                    updateGeometry = true;
                }
            }
        }

        ImGui::NewLine();
        if(ImGui::RadioButton("Foreground", mode1_layer == 0)) {
            selection_handler.change_layer(mode1_layer, 0);
            mode1_layer = 0;
        }
        ImGui::SameLine();
        if(ImGui::RadioButton("Background", mode1_layer == 1)) {
            selection_handler.change_layer(mode1_layer, 1);
            mode1_layer = 1;
        }

        ImGui::NewLine();
        ImGui::InputScalar("id", ImGuiDataType_U16, &mode1_placing.tile_id);
        ImGui::InputScalar("param", ImGuiDataType_U8, &mode1_placing.param);

        if(ImGui::BeginTable("tile_flags_table", 2)) {
            int flags = mode1_placing.flags;
            // clang-format off
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Horizontal mirror", &flags, 1);
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Vertical mirror", &flags, 2);

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Rotate 90", &flags, 4);
            ImGui::TableNextColumn(); ImGui::CheckboxFlags("Rotate 180", &flags, 8);
            // clang-format on

            mode1_placing.flags = flags;

            ImGui::EndTable();
        }

        ImGui::SeparatorText("preview");
        ImGui_draw_tile(mode1_placing.tile_id, game_data, 0);
    }

    ImGui::End();
}

static void draw_water_level() {
    if(!render_data->show_water) {
        return;
    }

    for(const Room& room : currentMap().rooms) {
        if(room.waterLevel >= 176) {
            continue;
        }

        auto bottom_left = glm::vec2 {room.x * 40 * 8, (room.y + 1) * 22 * 8};
        auto size = glm::vec2 {40 * 8, room.waterLevel - 176};

        render_data->overlay.AddRectFilled(bottom_left, bottom_left + size, {}, {}, IM_COL32(0, 0, 255, 76));
    }
}

static void draw_overlay() {
    auto& io = ImGui::GetIO();
    auto& map = currentMap();

    if(mouse_mode == 0) {
        glm::ivec2 tile_location = mode0_selection;
        if(tile_location == glm::ivec2(-1, -1)) {
            if(io.WantCaptureMouse) {
                tile_location = glm::ivec2(-1, -1);
            } else {
                tile_location = screen_to_world(mousePos);
            }
        }

        auto room_pos = tile_location / Room::size;
        auto room = map.getRoom(room_pos);

        if(room != nullptr) {
            auto tp = glm::ivec2(tile_location.x % 40, tile_location.y % 22);
            auto tile = room->tiles[0][tp.y][tp.x];

            if(!render_data->show_fg || tile.tile_id == 0) {
                tile = room->tiles[1][tp.y][tp.x];
                if(!render_data->show_bg || tile.tile_id == 0) {
                    tile = {};
                }
            }

            const auto pos = glm::ivec2(tile_location) * 8;

            if(game_data.sprites.contains(tile.tile_id)) {
                auto sprite = game_data.sprites[tile.tile_id];

                auto bb_min = pos;
                auto bb_max = pos + glm::ivec2(sprite.size);
                render_sprite_custom([&](glm::ivec2 pos_, glm::u16vec2 size, glm::ivec2 uv_pos, glm::ivec2 uv_size) {
                    pos_ += pos;
                    auto max = pos_ + glm::ivec2(size);
                    if(render_data->sprite_composition)
                        render_data->overlay.AddRect(pos_, max, IM_COL32_WHITE, 0.5f);

                    bb_min = glm::min(glm::min(bb_min, pos_), max);
                    bb_max = glm::max(glm::max(bb_max, pos_), max);
                }, tile, game_data, room->count_yellow());

                render_data->overlay.AddRect(bb_min, bb_max, render_data->sprite_composition ? IM_COL32(255, 255, 255, 204) : IM_COL32_WHITE, 1);
            } else if(tile.tile_id != 0) {
                render_data->overlay.AddRect(pos, pos + glm::ivec2(game_data.uvs[tile.tile_id].size), IM_COL32_WHITE, 1);
            } else {
                render_data->overlay.AddRect(pos, pos + 8, IM_COL32_WHITE, 1);
            }
            if(!render_data->room_grid)
                render_data->overlay.AddRect(room_pos * Room::size * 8, room_pos * Room::size * 8 + Room::size * 8, IM_COL32(255, 255, 255, 127), 1);
        }
    } else if(mouse_mode == 1) {
        auto mouse_world_pos = screen_to_world(mousePos);
        auto room_pos = mouse_world_pos / Room::size;
        auto room = map.getRoom(room_pos);

        if(room != nullptr) {
            if(!render_data->room_grid)
                render_data->overlay.AddRect(room_pos * Room::size * 8, room_pos * Room::size * 8 + Room::size * 8, IM_COL32(255, 255, 255, 127), 1);
            render_data->overlay.AddRect(mouse_world_pos * 8, mouse_world_pos * 8 + 8);
        }

        auto start = glm::ivec2(selection_handler.start());
        if(selection_handler.holding()) {
            auto end = start + selection_handler.size();
            render_data->overlay.AddRectDashed(start * 8, end * 8, IM_COL32_WHITE, 1, 4);
        } else if(selection_handler.selecting()) {
            auto end = mouse_world_pos;
            if(end.x < start.x) {
                std::swap(start.x, end.x);
            }
            if(end.y < start.y) {
                std::swap(start.y, end.y);
            }
            render_data->overlay.AddRectDashed(start * 8, end * 8 + 8, IM_COL32_WHITE, 1, 4);
            ImGui::SetTooltip("%ix%i", end.x - start.x + 1, end.y - start.y + 1);
        }
    }

    if(render_data->room_grid) {
        for(int i = 0; i <= map.size.x; i++) {
            auto x = (map.offset.x + i) * 40 * 8;
            render_data->overlay.AddLine({x, map.offset.y * 22 * 8}, {x, (map.offset.y + map.size.y) * 22 * 8}, IM_COL32(255, 255, 255, 191), 1 / camera.scale);
        }
        for(int i = 0; i <= map.size.y; i++) {
            auto y = (map.offset.y + i) * 22 * 8;
            render_data->overlay.AddLine({map.offset.x * 40 * 8, y}, {(map.offset.x + map.size.x) * 40 * 8, y}, IM_COL32(255, 255, 255, 191), 1 / camera.scale);
        }
    }
}

static void UpdateUIScaling() {
    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    assert(xscale == yscale);

    static float oldScale = 1.0;
    if(xscale == oldScale) return;
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
        auto fs = cmrc::font::get_filesystem();
        auto dat = fs.open("ProggyVector-Regular.ttf");

        ImFontConfig config {};
        config.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void*)dat.begin(), dat.size(), std::floor(15.0f * xscale), &config);
    }
}

// Main code
static int runViewer() {
#pragma region glfw/opengl setup
    glfwSetErrorCallback(glfw_error_callback);
    if(!glfwInit())
        return 1;

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // GL ES 2.0 + GLSL 100 (WebGL 1.0)
    const char* glsl_version = "#version 100";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(IMGUI_IMPL_OPENGL_ES3)
    // GL ES 3.0 + GLSL 300 es (WebGL 2.0)
    const char* glsl_version = "#version 300 es";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
#elif defined(__APPLE__)
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    // glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    float main_scale = ImGui_ImplGlfw_GetContentScaleForMonitor(glfwGetPrimaryMonitor()); // Valid on GLFW 3.3+ only
    window = glfwCreateWindow(main_scale * 1280, main_scale * 720, "Animal Well map viewer", nullptr, nullptr);
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
    onResize(window, main_scale * 1280, main_scale * 720);
#pragma endregion

#pragma region imgui setup
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    tile_list.init(); // load settings handler for tiles

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable Docking

    // io.ConfigDpiScaleFonts = true;
    io.ConfigDpiScaleViewports = true;

#ifndef __EMSCRIPTEN__
    if(glfwGetPlatform() != GLFW_PLATFORM_WAYLAND)          // do not enable viewports for wayland
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
#endif

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
    ImGui_ImplGlfw_InstallEmscriptenCallbacks(window, "#canvas");
#endif
    ImGui_ImplOpenGL3_Init(glsl_version);

#pragma endregion

#pragma region opgenl buffer setup

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    render_data = std::make_unique<RenderData>();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

#pragma endregion

    if(!load_game("C:/Program Files (x86)/Steam/steamapps/common/Animal Well/Animal Well.exe")) {
        if(!load_game("./Animal Well.exe")) {
            error_dialog.warning("Failed to find Animal Well.exe.\nManually load it with File > Import exe\nor copy Animal Well.exe into the folder containing the editor to load it automatically in the future.");
        }
    }

    // Main loop
#ifdef __EMSCRIPTEN__
    // For an Emscripten build we are disabling file-system access, so let's not attempt to do a fopen() of the imgui.ini file.
    // You may manually call LoadIniSettingsFromMemory() to load settings from your own storage.
    io.IniFilename = nullptr;
    EMSCRIPTEN_MAINLOOP_BEGIN
#else
    while(!glfwWindowShouldClose(window))
#endif
    {
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

        render_data->overlay.clear();

        handle_input();

        glm::mat4 view = glm::lookAt(
            glm::vec3(0, 0, 3), // Camera is at (0, 0, 3), in World Space
            glm::vec3(0, 0, 0), // and looks at the origin
            glm::vec3(0, 1, 0)  // Head is up
        );

        auto model = glm::identity<glm::mat4>();
        model = glm::scale(model, glm::vec3(camera.scale, camera.scale, 1));
        model = glm::translate(model, glm::vec3(camera.position, 0));
        MVP = projection * view * model;

        DockSpaceOverViewport();
        error_dialog.drawPopup();

        // skip rendering if no data is loaded
        if(game_data.loaded) {
            search_window.draw(game_data, [](int map, const glm::ivec2 pos) {
                if(map != selectedMap) {
                    selection_handler.release();
                    history.push_action(std::make_unique<SwitchLayer>(selectedMap));
                    selectedMap = map;
                    updateGeometry = true;
                }
                // center of screen
                camera.position = -(pos * 8 + 4);
            });
            tile_list.draw(game_data, mode1_placing);
            tile_viewer.draw(game_data, updateGeometry);
            texture_importer.draw();
            DrawPreviewWindow();

            draw_overlay();
            draw_water_level();
            search_window.draw_overlay(game_data, selectedMap, camera.scale);

            doRender(updateGeometry, game_data, selectedMap, MVP);
            updateGeometry = false;
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
#ifdef __EMSCRIPTEN__
    EMSCRIPTEN_MAINLOOP_END;
#endif

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    render_data = nullptr; // delete opengl resources

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
