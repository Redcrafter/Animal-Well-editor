#include "pipeline.hpp"
#include "geometry.hpp"
#include "renderData.hpp"

#include <imgui.h>
#include <GLFW/glfw3.h>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

constexpr auto room_size = glm::ivec2(40, 22);

void RenderQuad(glm::vec2 min, glm::vec2 max, glm::vec2 uv_min, glm::vec2 uv_max) {
    static std::unique_ptr<VAO> quadVAO = nullptr;
    static std::unique_ptr<VBO> quadVBO = nullptr;

    // clang-format off
    const float quadVertices[] = {
        // positions        // texture Coords
        min.x, max.y, uv_min.x, uv_max.y, 1, 1, 1, 1,
        min.x, min.y, uv_min.x, uv_min.y, 1, 1, 1, 1,
        max.x, max.y, uv_max.x, uv_max.y, 1, 1, 1, 1,
        max.x, min.y, uv_max.x, uv_min.y, 1, 1, 1, 1,
    };
    // clang-format on

    if(quadVAO == nullptr) {
        quadVAO = std::make_unique<VAO>();
        quadVAO->Bind();

        quadVBO = std::make_unique<VBO>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
        quadVBO->Bind();

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(4 * sizeof(float)));
    }
    quadVBO->BufferData(quadVertices, sizeof(quadVertices));

    glBindVertexArray(quadVAO->id);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
}

void ingame_render(GameData& game_data, int selectedMap, bool rerender) {
    static float time = 0;

#if DEBUG
    auto start = std::chrono::high_resolution_clock::now();
    auto start1 = start;
    // auto end = start;
    auto logTime = [&](const char* name) {
        auto end = std::chrono::high_resolution_clock::now();
        ImGui::DebugLog("%s: %fms\n", name, (end - start).count() / 1000000.0f);
        start = end;
    };
#else
    auto logTime = [&](const char* name) {};
#endif

    auto& map = game_data.maps[selectedMap];
    auto size = glm::ivec2(map.size.x, map.size.y) * room_size * 8;

    glViewport(0, 0, size.x, size.y);

    auto& rd = *render_data;
    auto& shaders = rd.shaders;

    rd.light_buffer.resize(size.x, size.y);
    rd.temp_buffer.resize(size.x, size.y);
    rd.visibility_buffer.resize(size.x, size.y);

    rd.fg_buffer.resize(size.x, size.y);
    rd.mg_buffer.resize(size.x, size.y);
    rd.bg_buffer.resize(size.x, size.y);
    rd.bg_tex_buffer.resize(size.x, size.y);

    glm::mat4 MVP = glm::ortho<float>(0, size.x, 0, size.y, 0.0f, 100.0f) *
                    glm::lookAt(
                        glm::vec3(map.offset * room_size * 8, 3),
                        glm::vec3(map.offset * room_size * 8, 0),
                        glm::vec3(0, 1, 0));

    shaders.textured.Use();
    shaders.textured.setMat4("MVP", MVP);
    shaders.textured.setVec4("color", glm::vec4(1));

    shaders.flat.Use();
    shaders.flat.setMat4("MVP", MVP);

    { // foreground
        rd.fg_buffer.Bind();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        if(rd.show_fg) {
            shaders.textured.Use();
            rd.textures.atlas.Bind();
            rd.fg_tiles.Draw();
        }
        logTime("foreground");
    }

    { // midground
        rd.mg_buffer.Bind();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        shaders.textured.Use();
        rd.textures.atlas.Bind();
        rd.mg_tiles.Draw();
        logTime("midground");
    }

    { // background
        rd.bg_buffer.Bind();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        if(rd.show_bg) {
            shaders.textured.Use();
            rd.textures.atlas.Bind();
            rd.bg_tiles.Draw();
        }
        logTime("background");
    }

    { // visibility
        rd.visibility_buffer.Bind();
        glClearColor(1, 1, 1, 1);
        glClear(GL_COLOR_BUFFER_BIT);

        { // visibility depth
            shaders.flat.Use();
            rd.visibility.Draw();
        }
        logTime("visibility raw");

        { // layer visibility
            auto& vis = shaders.visibility;
            vis.Use();

            vis.setInt("foreground", 0);
            vis.setInt("midGround", 1);
            vis.setInt("backgroundTiles", 2);

            glActiveTexture(GL_TEXTURE0);
            rd.fg_buffer.tex.Bind();
            glActiveTexture(GL_TEXTURE1);
            rd.mg_buffer.tex.Bind();
            glActiveTexture(GL_TEXTURE2);
            rd.bg_buffer.tex.Bind();

            glDisable(GL_BLEND);
            glColorMask(0, 1, 1, 1);
            RenderQuad();
            glColorMask(1, 1, 1, 1);
            glEnable(GL_BLEND);

            glActiveTexture(GL_TEXTURE0);
        }
        logTime("visibility merge");

        { // blur visibilty
            glDisable(GL_BLEND);
            shaders.blur.Use();

            rd.temp_buffer.Bind();
            shaders.blur.setBool("horizontal", true);
            rd.visibility_buffer.tex.Bind();
            RenderQuad();

            rd.visibility_buffer.Bind();
            shaders.blur.setBool("horizontal", false);
            rd.temp_buffer.tex.Bind();
            RenderQuad();

            glEnable(GL_BLEND);
        }

        logTime("visibilty blur");
    }

    if(rerender) { // lights
        renderLights(map, game_data.uvs);
        logTime("lights segmented");

        { // make lights fuzzy
            rd.light_buffer.Bind();
            glClearColor(0, 0, 0, 0);
            glClear(GL_COLOR_BUFFER_BIT);

            shaders.light.Use();
            shaders.light.setVec2("viewportSize", size);
            rd.temp_buffer.tex.Bind();
            RenderQuad();
        }
        logTime("lights fuzz");
    }

    { // background textures
        rd.bg_tex_buffer.Bind();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        shaders.textured.Use();
        rd.textures.background.Bind();
        rd.bg_text.Draw();
        logTime("background textures");
    }

    rd.temp_buffer.Bind();
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    for(auto&& room : map.rooms) {
        auto ind = room.lighting_index;
        if(ind < 0 || ind >= game_data.ambient.size()) ind = 0;
        auto& light = game_data.ambient[ind];

        glm::ivec2 pos = (glm::ivec2(room.x, room.y) - map.offset) * room_size * 8;
        glViewport(pos.x, pos.y, room_size.x * 8, room_size.y * 8);

        glm::vec2 uv_min = glm::vec2(pos) / glm::vec2(size);
        glm::vec2 uv_max = glm::vec2(pos + room_size * 8) / glm::vec2(size);

        { // background tiles
            auto& shader = shaders.merge_bg;
            shader.Use();

            shader.setVec4("ambientLightColor", glm::vec4(light.ambient_light_color) / 255.0f);
            shader.setVec4("fgAmbientLightColor", glm::vec4(light.fg_ambient_light_color) / 255.0f);
            shader.setVec4("bgAmbientLightColor", glm::vec4(light.bg_ambient_light_color) / 255.0f);
            shader.setVec4("fogColor", glm::vec4(light.fog_color) / 255.0f);

            shader.setFloat("midToneBrightness", 0.40);
            shader.setFloat("shadowBrightness", 0.50);
            shader.setFloat("time", time);

            shader.setVec2("viewportSize", size);

            shader.setInt("tex", 0);
            // ms.setInt("lightMask", 1);
            shader.setInt("visibility", 2);
            // ms.setInt("foregroundLight", 3);
            // ms.setInt("backgroundNormals", 4);
            shader.setInt("seperatedLights", 5);

            glActiveTexture(GL_TEXTURE0);
            rd.bg_buffer.tex.Bind();
            glActiveTexture(GL_TEXTURE2);
            rd.visibility_buffer.tex.Bind();
            glActiveTexture(GL_TEXTURE5);
            rd.light_buffer.tex.Bind();

            RenderQuad({-1, -1}, {1, 1}, uv_min, uv_max);

            glActiveTexture(GL_TEXTURE0);
        }

        { // background texture
            rd.temp_buffer.Bind();

            auto& shader = shaders.merge_bg_tex;
            shader.Use();
            shader.setVec4("ambientLightColor", glm::vec4(light.ambient_light_color) / 255.0f);
            shader.setVec4("fogColor", glm::vec4(light.fog_color) / 255.0f);

            shader.setVec2("viewportSize", size);
            shader.setFloat("time", time);

            shader.setInt("tex", 0);
             // ms.setInt("lightMask", 1);
            shader.setInt("visibility", 2);
             // ms.setInt("foregroundLight", 3);
             // ms.setInt("backgroundNormals", 4);
            shader.setInt("seperatedLights", 5);

            glActiveTexture(GL_TEXTURE0);
            rd.bg_tex_buffer.tex.Bind();
            glActiveTexture(GL_TEXTURE2);
            rd.visibility_buffer.tex.Bind();
            glActiveTexture(GL_TEXTURE5);
            rd.light_buffer.tex.Bind();

            glBlendFuncSeparate(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA, GL_ONE, GL_ONE);
            glBlendEquationSeparate(GL_FUNC_ADD, GL_MAX);
            RenderQuad({-1, -1}, {1, 1}, uv_min, uv_max);
            glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
            glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            glActiveTexture(GL_TEXTURE0);
        }

        { // foreground tiles
            auto& shader = shaders.merge_fg;
            shader.Use();
            shader.setInt("foreground", 0);
            shader.setInt("lightMask", 1);
            shader.setInt("foregroundLight", 2);
            shader.setInt("visibility", 3);

            shader.setFloat("rimLightBrightness", 1);
            shader.setVec4("ambientLightColor", glm::vec4(light.ambient_light_color) / 255.0f);
            shader.setVec4("fgAmbientLightColor", glm::vec4(light.fg_ambient_light_color) / 255.0f);

            glActiveTexture(GL_TEXTURE0);
            rd.fg_buffer.tex.Bind();
            glActiveTexture(GL_TEXTURE1);
            rd.light_buffer.tex.Bind();
            glActiveTexture(GL_TEXTURE2);
            // N/A
            glActiveTexture(GL_TEXTURE3);
            rd.visibility_buffer.tex.Bind();

            RenderQuad({-1, -1}, {1, 1}, uv_min, uv_max);

            glActiveTexture(GL_TEXTURE0);
        }
    }

    logTime("rooms");

    glViewport(0, 0, size.x, size.y);

    { // visibility mask
        auto& shader = shaders.visibility_mask;
        shader.Use();
        shader.setVec2("viewportSize", size);
        shader.setFloat("shadowBrightness", 0.50);
        shader.setFloat("time", time);

        rd.visibility_buffer.tex.Bind();

        glBlendFuncSeparate(GL_DST_COLOR, GL_ZERO, GL_ONE, GL_ONE);
        RenderQuad();
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        logTime("visbility mask");
    }

    { // water reflection
        { // copy output image
            rd.bg_buffer.Bind();
            shaders.textured.Use();
            rd.temp_buffer.tex.Bind();
            RenderQuad(map.offset * room_size * 8, (map.offset + map.size) * room_size * 8);
        }
        logTime("water copy");

        { // render water on top of output
            rd.water.clear();
            for(auto&& room : map.rooms) {
                if(room.waterLevel < room_size.y * 8) {
                    auto pos = glm::ivec2(room.x, room.y) * room_size * 8;
                    auto h = (room_size.y * 8 - room.waterLevel) / (float)size.y;
                    rd.water.AddRectFilled({pos.x, pos.y + room.waterLevel}, pos + room_size * 8, {0, 0}, {1, h});

                    // auto pos = (glm::ivec2(room.x, room.y) - map.offset) * room_size * 8;
                    // rd.water.AddRectFilled(glm::ivec2(pos.x, pos.y + room.waterLevel) / size, pos + room_size * 8, {0, 0}, {1, h});
                }
            }

            rd.temp_buffer.Bind();

            auto& shader = shaders.water;
            shader.Use();
            shader.setVec2("viewportSize", size);
            shader.setFloat("time", time);
            shader.setMat4("MVP", MVP);

            rd.bg_buffer.tex.Bind();

            rd.water.Buffer();
            rd.water.Draw();
        }
        logTime("water");
    }

    glViewport(0, 0, size.x, size.y);

    { // bloom
        auto buf1 = &rd.fg_buffer;
        auto buf2 = &rd.bg_buffer;

        { // darken image to highlight bright areas
            buf1->Bind();

            auto& shader = shaders.bloom_darken;
            shader.Use();
            rd.temp_buffer.tex.Bind();
            RenderQuad();
        }

        glDisable(GL_BLEND);
        shaders.blur.Use();
        shaders.blur.setBool("horizontal", true);

        for(int i = 0; i < 6; ++i) {
            buf2->Bind();
            buf1->tex.Bind();
            RenderQuad();

            std::swap(buf1, buf2);
        }

        shaders.blur.setBool("horizontal", false);
        for(int i = 0; i < 5; ++i) {
            buf2->Bind();
            buf1->tex.Bind();
            RenderQuad();

            std::swap(buf1, buf2);
        }
        glEnable(GL_BLEND);

        // last blur step directly to result texture
        rd.temp_buffer.Bind();
        buf1->tex.Bind();
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE, GL_ONE, GL_ONE);
        RenderQuad();
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        logTime("bloom");
    }

    { // color correction
        rd.fg_buffer.Bind();
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        auto& shader = shaders.color_correction;
        shader.Use();
        rd.temp_buffer.tex.Bind();

        for(auto& room : map.rooms) {
            glm::ivec2 pos = (glm::ivec2(room.x, room.y) - map.offset) * room_size * 8;
            glViewport(pos.x, pos.y, room_size.x * 8, room_size.y * 8);

            glm::vec2 uv_min = glm::vec2(pos) / glm::vec2(size);
            glm::vec2 uv_max = glm::vec2(pos + room_size * 8) / glm::vec2(size);

            auto ind = room.lighting_index;
            if(ind < 0 || ind >= game_data.ambient.size()) ind = 0;
            auto& light = game_data.ambient[ind];
            shader.setVec4("colorGainSaturation", {light.color_saturation, light.color_gain});

            RenderQuad({-1, -1}, {1, 1}, uv_min, uv_max);
        }

        logTime("color correction");
    }

    // Image img(size.x, size.y);
    // rd.light_buffer.tex.Bind();
    // glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data());
    // img.save_png("C:/Daten/Desktop/test.png");

    // start = start1;
    // logTime("total");

    time += ImGui::GetIO().DeltaTime;

    // restore viewport
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    int width, height;
    glfwGetFramebufferSize(glfwGetCurrentContext(), &width, &height);
    glViewport(0, 0, width, height);
}

void doRender(bool updateGeometry, GameData& game_data, int selectedMap, glm::mat4& MVP, Textured_Framebuffer* frameBuffer) {
    auto& map = game_data.maps[selectedMap];
    auto& rd = *render_data;

    if(updateGeometry) {
        renderMap(map, game_data);
        // logTime("verts map");

        renderBgs(map);
        // logTime("verts bg");

        if(rd.accurate_render)
            render_visibility(map, game_data.uvs);
        // logTime("verts visibilty");
    }

    if(rd.accurate_render)
        ingame_render(game_data, selectedMap, updateGeometry);

    if(frameBuffer) {
        frameBuffer->Bind();
        glViewport(0, 0, frameBuffer->tex.width, frameBuffer->tex.height);
    }

    if(rd.accurate_render) {
        glClearColor(0, 0, 0, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        rd.shaders.textured.Use();
        rd.shaders.textured.setMat4("MVP", MVP);
        rd.shaders.textured.setVec4("color", {1, 1, 1, 1});

        rd.fg_buffer.tex.Bind();

        auto& map = game_data.maps[selectedMap];
        RenderQuad(map.offset * room_size * 8, (map.offset + map.size) * room_size * 8);
    } else {
        glClearColor(0.45f, 0.45f, 0.45f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        rd.shaders.textured.Use();
        rd.shaders.textured.setMat4("MVP", MVP);

        if(rd.show_bg_tex) { // draw background textures
            rd.shaders.textured.setVec4("color", rd.bg_tex_color);
            rd.textures.background.Bind();
            rd.bg_text.Draw();
        }

        if(rd.show_bg) { // draw background tiles
            rd.textures.atlas.Bind();
            rd.shaders.textured.setVec4("color", rd.bg_color);
            rd.bg_tiles.Draw();
        }

        rd.textures.bunny.Bind();
        rd.bunny.Draw();

        rd.textures.time_capsule.Bind();
        rd.time_capsule.Draw();

        if(rd.show_fg) { // draw foreground tiles
            rd.textures.atlas.Bind();
            rd.shaders.textured.setVec4("color", rd.fg_color);
            rd.fg_tiles.Draw();
        }
    }

    // draw overlay (selection, water level)
    rd.shaders.flat.Use();
    rd.shaders.flat.setMat4("MVP", MVP);
    rd.overlay.Buffer();
    rd.overlay.Draw();
}
