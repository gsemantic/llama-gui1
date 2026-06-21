#pragma once

#include "../core/openrouter_types.h"
#include "../core/openrouter_client.h"
#include "../core/kilocode_types.h"
#include "../core/kilocode_client.h"
#include "../core/universal_openai_client.h"
#include "../core/universal_openai_types.h"
#include "../core/settings.h"
#include <imgui.h>
#include <vector>
#include <string>
#include <atomic>
#include <memory>

namespace llama_gui {
namespace ui {

/**
 * @brief Диалог настройки облачных сервисов (OpenRouter и другие)
 *
 * Предоставляет интерфейс для:
 * - Настройки OpenRouter API
 * - Выбора модели из доступных облачных
 * - Просмотра статистики использования
 * - Расширения для других облачных провайдеров в будущем
 */
class CloudServicesDialog {
public:
    /**
     * @brief Конструктор
     * @param settings Ссылка на настройки приложения
     */
    explicit CloudServicesDialog(llama_gui::core::Settings& settings);

    /**
     * @brief Деструктор
     */
    ~CloudServicesDialog() = default;

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
     * @brief Загрузка списка моделей OpenRouter
     */
    void load_models();

    /**
     * @brief Загрузка бесплатных моделей OpenRouter
     */
    void load_free_models();

    /**
     * @brief Поиск моделей
     * @param query Строка поиска
     * @param free_only Только бесплатные модели
     */
    void search_models(const std::string& query, bool free_only = false);

    /**
     * @brief Обновление настроек OpenRouter после загрузки профиля
     */
    void update_openrouter_settings();

private:
    llama_gui::core::Settings& settings_;
    std::unique_ptr<llama_gui::core::OpenRouterClient> openrouter_client_;
    std::unique_ptr<llama_gui::core::KiloCodeClient> kilocode_client_;

    bool is_open_ = false;
    bool is_loading_ = false;

    // Список моделей
    std::vector<llama_gui::core::OpenRouterModel> models_;
    std::vector<llama_gui::core::OpenRouterModel> filtered_models_;

    // UI состояние
    char search_buffer_[256] = "";
    char api_key_buffer_[256] = "";
    char custom_url_buffer_[512] = "";
    bool free_models_only_ = false;
    int selected_model_index_ = -1;
    bool show_model_list_ = false;
    bool show_usage_stats_ = false;

    // KiloCode UI состояние
    char kilocode_api_key_buffer_[256] = "";
    char kilocode_tor_proxy_buffer_[256] = "";
    char kilocode_search_buffer_[256] = "";
    bool kilocode_use_tor_ = true;
    int kilocode_selected_model_index_ = -1;
    std::vector<llama_gui::core::KiloCodeModel> kilocode_models_;
    std::vector<llama_gui::core::KiloCodeModel> kilocode_filtered_models_;

    // Universal UI состояние
    char universal_api_key_buffer_[256] = "";
    char universal_base_url_buffer_[512] = "";
    char universal_endpoint_buffer_[512] = "";
    char universal_search_buffer_[256] = "";
    bool universal_free_models_only_ = false;
    int universal_selected_model_index_ = -1;
    std::vector<llama_gui::core::UniversalOpenAIModel> universal_models_;
    std::vector<llama_gui::core::UniversalOpenAIModel> universal_filtered_models_;
    std::string loading_error_;

    // Вкладки облачных сервисов
    enum class CloudProvider {
        OpenRouter = 0,
        KiloCode = 1,
        Universal = 2
    };
    CloudProvider selected_provider_ = CloudProvider::OpenRouter;

    // Universal клиент
    std::unique_ptr<llama_gui::core::UniversalOpenAIClient> universal_client_;

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
    void render_openrouter_tab();
    void render_kilocode_tab();
    void render_model_details(const llama_gui::core::OpenRouterModel& model);
    void on_model_selected(const llama_gui::core::OpenRouterModel& model);
    std::string format_price(double price) const;
    std::string format_context_size(int64_t context) const;

    // KiloCode методы
    void load_kilocode_models();
    void load_kilocode_free_models();
    void search_kilocode_models(const std::string& query, bool free_only);
    void apply_kilocode_filters();
    void render_kilocode_model_list();
    void on_kilocode_model_selected(const llama_gui::core::KiloCodeModel& model);

    // Universal методы
    void load_universal_models();
    void load_universal_free_models();
    void search_universal_models(const std::string& query, bool free_only);
    void apply_universal_filters();
    void render_universal_model_list();
    void on_universal_model_selected(const llama_gui::core::UniversalOpenAIModel& model);
    void render_universal_tab();
    void update_universal_settings();

    // Callback для получения моделей
    void on_models_received(const llama_gui::core::OpenRouterModelsResponse& response);
    
    // Обновление статистики использования
    void refresh_usage_stats();
};

} // namespace ui
} // namespace llama_gui
