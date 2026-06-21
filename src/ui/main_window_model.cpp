#include "../include/ui/main_window.h"
#include <iostream>

namespace llama_gui {
namespace ui {

void MainWindow::load_model_from_path(const std::string& model_path) {
    if (!model_manager_) {
        std::cerr << "MainWindow: ModelManager not initialized" << std::endl;
        show_error("Model Loading", "ModelManager not initialized");
        return;
    }

    std::cout << "MainWindow: Loading model from: " << model_path << std::endl;

    // Проверяем, существует ли файл
    if (!model_manager_->is_valid_model_file(model_path)) {
        std::cerr << "MainWindow: Invalid model file: " << model_path << std::endl;
        show_error("Model Loading", "Invalid model file: " + model_path);
        return;
    }

    // Используем диалог загрузки с прогрессом
    load_model_with_progress_dialog(model_path, "", false);
}

} // namespace ui
} // namespace llama_gui
