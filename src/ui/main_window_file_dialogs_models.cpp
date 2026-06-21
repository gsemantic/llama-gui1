#include "../include/ui/main_window.h"
#include "../include/ui/file_dialog_helper.h"
#include <iostream>

namespace llama_gui {
namespace ui {

bool MainWindow::try_open_model_file_dialog(std::string& selected_path) {
    // Используем FileDialogHelper с фильтром для файлов моделей
    FileDialogHelper helper;
    return helper.open_file_dialog_sync("Select Model File", selected_path, "model_files");
}

bool MainWindow::try_open_embedding_model_file_dialog(std::string& selected_path) {
    // Используем FileDialogHelper с фильтром для embedding моделей
    FileDialogHelper helper;
    return helper.open_file_dialog_sync("Select Embedding Model File", selected_path, "embedding_model_files");
}

// Удалены дублирующие методы:
// - try_zenity_model_file_dialog()
// - try_kdialog_model_file_dialog()
// - try_python_model_file_dialog()
// - try_zenity_embedding_model_file_dialog()
// - try_kdialog_embedding_model_file_dialog()
// - try_python_embedding_model_file_dialog()
// Теперь используется FileDialogHelper напрямую

} // namespace ui
} // namespace llama_gui
