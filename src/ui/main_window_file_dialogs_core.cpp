#include "../include/ui/main_window.h"
#include "../include/ui/file_dialog_helper.h"
#include <iostream>

namespace llama_gui {
namespace ui {

void MainWindow::open_model_selection_dialog() {
    std::cout << "MainWindow: Opening model selection dialog" << std::endl;

    std::thread worker_thread([this]() {
        std::string selected_path;

#ifdef USE_SDL2
        // Используем FileDialogHelper с фильтром для файлов моделей
        FileDialogHelper helper;
        if (helper.open_file_dialog_sync("Select Model File", selected_path, "model_files")) {
            if (!selected_path.empty()) {
                std::lock_guard<std::mutex> lock(pending_dialog_results_mutex_);
                pending_dialog_results_.push({"model_file", selected_path});
                pending_dialog_results_size_++;
            }
        } else {
            std::lock_guard<std::mutex> lock(pending_dialog_results_mutex_);
            pending_dialog_results_.push({"message", "Please select a model file manually"});
            pending_dialog_results_size_++;
        }
#else
        std::lock_guard<std::mutex> lock(pending_dialog_results_mutex_);
        pending_dialog_results_.push({"message", "Please select a model file manually"});
        pending_dialog_results_size_++;
#endif
    });

    worker_thread.detach();
    show_info("Model Selection", "Opening model file selection dialog...");
}

void MainWindow::open_model_directory_dialog() {
    std::cout << "Opening model directory dialog..." << std::endl;

    std::thread worker_thread([this]() {
        std::string selected_path;

#ifdef USE_SDL2
        // Используем FileDialogHelper для выбора директории
        FileDialogHelper helper;
        if (helper.open_directory_dialog_sync("Select Models Directory", selected_path)) {
            if (!selected_path.empty()) {
                std::lock_guard<std::mutex> lock(pending_dialog_results_mutex_);
                pending_dialog_results_.push({"models_directory", selected_path});
                pending_dialog_results_size_++;
            }
        } else {
            std::lock_guard<std::mutex> lock(pending_dialog_results_mutex_);
            pending_dialog_results_.push({"message", "Please type the full path to your models directory in the input field"});
            pending_dialog_results_size_++;
        }
#else
        std::lock_guard<std::mutex> lock(pending_dialog_results_mutex_);
        pending_dialog_results_.push({"message", "Please type the full path to your models directory in the input field"});
        pending_dialog_results_size_++;
#endif
    });

    worker_thread.detach();
    show_info("Model Directory", "Opening directory selection dialog...");
}

// Удалены дублирующие методы:
// - try_open_native_file_dialog()
// - try_zenity_file_dialog()
// - try_kdialog_file_dialog()
// - try_python_file_dialog()
// Теперь используется FileDialogHelper напрямую

} // namespace ui
} // namespace llama_gui
