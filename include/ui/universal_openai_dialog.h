#pragma once

#include "../include/core/universal_openai_client.h"
#include "../include/core/universal_openai_settings.h"
#include "../include/core/settings.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace llama_gui {
namespace ui {

/**
 * @brief Диалог настройки универсального OpenAI-совместимого API
 */
class UniversalOpenAIDialog {
public:
    /**
     * @brief Конструктор
     * @param settings Ссылка на настройки приложения
     */
    explicit UniversalOpenAIDialog(llama_gui::core::Settings& settings);

    /**
     * @brief Открыть диалог
     */
    void open();

    /**
     * @brief Закрыть диалог
     */
    void close();

    /**
     * @brief Отрисовка диалога
     */
    void render();

private:
    /**
     * @brief Перечисление типов провайдеров
     */
    enum class ProviderType {
        Custom,
        OpenRouter,
        KiloCode,
        OpenAI,
        CustomEndpoint
    };

    /**
     * @brief Перечисление вкладок диалога
     */
    enum class DialogTab {
        General,
        Models
    };

    /**
     * @brief Перечисление вкладок в модели
     */
    enum class ModelTab {
        List,
        Details
    };

    // Настройки
    llama_gui::core::Settings& settings_;
    std::unique_ptr<llama_gui::core::UniversalOpenAIClient> client_;

    // Состояние диалога
    bool is_open_ = false;
    ProviderType selected_provider_ = ProviderType::Custom;
    DialogTab dialog_tab_ = DialogTab::General;
    ModelTab model_tab_ = ModelTab::List;

    // API ключ
    char api_key_buffer_[256] = {0};

    // Базовый URL
    char base_url_buffer_[512] = {0};

    // Endpoint
    char endpoint_buffer_[512] = {0};

    // Поиск моделей
    char search_buffer_[256] = {0};
    bool free_models_only_ = false;

    // Список моделей
    std::vector<llama_gui::core::UniversalOpenAIModel> models_;
    bool loading_models_ = false;
    std::string loading_error_;

    // Выбранная модель
    int selected_model_index_ = -1;

    // Кнопки
    bool show_model_list_ = false;
    bool show_usage_stats_ = false;

    // API ключ из .env
    std::string env_api_key_;

    // Внутренние методы
    void update_client();
    void load_models();
    void search_models(const std::string& query);
    void render_general_tab();
    void render_models_tab();
    void render_model_list();
    void render_model_details();
    void render_provider_presets();
};

} // namespace ui
} // namespace llama_gui
