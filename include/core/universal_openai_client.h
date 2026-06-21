#pragma once

#include "universal_openai_types.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace llama_gui {
namespace core {

/**
 * @brief Клиент для работы с произвольными OpenAI-совместимыми API
 *
 * Поддерживает любые API, которые следуют спецификации OpenAI Chat Completions API:
 * - https://platform.openai.com/docs/api-reference/chat
 *
 * Примеры:
 * - OpenAI (https://api.openai.com/v1)
 * - OpenRouter (https://openrouter.ai/api/v1)
 * - KiloCode (https://api.kilo.ai/api/gateway)
 * - Локальные инстансы с совместимой API
 */
class UniversalOpenAIClient {
public:
    /**
     * @brief Callback для получения списка моделей
     */
    using ModelsCallback = std::function<void(const UniversalOpenAIModelsResponse&)>;

    /**
     * @brief Callback для получения ответа на запрос генерации
     */
    using CompletionCallback = std::function<void(const UniversalOpenAICompletionResponse&)>;

    /**
     * @brief Callback для потоковой передачи токенов
     */
    using StreamCallback = std::function<void(const UniversalOpenAIStreamChunk&)>;

    /**
     * @brief Конструктор
     * @param api_key API ключ (опционально)
     */
    explicit UniversalOpenAIClient(const std::string& api_key = "");

    /**
     * @brief Деструктор
     */
    ~UniversalOpenAIClient();

    // Запрет копирования
    UniversalOpenAIClient(const UniversalOpenAIClient&) = delete;
    UniversalOpenAIClient& operator=(const UniversalOpenAIClient&) = delete;

    /**
     * @brief Установка API ключа
     */
    void set_api_key(const std::string& api_key);

    /**
     * @brief Получение API ключа
     */
    std::string get_api_key() const { return api_key_; }

    /**
     * @brief Установка базового URL API
     * @param url Базовый URL (например, "https://api.openai.com/v1")
     */
    void set_base_url(const std::string& url);

    /**
     * @brief Получение базового URL
     */
    std::string get_base_url() const { return base_url_; }

    /**
     * @brief Установка пути к endpoint (например, "chat/completions")
     * @param endpoint Путь к endpoint (без слэша в начале)
     */
    void set_endpoint(const std::string& endpoint);

    /**
     * @brief Получение текущего endpoint
     */
    std::string get_endpoint() const { return endpoint_; }

    /**
     * @brief Установка таймаута запросов
     * @param timeout_ms Таймаут в миллисекундах
     */
    void set_timeout(int timeout_ms);

    /**
     * @brief Получение текущего таймаута
     */
    int get_timeout() const { return timeout_ms_; }

    /**
     * @brief Установка версии API (добавляется к base_url)
     * @param version Версия API (например, "v1")
     */
    void set_api_version(const std::string& version);

    /**
     * @brief Получение списка всех моделей (асинхронно)
     * @param callback Callback для получения результата
     * @return true если запрос успешно инициирован
     */
    bool get_models_async(ModelsCallback callback);

    /**
     * @brief Получение списка всех моделей (синхронно)
     * @return Результат запроса
     */
    UniversalOpenAIModelsResponse get_models();

    /**
     * @brief Запрос на генерацию текста (синхронно)
     * @param params Параметры запроса
     * @return Результат генерации
     */
    UniversalOpenAICompletionResponse complete(const UniversalOpenAIRequestParams& params);

    /**
     * @brief Запрос на генерацию текста с потоковой передачей (асинхронно)
     * @param params Параметры запроса
     * @param callback Callback для получения чанков
     * @return true если запрос успешно инициирован
     */
    bool complete_streaming_async(const UniversalOpenAIRequestParams& params, StreamCallback callback);

    /**
     * @brief Проверка доступности API
     * @return true если API доступен
     */
    bool is_api_available();

    /**
     * @brief Получение информации о лимитах
     * @return Информация о лимитах
     */
    UniversalOpenAIRateLimit get_rate_limit();

private:
    std::string api_key_;
    std::string base_url_ = "https://api.openai.com/v1";
    std::string endpoint_ = "chat/completions";
    int timeout_ms_ = 30000;

    // Внутренние методы
    std::string make_request(const std::string& body);
    std::vector<std::string> get_request_headers() const;

    // Статические callback функции для curl
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t streaming_write_callback(void* contents, size_t size, size_t nmemb, void* userp);

    // Вспомогательные методы
    UniversalOpenAIModel parse_model(const nlohmann::json& json) const;
    UniversalOpenAIModelsResponse parse_models_response(const std::string& json_str) const;
    UniversalOpenAICompletionResponse parse_completion_response(const std::string& json_str) const;
    UniversalOpenAIStreamChunk parse_stream_chunk(const std::string& line) const;

    // Парсинг чанков потока
    std::string parse_stream_data(const std::string& data) const;
};

} // namespace core
} // namespace llama_gui
