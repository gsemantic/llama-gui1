#pragma once

#include "../core/openrouter_types.h"
#include "../core/openrouter_client.h"
#include "../core/settings.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <atomic>

namespace llama_gui {
namespace ui {

/**
 * @brief Диалог выбора моделей OpenRouter
 * 
 * Предоставляет интерфейс для:
 * - Просмотра списка доступных моделей
 * - Поиска моделей по имени
 * - Фильтрации бесплатных моделей
 * - Выбора модели для использования
 */
class OpenRouterModelsDialog {
public:
    /**
     * @brief Конструктор
     * @param settings Ссылка на настройки приложения
     */
    explicit OpenRouterModelsDialog(llama_gui::core::Settings& settings);
    
    /**
     * @brief Деструктор
     */
    ~OpenRouterModelsDialog() = default;

    /**
     * @brief Открытие диалога
     */
    void open();
    
    /**
     * @brief Закрытие диалога
     */
    void close();
    
    /**
     * @brief Проверка состояния диалога
     * @return true если диалог открыт
     */
    bool is_open() const { return is_open_; }

    /**
     * @brief Отрисовка диалога
     * Вызывается в главном цикле рендеринга
     */
    void render();

    /**
     * @brief Отрисовка списка моделей inline (внутри другого окна)
     * Вызывается внутри settings_dialog_server_chat.cpp
     */
    void render_inline();

    /**
     * @brief Загрузка списка моделей
     */
    void load_models();
    
    /**
     * @brief Загрузка бесплатных моделей
     */
    void load_free_models();
    
    /**
     * @brief Поиск моделей
     * @param query Строка поиска
     * @param free_only Только бесплатные модели
     */
    void search_models(const std::string& query, bool free_only = false);

private:
    llama_gui::core::Settings& settings_;
    llama_gui::core::OpenRouterClient client_;

    bool is_open_ = false;
    bool is_loading_ = false;

    // Список моделей
    std::vector<llama_gui::core::OpenRouterModel> models_;
    std::vector<llama_gui::core::OpenRouterModel> filtered_models_;
    
    // UI состояние
    char search_buffer_[256] = "";
    bool free_models_only_ = false;
    int selected_model_index_ = -1;
    
    // Колонки таблицы
    enum class Column {
        Name = 0,
        Provider,
        Context,
        Price,
        Free
    };
    
    // Вспомогательные методы
    void apply_filters();
    void render_model_list();
    void render_model_details(const llama_gui::core::OpenRouterModel& model);
    void on_model_selected(const llama_gui::core::OpenRouterModel& model);
    std::string format_price(double price) const;
    std::string format_context_size(int64_t context) const;
    
    // Callback для получения моделей
    void on_models_received(const llama_gui::core::OpenRouterModelsResponse& response);
};

} // namespace ui
} // namespace llama_gui
