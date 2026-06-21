#pragma once

#include <string>
#include <vector>

namespace llama_gui {
namespace core {

/**
 * @brief Настройки универсального OpenAI-совместимого API
 */
struct UniversalOpenAISettings {
    bool enabled = false;                    // Использовать универсальный API вместо локальной модели
    std::string selected_model = "";         // Выбранная модель (ID)
    std::string custom_base_url = "";        // Кастомный базовый URL (опционально)
    std::string custom_endpoint = "";        // Кастомный endpoint (опционально)
    int timeout_ms = 30000;                  // Таймаут запросов
    bool free_models_only = true;            // Показывать только бесплатные модели
    std::string last_search_query = "";      // Последний поисковый запрос

    // Автозаполнение
    std::vector<std::string> recent_models;  // Недавние модели

    // Статистика использования (кэш)
    int usage_total_requests = 0;            // Всего использовано запросов
    int usage_remaining = 50;                // Осталось запросов
    int usage_limit = 50;                    // Дневной лимит
};

} // namespace core
} // namespace llama_gui
