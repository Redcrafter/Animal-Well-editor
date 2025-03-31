#include "tile_viewer.hpp"

#include <algorithm>

#include <glm/glm.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include "../rendering/renderData.hpp"

static ImVec2 toImVec(const glm::vec2 vec) {
    return ImVec2(vec.x, vec.y);
}

Texture& get_tex_for_tile(int tile_id) {
    if(tile_id == 794) {
        return render_data->textures.bunny;
    }
    if(tile_id == 793) {
        return render_data->textures.time_capsule;
    }
    return render_data->textures.atlas;
}

void ImGui_draw_tile(uint16_t tile_id, const GameData& game_data, int frame) {
    auto uv = game_data.uvs[tile_id];

    auto& tex = get_tex_for_tile(tile_id);
    auto atlas_size = glm::vec2(tex.width, tex.height);

    if(game_data.sprites.contains(tile_id)) {
        auto& sprite = game_data.sprites.at(tile_id);
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

            auto aUv = glm::vec2(uv.pos + subsprite.atlas_pos);
            auto size = glm::vec2(subsprite.size);
            auto ap = pos + glm::vec2(subsprite.composite_pos) * 8.0f;

            draw_list->AddImage((ImTextureID)tex.id.value, toImVec(ap), toImVec(ap + glm::vec2(subsprite.size) * 8.0f), toImVec(aUv / atlas_size), toImVec((aUv + size) / atlas_size));
        }
    } else {
        auto pos = glm::vec2(uv.pos);
        auto size = glm::vec2(uv.size);

        if(uv.flags & (contiguous | self_contiguous)) {
            assert(uv.size == glm::u16vec2(8));
            size *= 4;
        }
        ImGui::Image((ImTextureID)tex.id.value, toImVec(size * 8.0f), toImVec(pos / atlas_size), toImVec((pos + size) / atlas_size));
    }
}

bool DrawUvFlags(uv_data& uv) {
    if(!ImGui::BeginTable("uv_flags_table", 2))
        return false;

    uint32_t flags = uv.flags;
    // clang-format off
    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides left", &flags, collides_left); // correct
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Hidden", &flags, 1 << 10);

    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides right", &flags, collides_right); // correct
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Blocks Light", &flags, blocks_light);

    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides up", &flags, collides_up); // correct
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("obscures", &flags, obscures);

    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Collides down", &flags, collides_down); // correct
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Contiguous", &flags, contiguous); // correct

    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Not Placeable", &flags, not_placeable);
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Self Contiguous", &flags, self_contiguous); // correct

    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Additive", &flags, additive);
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Dirt", &flags, dirt);

    ImGui::TableNextRow();
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("Has Normals", &flags, has_normals); // correct
    ImGui::TableNextColumn(); ImGui::CheckboxFlags("UV Light", &flags, uv_light); // correct
    // clang-format on

    ImGui::EndTable();

    if(flags != uv.flags) {
        uv.flags = (uv_flags)flags;
        return true;
    }

    return false;
}

void TileViewer::draw(GameData& game_data, bool& should_update) {
    if(!ImGui::Begin("Tile Viewer")) {
        ImGui::End();
        return;
    }

    if(ImGui::InputInt("Id", &selected_tile)) {
        selected_animation = 0;
        selected_frame = 0;
    }
    selected_tile = std::clamp(selected_tile, 0, (int)game_data.uvs.size() - 1);

    auto& uv = game_data.uvs[selected_tile];

    ImGui::SeparatorText("Tile Data");
    should_update |= DrawUvFlags(uv);
    should_update |= ImGui::InputScalarN("UV", ImGuiDataType_U16, &uv.pos, 2);
    should_update |= ImGui::InputScalarN("UV Size", ImGuiDataType_U16, &uv.size, 2);

    if(!game_data.sprites.contains(selected_tile)) {
        ImGui::SeparatorText("preview");
    } else {
        // auto selected_sprite = std::ranges::find(spriteMapping, selected_tile, [](const TileMapping t) { return t.tile_id; })->internal_id;
        ImGui::SeparatorText("Sprite Data");

        auto& sprite = game_data.sprites[selected_tile];

        ImGui::Text("Composite size %i %i", sprite.size.x, sprite.size.y);
        ImGui::Text("Layer count %zu", sprite.layers.size());
        ImGui::Text("Subsprite count %zu", sprite.sub_sprites.size());

        if(!sprite.animations.empty()) {
            ImGui::NewLine();
            if(ImGui::SliderInt("animation", &selected_animation, 0, std::max(0, (int)sprite.animations.size() - 1)) && playing) {
                selected_frame = sprite.animations[selected_animation].start;
            }
            auto& anim = sprite.animations[selected_animation];

            auto inner_spacing = ImGui::GetStyle().ItemInnerSpacing.x;
            {
                uint16_t min = 0, max = sprite.frame_count - 1;

                ImGui::PushMultiItemsWidths(2, ImGui::CalcItemWidth());
                if(ImGui::SliderScalar("##min", ImGuiDataType_U16, &anim.start, &min, &max)) {
                    anim.end = std::clamp(anim.end, anim.start, max);
                }
                ImGui::PopItemWidth();
                ImGui::SameLine(0, inner_spacing);

                if(ImGui::SliderScalar("##max", ImGuiDataType_U16, &anim.end, &min, &max)) {
                    anim.start = std::clamp(anim.start, min, anim.end);
                }
                ImGui::PopItemWidth();
                ImGui::SameLine(0, inner_spacing);

                ImGui::Text("frame range");
            }

            int type = anim.type;
            static const char* names[3] {"forward", "backward", "alternating"};
            ImGui::Combo("type", &type, names, 3);
            anim.type = (uint16_t)type;

            if(playing) {
                if(anim.start < anim.end) {
                    frame_step++;
                    if(frame_step > speed) {
                        selected_frame += direction;

                        if(selected_frame > anim.end) {
                            if(anim.type == 0) {
                                selected_frame = anim.start;
                            } else if(anim.type == 1) {
                                direction = -1;
                            } else if(anim.type == 2) {
                                direction = -1;
                                selected_frame -= 2;
                            }
                        }
                        if(selected_frame < anim.start) {
                            if(anim.type == 0) {
                                direction = 1;
                            } else if(anim.type == 1) {
                                selected_frame = anim.end;
                            } else {
                                direction = 1;
                                selected_frame += 2;
                            }
                        }
                        frame_step = 0;
                    }
                }

                if(ImGui::Button("Pause")) playing = false;
            } else {
                if(ImGui::Button("Play")) {
                    playing = true;
                    if(anim.type != 1) {
                        selected_frame = anim.start;
                        direction = 1;
                    } else {
                        selected_frame = anim.end;
                        direction = -1;
                    }
                    frame_step = 0;
                }
            }

            ImGui::SameLine(0, inner_spacing);

            ImGui::PushItemWidth(ImGui::CalcItemWidth() - ImGui::GetCursorPosX() + ImGui::GetStyle().ItemSpacing.x);

            ImGui::InputInt("Speed", &speed);
            ImGui::PopItemWidth();

            speed = std::max(0, speed);
        }

        if(sprite.frame_count > 1) {
            ImGui::NewLine();
            ImGui::SliderInt("frame", &selected_frame, 0, sprite.frame_count - 1);
        }
    }

    ImGui_draw_tile(selected_tile, game_data, selected_frame);

    ImGui::End();
}

void TileViewer::focus() {
    ImGui::FocusWindow(ImGui::FindWindowByName("Tile Viewer"));
}
