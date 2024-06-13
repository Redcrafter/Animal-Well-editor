#include "tile_list.hpp"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

#include "tile_viewer.hpp"

void TileList::draw(const GameData& game_data, const Texture& atlas) {
    if(!ImGui::Begin("Tile List")) {
        ImGui::End();
        return;
    }

    if(tiles_.empty()) {
        auto atlas_size = glm::vec2(atlas.width, atlas.height);

        for(size_t i = 0; i < game_data.uvs.size(); ++i) {
            auto uv = game_data.uvs[i];
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
        if(ImGui::ImageButton((ImTextureID)atlas.id.value, ImVec2(32, 32), ImVec2(pos.x, pos.y), ImVec2(pos.x + size.x, pos.y + size.y))) {
            tile_viewer.selected_tile = id;
            tile_viewer.focus();
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
