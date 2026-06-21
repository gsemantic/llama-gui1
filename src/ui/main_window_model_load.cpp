#include "../include/ui/main_window.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <filesystem>

#include "../external/imgui/imgui.h"
#include "../include/core/state_manager.h"

namespace llama_gui {
namespace ui {

void MainWindow::load_model_with_progress_dialog(const std::string& model_path, const std::string& pending_query, bool at_startup) {
    if (!model_manager_) {
        std::cerr << "MainWindow: ModelManager не инициализирован" << std::endl;
        show_error("Загрузка модели", "ModelManager не инициализирован");
        return;
    }

    // Проверяем, существует ли файл
    if (!model_manager_->is_valid_model_file(model_path)) {
        std::cerr << "MainWindow: Неверный файл модели: " << model_path << std::endl;
        show_error("Загрузка модели", "Неверный файл модели: " + model_path);
        return;
    }

    // Сохраняем запрос и показываем диалог загрузки
    pending_query_ = pending_query;
    model_load_at_startup_ = at_startup;
    show_model_load_dialog_ = true;
    model_load_progress_ = 0.0f;
    model_load_status_ = "Инициализация...";
    is_model_loading_.store(true);  // Блокируем отправку запросов

    std::cout << "MainWindow: Загрузка модели: " << model_path << std::endl;

    // Запускаем загрузку в отдельном потоке
    std::thread([this, model_path, at_startup]() {
        // Устанавливаем callback для обновления прогресса
        model_manager_->set_model_progress_callback([this](const std::string& model_name, float progress, const std::string& status) {
            model_load_progress_ = progress;
            model_load_status_ = status;
            std::cout << "Progress: " << (progress * 100.0f) << "% - " << status << std::endl;
        });

        // Загружаем модель
        bool success = model_manager_->load_model(model_path);

        // После загрузки модели
        if (success) {
            std::cout << "MainWindow: Модель загружена успешно: " << model_path << std::endl;

            // Обновляем настройки с путем к модели
            settings_.set_model_path(model_path);

            // Автоматически обновляем алиас модели на основе имени файла
            std::string model_filename = std::filesystem::path(model_path).filename().string();
            // Убираем расширение .gguf и суффиксы квантования
            std::string model_alias = model_filename;
            size_t pos = model_alias.find(".gguf");
            if (pos != std::string::npos) {
                model_alias = model_alias.substr(0, pos);
            }
            // Убираем суффиксы квантования (Q4_K_M, Q5_K_M, Q8_K_XL и т.д.)
            std::vector<std::string> quant_suffixes = {
                "Q8_K_XL", "Q8_K", "Q6_K", "Q5_K_M", "Q5_K_S", "Q5_K", 
                "Q4_K_M", "Q4_K_S", "Q4_K", "Q3_K_M", "Q3_K_S", "Q3_K",
                "Q2_K", "IQ4_XS", "IQ4_NL", "IQ3_XXS", "IQ3_S", "IQ2_XXS", "IQ2_XS", "IQ2_S", "F16", "F32"
            };
            for (const auto& suffix : quant_suffixes) {
                pos = model_alias.find("-" + suffix);
                if (pos != std::string::npos) {
                    model_alias = model_alias.substr(0, pos);
                    break;
                }
                pos = model_alias.find("_" + suffix);
                if (pos != std::string::npos) {
                    model_alias = model_alias.substr(0, pos);
                    break;
                }
            }
            settings_.set_model_alias(model_alias);
            std::cout << "MainWindow: Алиас модели установлен: " << model_alias << std::endl;

            // Сохраняем профиль (чтобы путь к модели сохранился для следующего запуска)
            std::string current_profile = settings_.get_current_profile_name();
            if (!current_profile.empty()) {
                settings_.save_profile(current_profile);
                std::cout << "MainWindow: Профиль '" << current_profile << "' сохранён с новым путём к модели" << std::endl;
            } else {
                // Если профиль не выбран, сохраняем в 'default'
                settings_.save_profile("default");
                std::cout << "MainWindow: Профиль 'default' сохранён с новым путём к модели" << std::endl;
            }
            
            // Запускаем сервер с загруженной моделью
            std::cout << "🚀 Запуск сервера с моделью..." << std::endl;
            
            // Обновляем статус для отображения загрузки сервера
            model_load_status_ = "Запуск сервера...";
            model_load_progress_ = 0.5f;  // 50% - модель загружена, сервер запускается
            
            if (server_manager_->restart_server()) {
                std::cout << "✓ Сервер запущен успешно" << std::endl;

                // Сервер запускается асинхронно, поэтому сразу показываем прогресс
                // Ожидание готовности происходит в ServerManager
                model_load_status_ = "Сервер запускается...";
                model_load_progress_ = 0.8f;
                
                // Небольшая задержка для обновления UI перед закрытием диалога
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                
                std::cout << "✓ Сервер запущен в фоновом режиме" << std::endl;
                model_load_progress_ = 1.0f;
                model_load_status_ = "Готово!";
            } else {
                std::cerr << "⚠ Не удалось запустить сервер" << std::endl;
            }
        } else {
            std::cerr << "MainWindow: Ошибка загрузки модели: " << model_path << std::endl;
        }

        // Закрываем диалог
        show_model_load_dialog_ = false;
        model_load_at_startup_ = false;
        is_model_loading_.store(false);  // Разрешаем отправку запросов

        // Создаем новый чат после загрузки модели (если это не загрузка при старте)
        if (success && !at_startup) {
            std::string new_conv_id = state_manager_.create_conversation("New Chat");
            state_manager_.set_active_conversation(new_conv_id);
            std::cout << "Created new conversation after model load: " << new_conv_id << std::endl;
        }

        // Если был запрос, обрабатываем его
        if (!pending_query_.empty() && success) {
            // Отправляем запрос после загрузки модели
            chat_interface_->set_input_text(pending_query_);
            // Триггерим отправку сообщения
            force_ui_update_ = true;
        } else if (!success) {
            show_error("Загрузка модели", "Не удалось загрузить модель: " + model_path);
        }

        pending_query_.clear();
        
        // Также обрабатываем pending_query_for_loading_ из chat_interface
        if (success && chat_interface_) {
            std::string pending_query = chat_interface_->get_pending_query_for_loading();
            if (!pending_query.empty()) {
                chat_interface_->clear_pending_query_for_loading();
                chat_interface_->set_input_text(pending_query);
                force_ui_update_ = true;
            }
        }
    }).detach();
}

void MainWindow::render_model_load_dialog() {
    // Упрощенная версия - прогресс показывается в статус-баре
    // Это окно больше не используется
}

void MainWindow::start_model_load_from_profile(const std::string& pending_query) {
    // Получаем путь к модели из настроек
    std::string model_path = settings_.get_model_path();
    
    if (model_path.empty()) {
        show_error("Загрузка модели", "Путь к модели не указан в настройках");
        return;
    }

    // Запускаем загрузку с прогрессом
    load_model_with_progress_dialog(model_path, pending_query);
}

} // namespace ui
} // namespace llama_gui
