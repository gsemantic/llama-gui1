#pragma once

#include <string>
#include <memory>
#include "core/settings.h"

namespace llama_gui {
namespace ui {

using Settings = llama_gui::core::Settings;

/**
 * @brief Диалог быстрых настроек (часто используемые)
 * 
 * Содержит 4 основные вкладки:
 * - Server (хост, порт, запуск/остановка)
 * - Chat (температура, max_tokens, системный промпт)
 * - Models (выбор модели, каталог)
 * - UI (тема, язык, шрифт)
 */
class QuickSettingsDialog {
public:
    QuickSettingsDialog(Settings& settings);
    ~QuickSettingsDialog();

    // Управление отображением
    void show() { show_dialog_ = true; }
    void hide() { show_dialog_ = false; }
    bool is_visible() const { return show_dialog_; }

    // Отрисовка
    void render();

private:
    // Вкладки
    void render_server_tab();
    void render_chat_tab();
    void render_models_tab();
    void render_ui_tab();

    // Обработчики кнопок
    void save_settings();
    void apply_settings();
    void reset_settings();
    void cancel_settings();

    // Вспомогательные методы
    static void HelpMarker(const std::string& desc);

    // Ссылка на настройки
    Settings& settings_;

    // Состояние UI
    bool show_dialog_ = false;
    bool settings_modified_ = false;

    // Буфер для имени профиля
    char current_profile_name_[128] = "";
    std::string status_message_;
};

} // namespace ui
} // namespace llama_gui
