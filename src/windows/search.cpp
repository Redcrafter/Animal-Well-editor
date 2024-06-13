#include "search.hpp"

#include <algorithm>

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>

ImGuiTableFlags flags = ImGuiTableFlags_Hideable | ImGuiTableFlags_Sortable | ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersV | ImGuiTableFlags_Resizable;

void SearchWindow::draw(const GameData& game_data, std::function<void(int, glm::ivec2)> callback) {
    if(ImGui::Begin("Search")) {
        ImGui::InputInt("tile_id", &tile_id);

        if(ImGui::IsItemDeactivated() && (ImGui::IsKeyPressed(ImGuiKey_Enter, ImGui::GetItemID()) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, ImGui::GetItemID()))) {
            search(game_data);
        }

        if(ImGui::Button("Search")) {
            search(game_data);
        }
        ImGui::SameLine();
        if(ImGui::Button("Clear")) {
            results.clear();
        }
        ImGui::Checkbox("Highlight", &show_on_map);

        ImGui::Text("%zu results", results.size());
        if(ImGui::BeginTable("search_results", 9, flags)) {
            ImGui::TableSetupScrollFreeze(0, 1); // Make top row always visible

            ImGui::TableSetupColumn("map", ImGuiTableColumnFlags_None);
            ImGui::TableSetupColumn("layer", ImGuiTableColumnFlags_None);
            ImGui::TableSetupColumn("x", ImGuiTableColumnFlags_None);
            ImGui::TableSetupColumn("y", ImGuiTableColumnFlags_None);

            ImGui::TableSetupColumn("room_x", ImGuiTableColumnFlags_DefaultHide);
            ImGui::TableSetupColumn("room_y", ImGuiTableColumnFlags_DefaultHide);

            ImGui::TableSetupColumn("tile_x", ImGuiTableColumnFlags_DefaultHide);
            ImGui::TableSetupColumn("tile_y", ImGuiTableColumnFlags_DefaultHide);

            ImGui::TableSetupColumn("goto", ImGuiTableColumnFlags_None);
            ImGui::TableHeadersRow();

            if(ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
                if(sort_specs->SpecsDirty) {
                    auto spec = sort_specs->Specs[0];

                    std::stable_sort(results.begin(), results.end(), [&](const SearchResult& lhs, const SearchResult& rhs) {
                        int delta = 0;
                        switch(spec.ColumnIndex) {
                            case 0: delta = lhs.map - rhs.map; break;
                            case 1: delta = lhs.layer - rhs.layer; break;
                            case 2: delta = (lhs.room_pos.x * 40 + lhs.tile_pos.x) - (rhs.room_pos.x * 40 + rhs.tile_pos.x); break;
                            case 3: delta = (lhs.room_pos.y * 22 + lhs.tile_pos.y) - (rhs.room_pos.y * 22 + rhs.tile_pos.y); break;
                            case 4: delta = lhs.room_pos.x - rhs.room_pos.x; break;
                            case 5: delta = lhs.room_pos.y - rhs.room_pos.y; break;
                            case 6: delta = lhs.tile_pos.x - rhs.tile_pos.x; break;
                            case 7: delta = lhs.tile_pos.y - rhs.tile_pos.y; break;
                        }

                        if(spec.SortDirection == ImGuiSortDirection_Ascending)
                            return delta < 0;
                        if(spec.SortDirection == ImGuiSortDirection_Descending)
                            return delta > 0;

                        return false;
                    });

                    sort_specs->SpecsDirty = false;
                }
            }

            // using clipper for large vertical lists
            ImGuiListClipper clipper;
            clipper.Begin(results.size());
            while(clipper.Step()) {
                for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++) {
                    ImGui::TableNextRow();

                    auto el = results[row];
                    auto pos = el.room_pos * glm::ivec2(40, 22) + el.tile_pos;

                    // clang-format off
                    ImGui::TableSetColumnIndex(0); ImGui::Text("%s", mapNames[el.map]);
                    ImGui::TableSetColumnIndex(1); ImGui::Text("%i", el.layer);
                    ImGui::TableSetColumnIndex(2); ImGui::Text("%i", pos.x);
                    ImGui::TableSetColumnIndex(3); ImGui::Text("%i", pos.y);

                    ImGui::TableSetColumnIndex(4); ImGui::Text("%i", el.room_pos.x);
                    ImGui::TableSetColumnIndex(5); ImGui::Text("%i", el.room_pos.y);

                    ImGui::TableSetColumnIndex(6); ImGui::Text("%i", el.tile_pos.x);
                    ImGui::TableSetColumnIndex(7); ImGui::Text("%i", el.tile_pos.y);
                    // clang-format on

                    ImGui::TableSetColumnIndex(8);
                    ImGui::PushID(row);
                    if(ImGui::SmallButton("goto")) {
                        callback(el.map, pos);
                    }
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
    }
    ImGui::End();
}

void SearchWindow::search(const GameData& game_data) {
    results.clear();
    searched_tile = tile_id;

    for(size_t i = 0; i < game_data.maps.size(); i++) {
        auto& map = game_data.maps[i];

        for(auto& room : map.rooms) {
            for(int y = 0; y < 22; y++) {
                for(int x = 0; x < 40; x++) {
                    auto tile1 = room.tiles[0][y][x];
                    if(tile1.tile_id == tile_id) {
                        results.push_back(SearchResult {(uint8_t)i, 0, glm::ivec2(room.x, room.y), glm::ivec2(x, y)});
                    }

                    auto tile2 = room.tiles[1][y][x];
                    if(tile2.tile_id == tile_id) {
                        results.push_back(SearchResult {(uint8_t)i, 1, glm::ivec2(room.x, room.y), glm::ivec2(x, y)});
                    }
                }
            }
        }
    }
}

void SearchWindow::draw_overlay(const GameData& game_data, int selectedMap, Mesh& overlay, float gScale) {
    if(!show_on_map || results.empty()) return;

    glm::vec2 size;

    if(game_data.sprites.contains(searched_tile)) {
        size = game_data.sprites.at(searched_tile).size;
    } else {
        size = game_data.uvs[searched_tile].size;
    }

    for(auto result : results) {
        if(result.map != selectedMap) continue;

        auto p = glm::vec2(result.room_pos * glm::ivec2(40, 22) + result.tile_pos) * 8.0f;
        overlay.AddRect(p, p + size, IM_COL32(255, 0, 0, 204), 8 / gScale);
    }
}
