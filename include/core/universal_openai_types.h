#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>

namespace llama_gui {
namespace core {

/**
 * @brief Информация о модели универсального OpenAI-совместимого API
 */
struct UniversalOpenAIModel {
    std::string id;                    // Уникальный идентификатор модели
    std::string object;                // Тип объекта ("model")
    std::string created;               // Дата создания (timestamp)
    std::string owned_by;              // Владелец модели

    // Дополнительные параметры
    int64_t context_length = 0;        // Размер контекстного окна
    std::vector<std::string> stop;     // Дополнительные стоп-последовательности
    std::string pricing;               // Информация о ценах

    // Для сортировки
    bool operator<(const UniversalOpenAIModel& other) const {
        return id < other.id;
    }
};

/**
 * @brief Ответ API со списком моделей
 */
struct UniversalOpenAIModelsResponse {
    std::vector<UniversalOpenAIModel> models;
    std::string error;
    bool success = false;
};

/**
 * @brief Сообщение в диалоге
 */
struct UniversalOpenAIMessage {
    std::string role;                  // "system", "user", "assistant", "tool"
    std::string content;
    std::string name;                  // Для tool сообщений
    std::vector<std::string> tool_calls;  // ID tool calls
    std::string tool_call_id;          // Для tool response

    // Для chat completions
    struct Tool {
        std::string type;              // "function"
        std::string function;          // {"name": "...", "arguments": "..."}
    };
    std::vector<Tool> tools;
};

/**
 * @brief Параметры для запроса chat completions
 */
struct UniversalOpenAIRequestParams {
    std::string model;
    std::vector<UniversalOpenAIMessage> messages;
    int max_tokens = 1024;
    float temperature = 0.7f;
    float top_p = 0.9f;
    int top_k = 40;
    bool stream = false;

    // Дополнительные параметры
    float presence_penalty = 0.0f;
    float frequency_penalty = 0.0f;

    // Дополнительные стоп-последовательности
    std::vector<std::string> stop;

    // Функции
    std::vector<std::string> functions;

    // User ID для трекинга
    std::string user_id = "llama-gui";
};

/**
 * @brief Ответ от API на запрос chat completions
 */
struct UniversalOpenAICompletionResponse {
    std::string id;
    std::string object;
    long long created = 0;
    std::string model;
    std::string choices_str;           // Оригинальная строка choices для парсинга

    struct Choice {
        int index = 0;
        std::string finish_reason;
        UniversalOpenAIMessage message;

        struct Delta {
            std::string role;
            std::string content;
            std::string tool_calls_json;
        };
        Delta delta;
    };
    std::vector<Choice> choices;

    // Usage
    struct Usage {
        int prompt_tokens = 0;
        int completion_tokens = 0;
        int total_tokens = 0;
    };
    Usage usage;

    // Error
    std::string error;
    bool success = false;
};

/**
 * @brief Потоковый чанк ответа
 */
struct UniversalOpenAIStreamChunk {
    std::string id;
    std::string object;
    long long created = 0;
    std::string model;
    std::string choices_str;

    struct Choice {
        int index = 0;
        std::string finish_reason;
        UniversalOpenAIMessage message;

        struct Delta {
            std::string role;
            std::string content;
        };
        Delta delta;
    };
    std::vector<Choice> choices;

    bool is_error = false;
    std::string error;
};

/**
 * @brief Статус операции
 */
enum class UniversalOpenAIStatus {
    Success,
    NetworkError,
    ApiError,
    AuthError,
    RateLimitError,
    InvalidRequest,
    Timeout,
    ParseError
};

/**
 * @brief Информация о лимитах
 */
struct UniversalOpenAIRateLimit {
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
 * @brief Результат операции
 */
struct UniversalOpenAIResult {
    UniversalOpenAIStatus status;
    std::string message;

    static UniversalOpenAIResult success(const std::string& msg = "OK") {
        return {UniversalOpenAIStatus::Success, msg};
    }

    static UniversalOpenAIResult error(UniversalOpenAIStatus s, const std::string& msg) {
        return {s, msg};
    }
};

} // namespace core
} // namespace llama_gui
