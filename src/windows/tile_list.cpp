#include "tile_list.hpp"

#include <regex>
#include <format>

#define IMGUI_DEFINE_MATH_OPERATORS
#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include "../rendering/geometry.hpp"
#include "tile_viewer.hpp"

Texture& get_tex_for_tile(int tile_id);
void HelpMarker(const char* desc);

static ImVec2 toImVec(const glm::vec2 vec) {
    return ImVec2(vec.x, vec.y);
}

static bool TileButton(uint16_t tile, const GameData& game_data, int box_size, int frame = 0) {
    const auto id = ImGui::GetID("");

    ImGuiContext& g = *GImGui;
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if(window->SkipItems)
        return false;

    const auto size = ImVec2(box_size, box_size);
    const ImVec2 padding = g.Style.FramePadding;
    const ImRect bb(window->DC.CursorPos, window->DC.CursorPos + size + padding * 2.0f);
    ImGui::ItemSize(bb);
    if(!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;

    ImGuiButtonFlags flags = 0;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, flags);

    // Render
    const auto col = ImGui::GetColorU32((held && hovered) ? ImGuiCol_ButtonActive : hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    ImGui::RenderNavHighlight(bb, id);
    ImGui::RenderFrame(bb.Min, bb.Max, col, true, ImClamp((float)ImMin(padding.x, padding.y), 0.0f, g.Style.FrameRounding));

    auto& tex = get_tex_for_tile(tile);
    const auto atlas_size = ImVec2(tex.width, tex.height);

    auto uv = game_data.uvs[tile];

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    auto uv0 = toImVec(uv.pos) / atlas_size;

    auto p1 = bb.Min + padding;
    auto p2 = bb.Max - padding;

    if(game_data.sprites.contains(tile)) {
        auto sprite = game_data.sprites.at(tile);
        auto s_min = glm::ivec2(INT_MAX);
        auto s_max = glm::ivec2(INT_MIN);

        render_sprite_custom([&](glm::ivec2 pos, glm::u16vec2 size, glm::ivec2 uv_pos, glm::ivec2 uv_size) {
            s_min = glm::min(s_min, pos);
            s_max = glm::max(s_max, pos + glm::ivec2(size));
        }, {tile}, game_data, 0);

        auto s_size = s_max - s_min;

        float div = std::max(s_size.x, s_size.y) / (float)box_size;

        if(s_size.x > s_size.y) {
            auto height = size.y / (s_size.x / (float)s_size.y);
            auto center = (p1.y + p2.y) / 2;
            p1.y = center - height / 2;
            p2.y = center + height / 2;
        }
        if(s_size.x < s_size.y) {
            auto width = size.x / (s_size.y / (float)s_size.x);
            auto center = (p1.x + p2.x) / 2;
            p1.x = center - width / 2;
            p2.x = center + width / 2;
        }

        render_sprite_custom([&](glm::ivec2 pos, glm::u16vec2 size, glm::ivec2 uv_pos, glm::ivec2 uv_size) {
            auto aUv = toImVec(uv_pos) / atlas_size;
            auto ap = p1 + toImVec(pos - s_min) / div;
            draw_list->AddImage((ImTextureID)tex.id.value, ap, ap + toImVec(size) / div, aUv, aUv + toImVec(uv_size) / atlas_size);
        }, {tile}, game_data, 0);
    } else {
        auto uv1 = toImVec(uv.pos + uv.size) / atlas_size;

        if(uv.size.x > uv.size.y) {
            auto height = size.y / (uv.size.x / (float)uv.size.y);
            auto center = (p1.y + p2.y) / 2;
            p1.y = center - height / 2;
            p2.y = center + height / 2;
        }
        if(uv.size.x < uv.size.y) {
            auto width = size.x / (uv.size.y / (float)uv.size.x);
            auto center = (p1.x + p2.x) / 2;
            p1.x = center - width / 2;
            p2.x = center + width / 2;
        }

        if(uv.flags & (contiguous | self_contiguous)) {
            // draw tile without neighbours
            window->DrawList->AddImage((ImTextureID)tex.id.value, p1, p2, uv0 + ImVec2(0, 0) / atlas_size, uv1 + ImVec2(16, 16) / atlas_size, IM_COL32_WHITE);
        } else {
            window->DrawList->AddImage((ImTextureID)tex.id.value, p1, p2, uv0, uv1, IM_COL32_WHITE);
        }
    }

    return pressed;
}

void TileList::draw(const GameData& game_data, MapTile& mode1_placing) {
    if(!ImGui::Begin("Tile List")) {
        ImGui::End();
        return;
    }

    ImGui::InputInt("size", &box_size);
    box_size = std::max(box_size, 1);
    ImGui::SameLine();
    HelpMarker("Left click a tile to copy it to edit mode.\nMiddle click a tile to open it in the tile viewer.\nPress 'Del' while hovering over a tile to delete it.\nTiles and groups can be reordered by dragging them around.");

    auto context = ImGui::GetCurrentContext();
    auto window = context->CurrentWindow;

    auto width = window->WorkRect.GetWidth();
    auto pad = context->Style.FramePadding;

    auto mp = context->IO.MousePos;
    auto el_width = box_size + pad.x * 2;

    glm::ivec2 insert_pos {-1, -1};
    int group_insert_pos = -1;
    bool drag_end = false;

    for(size_t i = 0; i < groups.size(); i++) {
        auto& group = groups[i];
        ImGui::PushID(i);

        auto sp = window->DC.CursorPos;

        bool open = ImGui::CollapsingHeader("##header", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
        if(ImGui::IsItemActive() && !ImGui::IsItemHovered()) { // start dragging
            group_drag_start = i;
        }
        if(ImGui::IsItemDeactivated()) {
            drag_end = true;
        }

        ImGui::SameLine();
        ImGui::InputText("##name", &group.name);
        ImGui::SameLine();
        if(ImGui::SmallButton("Delete")) {
            groups.erase(groups.begin() + i);
            i--;
            ImGui::PopID();
            continue;
        }

        if(open) {
            for(size_t j = 0; j < group.tiles.size(); j++) {
                auto tile = group.tiles[j];
                ImGui::PushID(j);

                auto sp = window->DC.CursorPos;

                if(TileButton(tile, game_data, box_size) && drag_start.x == -1) {
                    mode1_placing = {tile};
                }

                if(ImGui::IsItemHovered() && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                    group.tiles.erase(group.tiles.begin() + j);
                    j--;
                }
                if(ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
                    tile_viewer.select_tile(tile);
                    tile_viewer.focus();
                }
                if(ImGui::IsItemActive() && !ImGui::IsItemHovered()) { // start dragging
                    drag_start = {i, j};
                }
                if(ImGui::IsItemDeactivated()) {
                    drag_end = true;
                }

                ImGui::SetItemTooltip("%i", tile);
                ImGui::PopID();
                ImGui::SameLine();

                auto ep = window->DC.CursorPos;

                if(drag_start.x != -1) {
                    if(mp.y > ep.y && mp.y < ep.y + window->DC.CurrLineSize.y) {
                        float y1 = window->DC.CursorPos.y;
                        float y2 = window->DC.CursorPos.y + window->DC.CurrLineSize.y;

                        if(mp.x >= sp.x - pad.x && mp.x < sp.x + el_width / 2) {
                            float x = sp.x - pad.x;
                            window->DrawList->AddRectFilled(ImVec2(x - 1, y1), ImVec2(x + 1, y2), IM_COL32_WHITE);
                            insert_pos = {i, j}; // insert at current index
                        }
                        if(mp.x >= sp.x + el_width / 2 && mp.x < ep.x - pad.x) {
                            float x = ep.x - pad.x;
                            window->DrawList->AddRectFilled(ImVec2(x - 1, y1), ImVec2(x + 1, y2), IM_COL32_WHITE);
                            insert_pos = {i, j + 1}; // insert at next index
                        }
                    }
                }

                if(ImGui::GetCursorPosX() + box_size + pad.x >= width) {
                    ImGui::NewLine();
                }
            }

            if(ImGui::Button("+", {pad.x * 2 + box_size, pad.y * 2 + box_size})) {
                group.tiles.push_back(tile_viewer.get_selected());
            }
            if(ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
                ImGui::SetTooltip("Add tile currently selected in tile viewer");
            }
        }

        auto ep = window->DC.CursorPos;

        if(group_drag_start != -1) {
            auto height = ep.y - sp.y;
            auto y = mp.y - sp.y;

            if(y > 0 && y < height / 2) {
                // top half
                auto y1 = sp.y - context->Style.ItemSpacing.y / 2;
                window->DrawList->AddRectFilled(ImVec2(sp.x, y1 - 1), ImVec2(sp.x + width, y1 + 1), IM_COL32_WHITE);
                group_insert_pos = i;
            }
            if(y < height && y > height / 2) {
                // bottom half
                auto y1 = ep.y - context->Style.ItemSpacing.y / 2;
                window->DrawList->AddRectFilled(ImVec2(ep.x, y1 - 1), ImVec2(ep.x + width, y1 + 1), IM_COL32_WHITE);
                group_insert_pos = i + 1;
            }
        }

        ImGui::PopID();
    }

    if(ImGui::Button("+")) {
        groups.push_back(TileGroup { std::format("group {}", groups.size() + 1), {}});
    }

    if(drag_end) {
        if(group_insert_pos != -1) {
            auto g = groups[group_drag_start];
            groups.erase(groups.begin() + group_drag_start);

            if(group_insert_pos > group_drag_start) {
                group_insert_pos--;
            }
            groups.insert(groups.begin() + group_insert_pos, g);
        }
        if(insert_pos != glm::ivec2(-1, -1)) {
            auto& ga = groups[drag_start.x].tiles;

            auto el = ga[drag_start.y];
            ga.erase(ga.begin() + drag_start.y);

            if(insert_pos.x == drag_start.x) {
                // same group
                if(insert_pos.y > drag_start.y) {
                    insert_pos.y--;
                }

                ga.insert(ga.begin() + insert_pos.y, el);
            } else {
                auto& gb = groups[insert_pos.x].tiles;
                gb.insert(gb.begin() + insert_pos.y, el);
            }
        }
        drag_start = {-1, -1};
        group_drag_start = -1;
    }

    ImGui::End();
}

static void* MyUserData_ReadOpen(ImGuiContext* ctx, ImGuiSettingsHandler* handler, const char* name) {
    tile_list.groups.clear();
    return &tile_list; // anything other than nullptr
}
static void MyUserData_ReadLine(ImGuiContext* ctx, ImGuiSettingsHandler* handler, void* entry, const char* line) {
    const std::regex rgx("^group=(.*)\\|([0-9,]*)$");
    std::smatch matches;

    std::string s = line;
    if(s.empty())
        return;
    if(std::regex_search(s, matches, rgx)) {
        TileGroup g;
        g.name = matches[1];

        std::string str = matches[2];
        if(!str.empty()) {
            size_t pos_start = 0;
            size_t pos_end = 0;

            while((pos_end = str.find(',', pos_start)) != std::string::npos) {
                g.tiles.push_back(std::stoi(str.substr(pos_start, pos_end - pos_start)));
                pos_start = pos_end + 1;
            }
            g.tiles.push_back(std::stoi(str.substr(pos_start)));
        }
        tile_list.groups.push_back(g);
    } else {
        error_dialog.error("Invalid tile list format");
    }
}

static void MyUserData_WriteAll(ImGuiContext* ctx, ImGuiSettingsHandler* handler, ImGuiTextBuffer* out_buf) {
    out_buf->append("[UserData][Tile_List]\n");

    for(auto& g : tile_list.groups) {
        out_buf->appendf("group=%s|", g.name.c_str());

        for(size_t i = 0; i < g.tiles.size(); i++) {
            auto tile = g.tiles[i];
            if(i == 0) {
                out_buf->appendf("%i", tile);
            } else {
                out_buf->appendf(",%i", tile);
            }
        }
        out_buf->append("\n");
    }
}

void TileList::init() {
    ImGuiSettingsHandler ini_handler;
    ini_handler.TypeName = "UserData";
    ini_handler.TypeHash = ImHashStr("UserData");
    ini_handler.ReadOpenFn = MyUserData_ReadOpen;
    ini_handler.ReadLineFn = MyUserData_ReadLine;
    ini_handler.WriteAllFn = MyUserData_WriteAll;
    ImGui::AddSettingsHandler(&ini_handler);
}
