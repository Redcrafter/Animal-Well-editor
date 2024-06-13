#include "tile_viewer.hpp"

#include <algorithm>

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include "sprite_viewer.hpp"

bool DrawUvFlags(uv_data& uv) {
    if(!ImGui::BeginTable("uv_flags_table", 2))
        return false;

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

    ImGui::EndTable();

    if(flags != uv.flags) {
        uv.flags = flags;
        return true;
    }

    return false;
}

void TileViewer::draw(GameData& game_data, const Texture& atlas, bool& should_update) {
    if(!ImGui::Begin("Tile Viewer")) {
        ImGui::End();
        return;
    }

    ImGui::InputInt("Id", &selected_tile);
    selected_tile = std::clamp(selected_tile, 0, (int)game_data.uvs.size() - 1);

    auto& uv = game_data.uvs[selected_tile];

    auto atlas_size = glm::vec2(atlas.width, atlas.height);
    auto uv_tl = glm::vec2(uv.pos) / atlas_size;
    auto uv_br = glm::vec2(uv.pos + uv.size) / atlas_size;

    ImGui::Text("preview");
    ImGui::Image((ImTextureID)atlas.id.value, ImVec2(uv.size.x * 8, uv.size.y * 8), ImVec2(uv_tl.x, uv_tl.y), ImVec2(uv_br.x, uv_br.y));

    should_update |= DrawUvFlags(uv);
    should_update |= ImGui::InputScalarN("UV", ImGuiDataType_U16, &uv.pos, 2);
    should_update |= ImGui::InputScalarN("UV Size", ImGuiDataType_U16, &uv.size, 2);

    if(game_data.sprites.contains(selected_tile)) {
        auto spriteId = std::ranges::find(spriteMapping, selected_tile, [](const TileMapping t) { return t.tile_id; })->internal_id;

        ImGui::NewLine();
        ImGui::Text("Sprite id %i", spriteId);
        if(ImGui::Button("Open in Sprite Viewer")) {
            sprite_viewer.select(spriteId);
            sprite_viewer.focus();
        }
    }

    ImGui::End();
}

void TileViewer::focus() {
    ImGui::FocusWindow(ImGui::FindWindowByName("Tile Viewer"));
}
