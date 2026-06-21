#pragma once

#include "../core/embedding_generator.h"
#include "../core/rag_settings.h"
#include <imgui.h>
#include <string>
#include <memory>

namespace llama_gui {
namespace ui {

/**
 * @brief Диалог настройки эмбеддингов для RAG
 *
 * Позволяет настроить:
 * - Режим эмбеддинга (Local/Cloud/Hybrid)
 * - Облачный провайдер (HuggingFace/OpenRouter/Custom)
 * - API ключи и endpoint URL
 * - Модель для совместимости (all-MiniLM-L6-v2 по умолчанию)
 * - Кэширование эмбеддингов
 * - Статистику использования
 */
class EmbeddingSettingsDialog {
public:
    /**
     * @brief Конструктор
     * @param settings Ссылка на настройки RAG
     * @param embedding_generator Указатель на генератор эмбеддингов
     */
    explicit EmbeddingSettingsDialog(
        llama_gui::core::RagSettings& settings,
        std::shared_ptr<llama_gui::core::EmbeddingGenerator> embedding_generator);

    /**
     * @brief Деструктор
     */
    ~EmbeddingSettingsDialog() = default;

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
     * @brief Применение настроек
     */
    void apply_settings();

    /**
     * @brief Проверка доступности облачного API
     */
    void check_cloud_availability();

private:
    llama_gui::core::RagSettings& settings_;
    std::shared_ptr<llama_gui::core::EmbeddingGenerator> embedding_generator_;

    bool is_open_ = false;
    bool is_loading_ = false;
    bool cloud_available_ = false;

    // Буферы для UI
    char api_key_buffer_[256] = "";
    char endpoint_url_buffer_[512] = "";
    char model_name_buffer_[128] = "";

    // Состояние переключателей
    int selected_mode_index_ = 2;  // Hybrid по умолчанию
    int selected_provider_index_ = 0;  // HuggingFace по умолчанию

    // Статистика
    bool show_stats_ = false;

    // Вкладки
    enum class Tab {
        General = 0,
        Cloud,
        Local,
        Cache
    };
    Tab current_tab_ = Tab::General;

    // Вспомогательные методы
    void render_general_tab();
    void render_cloud_tab();
    void render_local_tab();
    void render_cache_tab();
    void render_stats();

    std::string get_mode_name(llama_gui::core::EmbeddingMode mode) const;
    std::string get_provider_name(llama_gui::core::CloudEmbeddingProvider provider) const;

    void on_mode_changed(int index);
    void on_provider_changed(int index);
};

} // namespace ui
} // namespace llama_gui
