#include "sprite_viewer.hpp"

#include <algorithm>

#include <imgui.h>
#include <imgui_internal.h>

static ImVec2 toImVec(const glm::vec2 vec) {
    return ImVec2(vec.x, vec.y);
}

static void ImGui_draw_sprite(const SpriteData& sprite, int frame, glm::u16vec2 uv, const Texture& atlas) {
    auto atlasSize = glm::vec2(atlas.width, atlas.height);

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

        draw_list->AddImage((ImTextureID)atlas.id.value, toImVec(ap), toImVec(ap + glm::vec2(subsprite.size) * 8.0f), toImVec(aUv / atlasSize), toImVec((aUv + size) / atlasSize));
    }
}

void SpriteViewer::draw(GameData& game_data, const Texture& atlas) {
    if(!ImGui::Begin("Sprite Viewer")) {
        ImGui::End();
        return;
    }
    if(ImGui::InputInt("Id", &selected_sprite)) {
        select(selected_sprite);
    }

    auto tile_id = spriteMapping[selected_sprite].tile_id;
    auto& sprite = game_data.sprites[tile_id];
    auto& uv = game_data.uvs[tile_id];

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
            uint16_t min = 0, max = sprite.composition_count - 1;

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

    if(sprite.composition_count > 1) {
        ImGui::NewLine();
        ImGui::SliderInt("frame", &selected_frame, 0, sprite.composition_count - 1);
    }

    ImGui_draw_sprite(sprite, selected_frame, uv.pos, atlas);

    ImGui::End();
}

void SpriteViewer::select(int id) {
    selected_sprite = std::clamp(id, 0, 157);
    selected_animation = 0;
    selected_frame = 0;
    playing = false;
    frame_step = 0;
}

void SpriteViewer::select_from_tile(int tile_id) {
    // todo: lazy implementation could be improved with map
    auto spriteId = std::ranges::find(spriteMapping, tile_id, [](const TileMapping t) { return t.tile_id; })->internal_id;
    select(spriteId);
}

void SpriteViewer::focus() {
    ImGui::FocusWindow(ImGui::FindWindowByName("Sprite Viewer"));
}
