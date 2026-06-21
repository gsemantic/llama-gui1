#include "../include/ui/main_window.h"
#include "../include/core/version.h"
#include <iostream>

#ifdef USE_SDL2
#include <SDL.h>
#include "../external/imgui/backends/imgui_impl_sdl2.h"
#endif

#include "../external/imgui/imgui.h"

namespace llama_gui {
namespace ui {

void MainWindow::set_title(const std::string& title) {
    title_ = title;
    std::cout << "MainWindow: Title set to \"" << title_ << "\"" << std::endl;
}

void MainWindow::set_size(int width, int height) {
    width_ = width;
    height_ = height;
}

void MainWindow::set_position(int x, int y) {
    // Stub implementation
}

void MainWindow::on_conversation_changed(const llama_gui::core::StateEvent& event) {
    // Handle different event types with smart redraw
    if (event.type == llama_gui::core::StateEventType::MessageAdded) {
        // Mark chat as dirty for smart redraw
        ui_dirty_chat_ = true;
        ui_dirty_conversations_ = true;
    } else if (event.type == llama_gui::core::StateEventType::ConversationCreated) {
        // Mark conversations as dirty for smart redraw
        ui_dirty_conversations_ = true;
        ui_dirty_chat_ = true;
    } else if (event.type == llama_gui::core::StateEventType::ConversationChanged) {
        // Mark both as dirty when conversation changes
        ui_dirty_conversations_ = true;
        ui_dirty_chat_ = true;
    }
}

void MainWindow::on_server_state_changed() {
    // Server state changed handler
}

void MainWindow::on_settings_changed() {
    // Settings changed handler
    // Reinitialize RAG manager if embedding model path changed
    std::string embedding_model_path = settings_.get_embedding_model_path();
    if (!embedding_model_path.empty() && rag_manager_) {
        // Reinitialize RagManager with new embedding model path
        rag_manager_ = std::make_unique<llama_gui::core::RagManager>(embedding_model_path);
        if (rag_manager_) {
            rag_manager_->initialize_indexes();
            chat_interface_->set_rag_manager(rag_manager_.get());
            
            if (rag_interface_) {
                rag_interface_->set_rag_manager(rag_manager_.get());
            }
            std::cout << "RagManager reinitialized with new embedding model: " << embedding_model_path << std::endl;
        }
    }
}

void MainWindow::on_state_changed(const std::string& key) {
    // State changed handler
}

void MainWindow::on_first_frame() {
    first_frame_ = false;
}

void MainWindow::show_error(const std::string& title, const std::string& message) {
    std::cerr << "ERROR: " << TR("general.error") << ": " << title << " - " << message << std::endl;
}

void MainWindow::show_warning(const std::string& title, const std::string& message) {
    std::cout << "WARNING: " << TR("general.warning") << ": " << title << " - " << message << std::endl;
}

void MainWindow::show_info(const std::string& title, const std::string& message) {
    std::cout << "INFO: " << title << " - " << message << std::endl;
}

void MainWindow::show_about_dialog() {
    if (!show_about_) return;
    
    ImGui::OpenPopup(TR("menu.help.about.app"));
    
    if (ImGui::BeginPopupModal(TR("menu.help.about.app"), &show_about_, ImGuiWindowFlags_AlwaysAutoResize)) {
        // Заголовок с названием приложения
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "Llama.cpp C++ GUI");
        ImGui::Separator();
        ImGui::Spacing();
        
        // Версия
        ImGui::Text(TR("about.version"), llama_gui::core::getVersionFull());
        ImGui::Text(TR("about.build_date"), llama_gui::core::getBuildDate(), llama_gui::core::getBuildTime());
        ImGui::Text(TR("about.git_commit"), llama_gui::core::getGitCommitHash());
        ImGui::Text(TR("about.build_year"), llama_gui::core::getBuildYear());
        ImGui::Spacing();
        
        // Информация о сборке
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Build: %s", BUILD_TYPE);
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "Compiler: %s", COMPILER_VERSION);
        ImGui::Spacing();
        
        // Описание
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextWrapped(TR("about.description"));
        ImGui::Spacing();
        
        // Копирайт
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), TR("about.copyright"));
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), TR("about.license"));
        ImGui::Spacing();
        
        // Кнопка закрытия
        if (ImGui::Button(TR("close"), ImVec2(120, 0))) {
            show_about_ = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::SetItemDefaultFocus();
        ImGui::EndPopup();
    }
}

void MainWindow::show_help_dialog() {
    std::cout << "MainWindow: Help dialog (stub)" << std::endl;
}

void MainWindow::show_keyboard_shortcuts() {
    std::cout << "MainWindow: Keyboard shortcuts (stub)" << std::endl;
}

#ifdef USE_SDL2
void MainWindow::handle_sdl_events() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        // Only process ImGui events if SDL2 backend was initialized
        if (sdl_window_ && gl_context_) {
            ImGui_ImplSDL2_ProcessEvent(&event);
        }
        if (event.type == SDL_QUIT) {
            is_running_ = false;
        }
        if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE &&
            event.window.windowID == SDL_GetWindowID(sdl_window_)) {
            is_running_ = false;
        }
        if (event.type == SDL_KEYDOWN) {
            if (event.key.keysym.sym == SDLK_ESCAPE) {
                is_running_ = false;
            }
        }
    }
}
#endif

void MainWindow::handle_keyboard_shortcuts() {
#ifdef USE_IMGUI
    ImGuiIO& io = ImGui::GetIO();

    // Use the command manager to handle keyboard shortcuts
    if (command_manager_.isShortcutRegistered("Ctrl+N")) {
        if (ImGui::IsKeyPressed(ImGuiKey_N) && io.KeyCtrl) {
            command_manager_.executeShortcut("Ctrl+N");
        }
    }

    if (command_manager_.isShortcutRegistered("Ctrl+O")) {
        if (ImGui::IsKeyPressed(ImGuiKey_O) && io.KeyCtrl) {
            command_manager_.executeShortcut("Ctrl+O");
        }
    }

    if (command_manager_.isShortcutRegistered("Ctrl+S")) {
        if (ImGui::IsKeyPressed(ImGuiKey_S) && io.KeyCtrl) {
            command_manager_.executeShortcut("Ctrl+S");
        }
    }

    if (command_manager_.isShortcutRegistered("Ctrl+,")) {
        if (ImGui::IsKeyPressed(ImGuiKey_Comma) && io.KeyCtrl) {
            command_manager_.executeShortcut("Ctrl+,");
        }
    }

    if (command_manager_.isShortcutRegistered("Alt+F4")) {
        if (ImGui::IsKeyPressed(ImGuiKey_F4) && io.KeyAlt) {
            command_manager_.executeShortcut("Alt+F4");
        }
    }

    // Additional shortcuts for window toggles
    if (ImGui::IsKeyPressed(ImGuiKey_1) && io.KeyCtrl) {
        window_manager_.toggleWindow("conversations");
    }

    if (ImGui::IsKeyPressed(ImGuiKey_2) && io.KeyCtrl) {
        window_manager_.toggleWindow("files");
    }

    if (ImGui::IsKeyPressed(ImGuiKey_3) && io.KeyCtrl) {
        window_manager_.toggleWindow("chat");
    }

    // Escape key to close popups/modals
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
        // Close any open popups
        ImGui::CloseCurrentPopup();
    }

#endif
}

void MainWindow::load_selected_model(const std::string& model_path) {
    if (model_manager_->load_model(model_path)) {
        // Also update the settings with the selected model path
        settings_.set_model_path(model_path);
        show_info("Model Loaded", "Successfully loaded model: " + model_path);
    } else {
        show_error("Model Load Error", "Failed to load model: " + model_path);
    }
}

void MainWindow::unload_current_model() {
    if (model_manager_->unload_model()) {
        show_info("Model Unloaded", "Successfully unloaded current model");
    } else {
        show_error("Model Unload Error", "Failed to unload current model");
    }
}

void MainWindow::open_file(const std::string& file_path) {
    file_manager_->open_file(file_path);
}

void MainWindow::save_conversation(const std::string& conversation_id, const std::string& file_path) {
    std::cout << "MainWindow: Saving conversation " << conversation_id << " to " << file_path << std::endl;
}

void MainWindow::export_conversations(const std::string& file_path) {
    // Use the available save_to_file method instead
    state_manager_.save_to_file(file_path);
}

void MainWindow::save_workspace(const std::string& name) {
    std::cout << "MainWindow: Saving workspace " << name << std::endl;
    advanced_menu_system_.saveWorkspace(name);
    show_info("Workspace Saved", "Successfully saved workspace: " + name);
}

void MainWindow::load_workspace(const std::string& name) {
    std::cout << "MainWindow: Loading workspace " << name << std::endl;
    advanced_menu_system_.loadWorkspace(name);
    workspace_just_loaded_ = true;  // Устанавливаем флаг для применения позиций
    show_info("Workspace Loaded", "Successfully loaded workspace: " + name);
}

void MainWindow::reset_workspace() {
    std::cout << "MainWindow: Resetting workspace to default" << std::endl;

    // Reset window states to default
    window_manager_.setWindowVisible("conversations", true);
    window_manager_.setWindowVisible("files", true);
    window_manager_.setWindowVisible("chat", true);

    show_info("Workspace Reset", "Workspace has been reset to default layout");
}

void MainWindow::show_workspace_save_dialog() {
    show_workspace_save_dialog_ = true;
    workspace_save_name_ = "default";
}

void MainWindow::show_workspace_load_dialog() {
    show_workspace_load_dialog_ = true;
    workspace_load_name_ = "default";
}

void MainWindow::render_workspace_save_dialog() {
    if (show_workspace_save_dialog_) {
        ImGui::OpenPopup("Save Workspace");
        show_workspace_save_dialog_ = false;
    }

    if (ImGui::BeginPopupModal("Save Workspace", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char buf[256] = "default";

        ImGui::Text("Enter workspace name:");
        ImGui::Separator();

        if (ImGui::InputText("##Name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string name = buf;
            if (!name.empty()) {
                save_workspace(name);
            }
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            std::string name = buf;
            if (!name.empty()) {
                save_workspace(name);
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void MainWindow::render_workspace_load_dialog() {
    if (show_workspace_load_dialog_) {
        ImGui::OpenPopup("Load Workspace");
        show_workspace_load_dialog_ = false;
    }

    if (ImGui::BeginPopupModal("Load Workspace", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char buf[256] = "default";

        ImGui::Text("Enter workspace name:");
        ImGui::Separator();

        // Список доступных workspace
        auto workspaces = get_workspace_list();
        if (!workspaces.empty()) {
            ImGui::Text("Available workspaces:");
            ImGui::BeginChild("WorkspaceList", ImVec2(300, 150), true);
            for (const auto& ws : workspaces) {
                if (ImGui::Selectable(ws.c_str(), false)) {
                    strncpy(buf, ws.c_str(), sizeof(buf) - 1);
                    buf[sizeof(buf) - 1] = '\0';
                }
            }
            ImGui::EndChild();
            ImGui::Separator();
        }

        if (ImGui::InputText("##Name", buf, sizeof(buf), ImGuiInputTextFlags_EnterReturnsTrue)) {
            std::string name = buf;
            if (!name.empty()) {
                load_workspace(name);
            }
            ImGui::CloseCurrentPopup();
        }

        if (ImGui::Button("Load", ImVec2(120, 0))) {
            std::string name = buf;
            if (!name.empty()) {
                load_workspace(name);
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Delete", ImVec2(120, 0))) {
            std::string name = buf;
            if (!name.empty()) {
                delete_workspace(name);
                // Обновляем список workspace после удаления
                workspace_list_cache_.clear();
                workspace_list_cache_time_ = 0;
                // Очищаем поле ввода
                buf[0] = '\0';
            }
            // Не закрываем диалог, чтобы можно было удалить ещё один workspace
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void MainWindow::delete_workspace(const std::string& name) {
    std::cout << "MainWindow: Deleting workspace " << name << std::endl;
    advanced_menu_system_.deleteWorkspace(name);
    show_info("Workspace Deleted", "Workspace '" + name + "' has been deleted");
}

std::vector<std::string> MainWindow::get_workspace_list() const {
    // Получаем текущее время для проверки кэша
#ifdef USE_SDL2
    uint32_t current_time = SDL_GetTicks();
#else
    // Fallback если SDL2 не доступен
    uint32_t current_time = 0;
#endif

    // Проверяем валидность кэша
    if (!workspace_list_cache_.empty() && workspace_list_cache_time_ != 0) {
        uint32_t cache_age = current_time - workspace_list_cache_time_;
        if (cache_age < WORKSPACE_CACHE_DURATION_MS) {
            return workspace_list_cache_;
        }
    }

    // Обновляем кэш
    workspace_list_cache_ = advanced_menu_system_.getAvailableWorkspaceFiles();
    workspace_list_cache_time_ = current_time;
    return workspace_list_cache_;
}

void MainWindow::process_pending_dialog_results() {
    // Optimized dialog processing - only process if there are pending results
    if (pending_dialog_results_size_.load() == 0) {
        return; // No pending results, skip processing
    }

    std::lock_guard<std::mutex> lock(pending_dialog_results_mutex_);

    while (!pending_dialog_results_.empty()) {
        auto result = pending_dialog_results_.front();
        pending_dialog_results_.pop();
        pending_dialog_results_size_--; // Decrement atomic counter

        std::string type = result.first;
        std::string value = result.second;

        if (type == "models_directory") {
            // Update model directory
            model_manager_->set_models_directory(value);
            model_manager_->refresh_model_list();

            // Also update settings to persist the directory choice
            settings_.set_custom_setting("models_directory", value);
            show_info("Model Directory", "Successfully set model directory to: " + value);
        } else if (type == "message") {
            // Show message to user
            show_info("Model Directory", value);
        } else if (type == "model_file") {
            // Handle model file selection
            handle_model_file_selection(value);
        }
    }
}

void MainWindow::handle_model_file_selection(const std::string& model_path) {
    std::cout << "MainWindow: Handling model file selection: " << model_path << std::endl;

    // Check if a different model is already loaded
    std::string current_model = model_manager_->get_current_model();
    if (!current_model.empty() && current_model != model_path) {
        // Show confirmation dialog
        show_model_reload_confirmation(model_path);
    } else {
        // Load the model directly
        load_model_and_restart_server(model_path);
    }
}

void MainWindow::show_model_reload_confirmation(const std::string& new_model_path) {
    std::cout << "MainWindow: Showing model reload confirmation" << std::endl;

    // Store the pending model path
    pending_model_path_ = new_model_path;

    // Show confirmation dialog
    ImGui::OpenPopup("Confirm Model Reload");
}

void MainWindow::load_model_and_restart_server(const std::string& model_path) {
    std::cout << "MainWindow: Loading model and restarting server with progress dialog: " << model_path << std::endl;

    // Используем диалог загрузки с прогрессом (at_startup = false, нет pending query)
    load_model_with_progress_dialog(model_path, "", false);
}

} // namespace ui
} // namespace llama_gui
