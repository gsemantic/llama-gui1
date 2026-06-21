#include "../include/ui/main_window.h"
#include <iostream>
#include <fstream>

namespace llama_gui {
namespace ui {

void MainWindow::shutdown() {
    if (is_running_) {
        is_running_ = false;
    }

    // Автосохранение текущего профиля при выходе
    std::cout << "MainWindow: Auto-saving current profile on exit..." << std::endl;
    std::string current_profile = config_manager_.getCurrentProfileName();
    if (!current_profile.empty()) {
        if (config_manager_.saveProfile(current_profile)) {
            std::cout << "✓ Profile auto-saved: " << current_profile << std::endl;
        } else {
            std::cerr << "✗ Failed to auto-save profile" << std::endl;
        }
    }

    // Автосохранение состояния окон при выходе
    std::cout << "MainWindow: Saving workspace on exit..." << std::endl;
    advanced_menu_system_.saveWorkspace("default");
    std::cout << "✓ Workspace saved on exit" << std::endl;

    // === RAG: Автосохранение индекса при закрытии ===
    if (rag_manager_) {
        std::cout << "MainWindow: Auto-saving RAG index on exit..." << std::endl;
        if (rag_manager_->save_index()) {
            std::cout << "✓ RAG index auto-saved on exit..." << std::endl;
        } else {
            std::cerr << "✗ Failed to auto-save RAG index" << std::endl;
        }
    }

    destroy_ui_components();
    is_initialized_ = false;
    std::cout << "MainWindow: Shutdown complete" << std::endl;
}

void MainWindow::create_ui_components() {
    // UI components are already created in constructor
}

void MainWindow::destroy_ui_components() {
    chat_interface_.reset();
    file_manager_.reset();
    conversation_manager_.reset();
    settings_dialog_.reset();
}

std::string MainWindow::get_config_directory() const {
    return ".config/llama-gui";
}

std::string MainWindow::get_cache_directory() const {
    return ".cache/llama-gui";
}

bool MainWindow::ensure_directory_exists(const std::string& path) const {
    // Stub implementation - always returns true
    (void)path;
    return true;
}

} // namespace ui
} // namespace llama_gui
