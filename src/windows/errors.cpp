#include "errors.hpp"

#include <imgui.h>

void ErrorDialog::drawPopup() {
    if(!errors.empty()) {
        ImGui::OpenPopup("Error");
    }

    if(ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        for(auto& e : errors) {
            ImGui::Text(e.error ? "[Error]" : "[Warning]");
            ImGui::SameLine();
            ImGui::Text("%s", e.text.c_str());
        }

        if(ImGui::Button("Ok")) {
            errors.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
