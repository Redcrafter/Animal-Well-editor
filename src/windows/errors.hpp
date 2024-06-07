#pragma once

#include <string>
#include <vector>

#include <imgui.h>

class {
    std::vector<std::string> errors;

  public:
    void draw() {
        if(!errors.empty()) {
            ImGui::OpenPopup("Error");
        }

        if(ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            if(errors.size() == 1) {
                ImGui::Text("Error: %s", errors[0].c_str());
            } else {
                ImGui::Text("Errors:");
                for(auto& e : errors) {
                    ImGui::Indent();
                    ImGui::Text("%s", e.c_str());
                    ImGui::Unindent();
                }
            }

            if(ImGui::Button("Ok")) {
                errors.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void push(const std::string& error) {
        errors.push_back(error);
    }
    void clear() {
        errors.clear();
    }
} ErrorDialog;
