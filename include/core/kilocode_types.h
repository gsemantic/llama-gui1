#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace llama_gui {
namespace core {

/**
 * @brief Информация о модели KiloCode
 */
struct KiloCodeModel {
    std::string id;                    // Уникальный идентификатор модели (например, "kilo-auto/free")
    std::string name;                  // Отображаемое имя
    std::string provider;              // Провайдер модели
    std::string description;           // Описание модели
    int64_t context_length = 0;        // Размер контекстного окна
    int64_t max_output_tokens = 0;     // Максимум выходных токенов
    bool is_free = false;              // Бесплатная ли модель

    // Цены (в долларах за 1M токенов)
    double prompt_price_usd_per_million = 0.0;
    double completion_price_usd_per_million = 0.0;

    // Для сортировки и фильтрации
    bool operator<(const KiloCodeModel& other) const {
        return name < other.name;
    }
};

/**
 * @brief Ответ API KiloCode со списком моделей
 */
struct KiloCodeModelsResponse {
    std::vector<KiloCodeModel> models;
    std::string error;
    bool success = false;
};

/**
 * @brief Параметры для запроса к KiloCode API
 */
struct KiloCodeRequestParams {
    std::string model;
    std::string prompt;
    int max_tokens = 1024;
    float temperature = 0.7f;
    float top_p = 0.9f;
    int top_k = 40;
    bool stream = false;

    // Дополнительные параметры
    float presence_penalty = 0.0f;
    float frequency_penalty = 0.0f;

    // Системный промпт
    std::string system_prompt;

    // История диалога (для chat completion)
    struct Message {
        std::string role;  // "system", "user", "assistant"
        std::string content;
    };
    std::vector<Message> messages;
};

/**
 * @brief Ответ от KiloCode на запрос генерации
 */
struct KiloCodeCompletionResponse {
    std::string id;
    std::string model;
    std::string content;
    std::string finish_reason;

    // Статистика использования
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;

    // Стоимость (если доступна)
    double cost_usd = 0.0;

    std::string error;
    bool success = false;
};

/**
 * @brief Статус операции
 */
enum class KiloCodeStatus {
    Success,
    NetworkError,
    ApiError,
    AuthError,
    RateLimitError,
    InvalidRequest,
    Timeout,
    ProxyError
};

/**
 * @brief Информация о лимитах KiloCode
 */
struct KiloCodeRateLimit {
    int total_requests = 0;
    int remaining_requests = 0;
    int limit = 0;
    std::string reset_time;
    bool is_free_tier = true;

    std::string get_status_text() const {
        if (remaining_requests <= 0) {
            return "Лимит исчерпан";
        } else if (remaining_requests <= 10) {
            return "Мало запросов";
        } else {
            return "Нормально";
        }
    }
};

/**
 * @brief Результат операции с KiloCode
 */
struct KiloCodeResult {
    KiloCodeStatus status;
    std::string message;

    static KiloCodeResult success(const std::string& msg = "OK") {
        return {KiloCodeStatus::Success, msg};
    }

    static KiloCodeResult error(KiloCodeStatus s, const std::string& msg) {
        return {s, msg};
    }
};

} // namespace core
} // namespace llama_gui
