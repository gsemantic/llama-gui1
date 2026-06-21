#include "../include/ui/main_window.h"

#ifdef ENABLE_LLAMA_BENCH

#include "../include/ui/llama_bench_dialog.h"
#include "../include/core/config_manager.h"
#include "../include/bench/bench_common.h"
#include <iostream>

namespace llama_gui {
namespace ui {

void MainWindow::openLlamaBenchDialog() {
    // Инициализировать диалог если нужно
    if (!llama_bench_dialog_) {
        llama_bench_dialog_ = std::make_unique<LlamaBenchDialog>();
        
        // Найти путь к llama-bench
        std::string llama_bench_path = config_manager_.getProfilesDirectory() + "/../../llama-b7472/llama-bench";
        
        // Проверить альтернативные пути
        if (!llama_gui::bench::BenchCommon::fileExists(llama_bench_path)) {
            llama_bench_path = "/home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-b7472/llama-bench";
        }
        
        std::string profiles_dir = config_manager_.getProfilesDirectory();
        
        if (!llama_bench_dialog_->initialize(llama_bench_path, profiles_dir)) {
            show_error("Llama Bench", "Failed to initialize Llama Bench dialog.\nMake sure llama-bench is available.");
            llama_bench_dialog_.reset();
            return;
        }
        
        // Передать server_manager для остановки/запуска
        llama_bench_dialog_->setServerManager(server_manager_);
        
        std::cout << "✓ Llama Bench dialog initialized" << std::endl;
    }
    
    // Показать диалог
    llama_bench_dialog_->setVisible(true);
}

} // namespace ui
} // namespace llama_gui

#endif // ENABLE_LLAMA_BENCH
