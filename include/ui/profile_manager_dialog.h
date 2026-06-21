#pragma once

#include <string>
#include <vector>
#include <functional>
#include "../core/config_manager.h"

namespace llama_gui {
namespace ui {

/**
 * @class ProfileManagerDialog
 * @brief Диалог управления профилями настроек
 * 
 * Позволяет пользователю:
 * - Просматривать список профилей
 * - Загружать профили
 * - Сохранять текущие настройки в профиль
 * - Создавать новые профили
 * - Удалять существующие профили
 */
class ProfileManagerDialog {
public:
    explicit ProfileManagerDialog(llama_gui::core::ConfigManager& config);
    ~ProfileManagerDialog() = default;

    // Запрет копирования
    ProfileManagerDialog(const ProfileManagerDialog&) = delete;
    ProfileManagerDialog& operator=(const ProfileManagerDialog&) = delete;

    /**
     * @brief Отрисовка диалога
     */
    void render();

    /**
     * @brief Открыть/закрыть диалог
     */
    void setOpen(bool open) { is_open_ = open; }
    bool isOpen() const { return is_open_; }

    /**
     * @brief Показать диалог создания нового профиля
     */
    void showCreateDialog();

    /**
     * @brief Установить callback для уведомления о загрузке профиля
     * @param callback Функция, вызываемая при загрузке профиля (принимает имя профиля)
     */
    void setProfileLoadCallback(std::function<void(const std::string&)> callback);

private:
    llama_gui::core::ConfigManager& config_manager_;
    bool is_open_ = false;
    std::function<void(const std::string&)> profile_load_callback_;
    
    // Состояние UI
    std::string selected_profile_;
    char new_profile_name_[256] = "";
    char profile_name_buffer_[256] = "";
    bool initialized_ = false; // Флаг инициализации при первом открытии

    // Флаги диалогов
    bool show_create_dialog_ = false;
    bool show_delete_confirm_ = false;
    bool show_rename_dialog_ = false;
    
    // Флаги для управления фокусом
    bool request_focus_create_dialog_ = false;
    bool request_focus_rename_dialog_ = false;
    
    // Сообщение о статусе
    std::string status_message_;
    float status_timer_ = 0.0f;

    /**
     * @brief Отрисовка списка профилей
     */
    void renderProfileList();

    /**
     * @brief Отрисовка кнопок действий
     */
    void renderActionButtons();

    /**
     * @brief Отрисовка диалога создания профиля
     */
    void renderCreateDialog();

    /**
     * @brief Отрисовка диалога подтверждения удаления
     */
    void renderDeleteConfirmDialog();

    /**
     * @brief Отрисовка диалога переименования
     */
    void renderRenameDialog();

    /**
     * @brief Показать временное сообщение о статусе
     */
    void showStatusMessage(const std::string& message);

    /**
     * @brief Отрисовка сообщения о статусе
     */
    void renderStatusMessage();
};

} // namespace ui
} // namespace llama_gui
