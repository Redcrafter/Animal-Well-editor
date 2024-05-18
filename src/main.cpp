#include <stdio.h>

#include "glStuff.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>  // Will drag system OpenGL headers

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>

#include "parsing.hpp"

static void glfw_error_callback(int error, const char* description) {
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

float gScale = 1;
// Camera matrix
glm::mat4 view = glm::lookAt(
	glm::vec3(0, 0, 3), // Camera is at (0, 0, 3), in World Space
	glm::vec3(0, 0, 0), // and looks at the origin
	glm::vec3(0, 1, 0)  // Head is up (set to 0,-1,0 to look upside-down)
);
glm::mat4 projection;
glm::vec2 mousePos = glm::vec2(-1);

glm::vec2 screenSize;

static void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
	glm::vec2 newPos{ xpos, ypos };
	if(mousePos == newPos)
		return;

	if(mousePos == glm::vec2(-1)) {
		mousePos = newPos;
		return;
	}
	auto delta = mousePos - newPos;

	if(glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)) {
		view = glm::translate(view, glm::vec3(-delta / gScale, 0));
	}

	mousePos = newPos;
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
	float scale = 1;
	if(yoffset > 0) {
		scale = 1.1;
	} else if(yoffset < 0) {
		scale = 1 / 1.1;
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
	// projection = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 100.0f); // In world coordinates
}

static bool isSolid(uint16_t id) {
	return id == 81 || 
		id == 356 || id == 314 || id == 299 || id == 140 || id == 180 || id == 308 || id == 546 || id == 198 || id == 676 || id == 215 || id == 1 || id == 173;
}

static bool doesTile(uint16_t id) {
	return id == 356 || id == 314 || id == 299 || id == 140 || id == 180 || id == 308 || id == 546 || id == 198 || id == 676 || id == 215 || id == 1 || id == 173;
}

// Main code
int main(int, char**) {
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
	GLFWwindow* window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", nullptr, nullptr);
	if(window == nullptr)
		return 1;
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);  // Enable vsync

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

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	(void)io;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable Multi-Viewport / Platform Windows
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

	// Load Fonts
	// - If no fonts are loaded, dear imgui will use the default font. You can also load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
	// - AddFontFromFileTTF() will return the ImFont* so you can store it if you need to select the font among multiple.
	// - If the file cannot be loaded, the function will return a nullptr. Please handle those errors in your application (e.g. use an assertion, or display an error and quit).
	// - The fonts will be rasterized at a given size (w/ oversampling) and stored into a texture when calling ImFontAtlas::Build()/GetTexDataAsXXXX(), which ImGui_ImplXXXX_NewFrame below will call.
	// - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use Freetype for higher quality font rendering.
	// - Read 'docs/FONTS.md' for more instructions and details.
	// - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
	// - Our Emscripten build process allows embedding fonts to be accessible at runtime from the "fonts/" folder. See Makefile.emscripten for details.
	// io.Fonts->AddFontDefault();
	// io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\segoeui.ttf", 18.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/DroidSans.ttf", 16.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Roboto-Medium.ttf", 16.0f);
	// io.Fonts->AddFontFromFileTTF("../../misc/fonts/Cousine-Regular.ttf", 15.0f);
	// ImFont* font = io.Fonts->AddFontFromFileTTF("c:\\Windows\\Fonts\\ArialUni.ttf", 18.0f, nullptr, io.Fonts->GetGlyphRangesJapanese());
	// IM_ASSERT(font != nullptr);

	// Our state
	// bool show_demo_window = true;
	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

	auto gameData = loadGameAssets();

	Texture atlas;
	atlas.Load(gameData[255].data);

	std::vector<glm::vec4> vertecies; // position.xy/uv.zw

	std::map<uint64_t, Room*> room_lookup;


#pragma region map rendering

	std::map<uint32_t, SpriteData> spriteMap;

	for(auto el : spriteMapping) {
		auto& item = gameData[el.asset_id];
		if(item.type != AssetType::SpriteData) {
			printf("wrong format %i\n", el.asset_id);
			continue;
		}

		auto actual_ptr = item.data.data();

		auto& out = spriteMap[el.tile_id];

		out.magic = *(uint16_t*)actual_ptr;
		// assert(magic == 0x0003AC1D);

		// out.tile_id = el.tile_id;
		out.composite_size.x = *(uint16_t*)(actual_ptr + 4);
		out.composite_size.y = *(uint16_t*)(actual_ptr + 6);
		out.layer_count = *(uint16_t*)(actual_ptr + 8);
		out.composition_count = *(uint16_t*)(actual_ptr + 10);

		out.subsprite_count = *(uint8_t*)(actual_ptr + 12);
		out.animation_count = *(uint8_t*)(actual_ptr + 13);

		auto arr3_size = out.subsprite_count * sizeof(SubSprite);
		auto arr1_size = out.animation_count * sizeof(SpriteAnimation);

		actual_ptr += 0x30;

		out.animations = { (SpriteAnimation*)actual_ptr, (SpriteAnimation*)(actual_ptr + arr1_size) };
		actual_ptr += arr1_size;

		out.compositions = { (uint8_t*)actual_ptr, (uint8_t*)(actual_ptr + out.layer_count * out.composition_count) };
		actual_ptr += out.layer_count * out.composition_count;

		out.sub_sprites = { (SubSprite*)actual_ptr, (SubSprite*)(actual_ptr + arr3_size) };
		actual_ptr += arr3_size;

		out.layers = { (SpriteLayer*)actual_ptr, (SpriteLayer*)(actual_ptr + out.layer_count * 3) };

		// __debugbreak();
	}

	std::vector<uv_data> uvs;
	{
		auto item = gameData[254];

		auto ptr = (uv_data*)(item.data.data() + 0xC);
		auto count = (item.data.size() - 0xC) / sizeof(uv_data);

		for(size_t i = 0; i < count; i++) {
			uvs.push_back(ptr[i]);
		}
	}

	auto& map = gameData[300];

	auto head = *(MapHeader*)map.data.data();
	Room* rooms = (Room*)(map.data.data() + 0x10);

	int x_min = 65535, x_max = 0;
	int y_min = 65535, y_max = 0;

	for(size_t i = 0; i < head.roomCount; i++) {
		auto& room = rooms[i];
		x_min = std::min(x_min, (int)room.x);
		x_max = std::max(x_max, (int)room.x);
		y_min = std::min(y_min, (int)room.y);
		y_max = std::max(y_max, (int)room.y);
	}

	int width = x_max - x_min + 1;
	int height = y_max - y_min + 1;

	auto atlasSize = glm::vec2(atlas.width, atlas.height);

	for(size_t i = 0; i < head.roomCount; i++) {
		auto& room = rooms[i];
		int x1 = room.x - x_min;
		int y1 = room.y - y_min;
		room_lookup[x1 | (((uint64_t)y1) << 32)] = &room;
	}

	auto check_tile = [&](uint16_t x, uint16_t y) {
		if(x < 0 || x >= width * 40 || y < 0 || y >= height * 22) return true;

		auto& room = room_lookup[(x / 40) | (((uint64_t)(y / 22)) << 32)];
		auto& tile = room->tiles[0][y % 22][x % 40];

		return isSolid(tile.tile_id);
	};

	for(size_t i = 0; i < head.roomCount; i++) {
		auto& room = rooms[i];

		int x1 = room.x - x_min;
		int y1 = room.y - y_min;

		for(size_t y2 = 0; y2 < 22; y2++) {
			for(size_t x2 = 0; x2 < 40; x2++) {
				auto tile = room.tiles[0][y2][x2];
				// assert((tile & 0xFFFF0000) == 0);
				if(tile.tile_id == 0 || tile.tile_id >= 0x400) continue;

				auto pos = glm::vec2(x2 + x1 * 40, y2 + y1 * 22);
				pos *= 8;
				auto uv = uvs[tile.tile_id];

				if(spriteMap.contains(tile.tile_id)) {
					auto& sprite = spriteMap[tile.tile_id];
					// __debugbreak();

					int composition_id = 0;

					for(int j = 0; j < sprite.layer_count; ++j) {
						auto subsprite_id = sprite.compositions[composition_id * sprite.layer_count + j];
						if(subsprite_id >= sprite.subsprite_count)
							continue;

						auto& layer = sprite.layers[j];
						if(layer.is_normals1 || layer.is_normals2 || !layer.is_visible) continue;

						auto& subsprite = sprite.sub_sprites[subsprite_id];

						auto aUv = uv.pos + subsprite.atlas_pos;
						auto size = subsprite.size;
						auto ap = pos + glm::vec2(subsprite.composite_pos);

						vertecies.emplace_back(ap, glm::vec2(aUv) / atlasSize); // tl
						vertecies.emplace_back(ap + glm::vec2(size.x, 0), glm::vec2(aUv.x + size.x, aUv.y) / atlasSize); // tr
						vertecies.emplace_back(ap + glm::vec2(0, size.y), glm::vec2(aUv.x, aUv.y + size.y) / atlasSize); // bl

						vertecies.emplace_back(ap + glm::vec2(size.x, 0), glm::vec2(aUv.x + size.x, aUv.y) / atlasSize); // tr
						vertecies.emplace_back(ap + glm::vec2(size), glm::vec2(aUv + size) / atlasSize); // br
						vertecies.emplace_back(ap + glm::vec2(0, size.y), glm::vec2(aUv.x, aUv.y + size.y) / atlasSize); // bl
					}
				} else {
					auto uvs = glm::vec2(uv.size);

					if(doesTile(tile.tile_id)) {
						auto l = check_tile(x2 + x1 * 40 - 1, y2 + y1 * 22);
						auto r = check_tile(x2 + x1 * 40 + 1, y2 + y1 * 22);
						auto u = check_tile(x2 + x1 * 40    , y2 + y1 * 22 - 1);
						auto d = check_tile(x2 + x1 * 40    , y2 + y1 * 22 + 1);

						if(l && r && u && d) {
							uv.pos.x += 8;
							uv.pos.y += 8;
						} else if(l && r && d) {
							uv.pos.x += 8;
						} else if(l && u && d) {
							uv.pos.x += 16;
							uv.pos.y += 8;
						} else if(l && u && r) {
							uv.pos.x += 8;
							uv.pos.y += 16;
						} else if(u && d && r) {
							uv.pos.y += 8;
						} else if(l && u) {
							uv.pos.x += 16;
							uv.pos.y += 16;
						} else if(l && d) {
							uv.pos.x += 16;
						} else if(u && r) {
							uv.pos.y += 16;
						} else if(d && r) {
							// default
						} else if(l && r) {
							uv.pos.x += 8;
							uv.pos.y += 24;
						} else if(u && d) {
							uv.pos.x += 24;
							uv.pos.y += 8;
						} else if(l) {
							uv.pos.x += 16;
							uv.pos.y += 24;
						} else if(r) {
							uv.pos.y += 24;
						} else if(u) {
							uv.pos.x += 24;
							uv.pos.y += 16;
						} else if(d) {
							uv.pos.x += 24;
						} else {
							uv.pos.x += 24;
							uv.pos.y += 24;
						}
					} else { // flags ignore for tiling
						if(tile.vertical_mirror) {
							uv.pos.y += uv.size.y;
							uvs.y = -uvs.y;
						}
						if(tile.horizontal_mirror) {
							uv.pos.x += uv.size.x;
							uvs.x = -uvs.x;
						}
					}


					vertecies.emplace_back(pos, glm::vec2(uv.pos) / atlasSize); // tl
					vertecies.emplace_back(pos + glm::vec2(uv.size.x, 0), glm::vec2(uv.pos.x + uvs.x, uv.pos.y) / atlasSize); // tr
					vertecies.emplace_back(pos + glm::vec2(0, uv.size.y), glm::vec2(uv.pos.x, uv.pos.y + uvs.y) / atlasSize); // bl

					vertecies.emplace_back(pos + glm::vec2(uv.size.x, 0), glm::vec2(uv.pos.x + uvs.x, uv.pos.y) / atlasSize); // tr
					vertecies.emplace_back(pos + glm::vec2(uv.size), (glm::vec2(uv.pos) + uvs) / atlasSize); // br
					vertecies.emplace_back(pos + glm::vec2(0, uv.size.y), glm::vec2(uv.pos.x, uv.pos.y + uvs.y) / atlasSize); // bl
				}
			}
		}
	}

#pragma endregion


	ShaderProgram shader("shaders/shader.vs", "shaders/shader.fs");
	shader.Use();

	VAO VAO{};
	VBO VBOv{ GL_ARRAY_BUFFER, GL_STATIC_DRAW };

	VAO.Bind();

	VBOv.Bind();
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
	glEnableVertexAttribArray(0);

	VBOv.BufferData(vertecies.data(), vertecies.size() * sizeof(glm::vec4));

	// Main loop
	while(!glfwWindowShouldClose(window)) {
		// Poll and handle events (inputs, window resize, etc.)
		// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
		// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
		// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
		// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
		glfwPollEvents();

		// Start the Dear ImGui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();

		auto MVP = projection * view;

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		// if(show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
		{
			ImGui::Begin("Tile info");  // Create a window called "Hello, world!" and append into it.

			auto mp = glm::vec4(((mousePos - screenSize / 2.0f) / screenSize) * 2.0f, 0, 1);
			mp.y = -mp.y;
			auto wp = glm::vec2(glm::inverse(MVP) * mp);

			// ImGui::Text("mouse pos %f %f", mousePos.x, mousePos.y);
			ImGui::Text("world pos %f %f", wp.x, wp.y);


			auto room_size = glm::vec2(40 * 8, 22 * 8);
			auto rp = glm::ivec2(wp / room_size);
			auto room = room_lookup[rp.x | (((uint64_t)rp.y) << 32)];

			if(wp.x >= 0 && wp.y >= 0 && room != nullptr) {
				ImGui::NewLine();
				ImGui::Text("room position %i %i", room->x, room->y);
				ImGui::Text("room water level %i", room->waterLevel);
				ImGui::Text("room background id %i", room->bgId);

				auto tp = glm::ivec2(glm::mod(wp, room_size) / 8.0f);
				auto& tile = room->tiles[0][tp.y][tp.x];
				auto uv = uvs[tile.tile_id];

				ImGui::NewLine();
				ImGui::Text("tile");
				ImGui::Text("index %i %i", tp.x, tp.y);
				ImGui::Text("id %i", tile.tile_id);
				ImGui::Text("idk %i", tile.idk1);

				ImGui::BeginDisabled();
				int flags = tile.flags;
				ImGui::CheckboxFlags("horizontal_mirror", &flags, 1);
				ImGui::CheckboxFlags("vertical_mirror", &flags, 2);
				ImGui::CheckboxFlags("rotate_90", &flags, 4);
				ImGui::CheckboxFlags("rotate_180", &flags, 8);

				int uv_flags = uv.flags;
				ImGui::NewLine();
				ImGui::Text("uv flags");

				ImGui::CheckboxFlags("collides_left", &uv_flags, 1 << 0); // correct
				ImGui::CheckboxFlags("collides_right", &uv_flags, 1 << 1); // correct
				ImGui::CheckboxFlags("collides_up", &uv_flags, 1 << 2); // correct
				ImGui::CheckboxFlags("collides_down", &uv_flags, 1 << 3); // correct

				ImGui::CheckboxFlags("not_placeable", &uv_flags, 1 << 4); // seems correct
				ImGui::CheckboxFlags("additive", &uv_flags, 1 << 5);

				ImGui::CheckboxFlags("contiguous", &uv_flags, 1 << 6); // see correct
				ImGui::CheckboxFlags("self_contiguous", &uv_flags, 1 << 7);
				ImGui::CheckboxFlags("blocks_light", &uv_flags, 1 << 8);
				ImGui::CheckboxFlags("obscures", &uv_flags, 1 << 9);
				ImGui::CheckboxFlags("dirt", &uv_flags, 1 << 10);
				ImGui::CheckboxFlags("hidden", &uv_flags, 1 << 11);
				ImGui::CheckboxFlags("has_normals", &uv_flags, 1 << 12); // correct
				ImGui::CheckboxFlags("uv_light", &uv_flags, 1 << 13); // correct

				ImGui::CheckboxFlags("14", &uv_flags, 1 << 14); // probably unused
				ImGui::CheckboxFlags("15", &uv_flags, 1 << 15); // probably unused

				ImGui::EndDisabled();

				if(spriteMap.contains(tile.tile_id)) {
					auto sprite = spriteMap[tile.tile_id];

					ImGui::NewLine();
					ImGui::Text("sprite");
					ImGui::Text("composite_size %i %i", sprite.composite_size.x, sprite.composite_size.y);
					ImGui::Text("layer_count %i", sprite.layer_count);
					ImGui::Text("composition_count %i", sprite.composition_count);
					ImGui::Text("subsprite_count %i", sprite.subsprite_count);
					ImGui::Text("animation_count %i", sprite.animation_count);
				}
			}

			ImGui::End();
		}

		// Rendering
		ImGui::Render();
		// int display_w, display_h;
		// glfwGetFramebufferSize(window, &display_w, &display_h);
		// glViewport(0, 0, display_w, display_h);
		glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
		glClear(GL_COLOR_BUFFER_BIT);

		shader.setMat4("MVP", MVP);

		glActiveTexture(GL_TEXTURE0);
		atlas.Bind();

		VAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, vertecies.size());


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
