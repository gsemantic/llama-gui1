#include "../include/ui/main_window.h"
#include "../include/ui/localization_manager.h"
#include "../include/ui/file_dialog_helper.h"
#include <iostream>
#include <fstream>

#include "../external/imgui/imgui.h"

namespace llama_gui {
namespace ui {

// === Conversation File Operations ===

void MainWindow::open_conversation_file() {
    std::cout << "MainWindow: Opening conversation file..." << std::endl;
    show_open_conversation_dialog_ = true;
}

void MainWindow::save_current_conversation() {
    if (current_conversation_path_.empty()) {
        save_current_conversation_as("conversation.json");
        return;
    }

    if (state_manager_.save_to_file(current_conversation_path_)) {
        conversation_modified_ = false;
        show_info("File Saved", "Conversation saved to: " + current_conversation_path_);
    } else {
        show_error("Save Error", "Failed to save conversation to: " + current_conversation_path_);
    }
}

void MainWindow::save_current_conversation_as(const std::string& file_path) {
    std::string target_path = file_path;

    // If no extension, add .json
    if (target_path.find('.') == std::string::npos) {
        target_path += ".json";
    }

    if (state_manager_.save_to_file(target_path)) {
        current_conversation_path_ = target_path;
        conversation_modified_ = false;
        show_info("File Saved", "Conversation saved to: " + current_conversation_path_);
    } else {
        show_error("Save Error", "Failed to save conversation to: " + target_path);
    }
}

// === Conversation File Dialogs ===

bool MainWindow::open_conversation_file_dialog(std::string& selected_path) {
    // Используем FileDialogHelper с фильтром для JSON файлов
    FileDialogHelper helper;
    return helper.open_file_dialog_sync("Open Conversation", selected_path, "json_files");
}

bool MainWindow::save_conversation_file_dialog(std::string& selected_path) {
    // Используем FileDialogHelper с фильтром для JSON файлов
    FileDialogHelper helper;
    return helper.save_file_dialog_sync("Save Conversation", "conversation.json", selected_path, "json_files");
}

// Удалены дублирующие методы:
// - try_zenity_conversation_file_dialog()
// - try_kdialog_conversation_file_dialog()
// - try_python_conversation_file_dialog()
// Теперь используется FileDialogHelper напрямую

// === Conversation File Dialogs Rendering ===

void MainWindow::render_open_conversation_dialog() {
    if (!show_open_conversation_dialog_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(TR("file.open"), &show_open_conversation_dialog_, ImGuiWindowFlags_NoCollapse)) {

        ImGui::Text(TR("file.select_to_open"));
        ImGui::Separator();

        // Try to open native file dialog
        std::string selected_path;
        if (ImGui::Button(TR("file.browse_files"))) {
            if (open_conversation_file_dialog(selected_path)) {
                if (!selected_path.empty()) {
                    // Load the conversation
                    if (state_manager_.load_from_file(selected_path)) {
                        current_conversation_path_ = selected_path;
                        conversation_modified_ = false;
                        show_info("Conversation Loaded", "Successfully loaded: " + selected_path);
                        show_open_conversation_dialog_ = false;
                    } else {
                        show_error("Load Error", "Failed to load conversation from: " + selected_path);
                    }
                }
            }
        }

        ImGui::SameLine();

        if (ImGui::Button(TR("file.cancel"))) {
            show_open_conversation_dialog_ = false;
        }

        ImGui::Separator();

        // Manual path input
        static char path_buffer[512] = "";
        ImGui::Text(TR("file.enter_path"));
        ImGui::InputText("##path", path_buffer, sizeof(path_buffer));

        ImGui::SameLine();
        if (ImGui::Button(TR("file.load"))) {
            std::string manual_path(path_buffer);
            if (!manual_path.empty()) {
                if (state_manager_.load_from_file(manual_path)) {
                    current_conversation_path_ = manual_path;
                    conversation_modified_ = false;
                    show_info("Conversation Loaded", "Successfully loaded: " + manual_path);
                    show_open_conversation_dialog_ = false;
                } else {
                    show_error("Load Error", "Failed to load conversation from: " + manual_path);
                }
            }
        }

        ImGui::End();
    }
}

void MainWindow::render_save_conversation_dialog() {
    if (!show_save_conversation_dialog_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(500, 300), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(TR("menu.file.save_as"), &show_save_conversation_dialog_, ImGuiWindowFlags_NoCollapse)) {

        ImGui::Text(TR("file.enter_filename"));
        ImGui::Separator();

        // Try to open native save file dialog
        if (ImGui::Button(TR("file.browse"))) {
            std::string selected_path;
            if (save_conversation_file_dialog(selected_path)) {
                if (!selected_path.empty()) {
                    // Save the conversation
                    if (state_manager_.save_to_file(selected_path)) {
                        current_conversation_path_ = selected_path;
                        conversation_modified_ = false;
                        show_info("Conversation Saved", "Successfully saved: " + selected_path);
                        show_save_conversation_dialog_ = false;
                    } else {
                        show_error("Save Error", "Failed to save conversation to: " + selected_path);
                    }
                }
            }
        }

        ImGui::SameLine();

        if (ImGui::Button(TR("file.cancel"))) {
            show_save_conversation_dialog_ = false;
        }

        ImGui::Separator();

        // Manual path input
        static char path_buffer[512] = "conversation.json";
        ImGui::Text(TR("file.enter_path"));
        ImGui::InputText("##save_path", path_buffer, sizeof(path_buffer));

        ImGui::SameLine();
        if (ImGui::Button(TR("file.save"))) {
            std::string manual_path(path_buffer);
            if (!manual_path.empty()) {
                // Add .json extension if not present
                if (manual_path.find('.') == std::string::npos) {
                    manual_path += ".json";
                }

                if (state_manager_.save_to_file(manual_path)) {
                    current_conversation_path_ = manual_path;
                    conversation_modified_ = false;
                    show_info("Conversation Saved", "Successfully saved: " + manual_path);
                    show_save_conversation_dialog_ = false;
                } else {
                    show_error("Save Error", "Failed to save conversation to: " + manual_path);
                }
            }
        }

        ImGui::End();
    }
}

} // namespace ui
} // namespace llama_gui
