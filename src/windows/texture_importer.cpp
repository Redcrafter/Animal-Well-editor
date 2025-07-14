#include "texture_importer.hpp"

#include "../globals.hpp"
#include "../rendering/renderData.hpp"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <nfd.h>

glm::u16vec2 calc_tile_size(int i) {
    auto uv = game_data.uvs[i];
    auto size = uv.size;

    // clang-format off
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
                if(uv.flags & (contiguous | self_contiguous)) {
                    size.x *= 4;
                    size.y *= 4;
                }
                if(uv.flags & has_normals) {
                    size.y *= 2;
                }
            }
            break;
    }
    // clang-format on

    return size;
}

void TextureImporter::open(int tile_id) {
    open_ = true;
    selected_tile = tile_id;
    insert_pos = game_data.uvs[selected_tile].pos;
    if(insert_pos == glm::ivec2(0, 0)) {
        // find fitting empty space
    }
    last_insert_pos = insert_pos;
    image = Image();
}

void TextureImporter::apply() {
    // replace any alpha pixels with cyan
    auto vptr = image.data(); // abgr
    for(int i = 0; i < image.width() * image.height(); ++i) {
        if((vptr[i] & 0xFF000000) == 0) {
            vptr[i] = 0xFFFFFF00;
        }
    }

    game_data.uvs[selected_tile].pos = insert_pos;
    game_data.uvs[selected_tile].size = {image.width(), image.height()};

    image.copy_to(game_data.atlas, insert_pos.x, insert_pos.y);

    render_data->textures.update();
    updateGeometry = true;

    open_ = false;
}

void TextureImporter::draw() {
    if(!open_) return;

    if(ImGui::Begin("Import texture", &open_)) {
        if(ImGui::Button("Open Image")) {
            LoadImage();
        }

        if(ImGui::InputInt("tiled id", &selected_tile)) {
            selected_tile = glm::clamp(selected_tile, 0, (int)game_data.uvs.size() - 1);
            insert_pos = game_data.uvs[selected_tile].pos;
        }
        if(ImGui::InputInt2("position", &insert_pos.x)) {
            selected_tile = -1;

            auto atlas_size = glm::ivec2(game_data.atlas.width(), game_data.atlas.height());
            auto image_size = glm::ivec2(image.width(), image.height());
            insert_pos = glm::clamp(insert_pos, glm::ivec2(0), atlas_size - image_size);
        }

        if(ImGui::Button("Apply")) {
            apply();
        }
        drawCanvas();
    }
    ImGui::End();
}

void TextureImporter::drawCanvas() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));      // Disable padding
    ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(50, 50, 50, 255));  // Set a background color
    ImGui::BeginChild("canvas", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoMove);
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    auto atlas_size = glm::ivec2(game_data.atlas.width(), game_data.atlas.height()) * scale;
    auto image_size = glm::ivec2(image.width(), image.height()) * scale;

    ImGui::Image((ImTextureID)render_data->textures.atlas.id.value, ImVec2(atlas_size.x, atlas_size.y));

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    // const glm::vec2 p = glm::vec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
    auto wr = ImGui::GetCurrentWindow()->WorkRect.Min;
    auto p = glm::ivec2(wr.x, wr.y);
    auto mouse = ImGui::GetMousePos();

    for(size_t i = 0; i < game_data.uvs.size(); ++i) {
        auto uv = game_data.uvs[i];
        auto size = calc_tile_size(i);

        if(i == 793 || i == 794) {
            continue;
        }

        auto start = p + glm::ivec2(uv.pos) * scale;
        auto end = start + glm::ivec2(size) * scale;

        if(mouse.x >= start.x && mouse.y >= start.y && mouse.x < end.x && mouse.y < end.y) {
            ImGui::SetTooltip("%zu", i);
        }

        draw_list->AddRect(ImVec2(start.x, start.y), ImVec2(end.x, end.y), IM_COL32_WHITE, 0, 0, 2);
    }

    if(image_size != glm::ivec2(0, 0)) {
        ImGui::SetCursorPos(ImVec2(insert_pos.x * scale, insert_pos.y * scale));
        ImGui::InvisibleButton("###asdf", ImVec2(image_size.x, image_size.y));

        if(ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }

        auto s = ImGui::GetCurrentWindow()->Scroll;
        auto scroll = glm::ivec2(s.x, s.y);

        if(ImGui::IsItemActive()) {
            auto delta = ImGui::GetMouseDragDelta();
            insert_pos = last_insert_pos + (glm::ivec2(delta.x, delta.y) - (last_scroll - scroll)) / 4;
        } else {
            last_insert_pos = insert_pos;
            last_scroll = scroll;
        }
        ImGui::SetCursorPos(ImVec2(insert_pos.x * scale, insert_pos.y * scale));
        ImGui::Image((ImTextureID)image_texture->id.value, ImVec2(image_size.x, image_size.y));

        auto start = p + insert_pos * scale;
        auto end = start + glm::ivec2(image.width(), image.height()) * scale;
        draw_list->AddRect(ImVec2(start.x, start.y), ImVec2(end.x, end.y), IM_COL32_WHITE, 0, 0, 2);
    }

    ImGui::EndChild();
}

void TextureImporter::LoadImage() {
    std::string path;
    auto result = NFD::OpenDialog({{"png", {"png"}}}, nullptr, path, glfwGetCurrentContext());

    if(result == NFD::Result::Error) {
        error_dialog.error(NFD::GetError());
    }
    if(result != NFD::Result::Okay) {
        return;
    }

    if(!image_texture) {
        image_texture.emplace();
    }

    try {
        image = Image(path);
        auto copy = image.copy();

        // chroma key cyan and replace with alpha
        auto vptr = copy.data();
        for(int i = 0; i < copy.width() * copy.height(); ++i) {
            if(vptr[i] == 0xFFFFFF00) {
                vptr[i] = 0;
            }
        }
        image_texture->Load(copy);
    } catch(const std::exception& e) {
        image = Image();
        error_dialog.error(e.what());
    }
}
