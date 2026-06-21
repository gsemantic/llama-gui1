#pragma once

#include "openrouter_types.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace llama_gui {
namespace core {

/**
 * @brief Клиент для работы с OpenRouter API
 * 
 * API документация: https://openrouter.ai/docs
 * 
 * Использование:
 * - Получение списка моделей: get_models()
 * - Фильтрация бесплатных моделей: get_free_models()
 * - Поиск моделей: search_models(query)
 * - Генерация текста: complete()
 */
class OpenRouterClient {
public:
    /**
     * @brief Callback для получения списка моделей
     */
    using ModelsCallback = std::function<void(const OpenRouterModelsResponse& response)>;
    
    /**
     * @brief Callback для получения ответа на запрос генерации
     */
    using CompletionCallback = std::function<void(const OpenRouterCompletionResponse& response)>;
    
    /**
     * @brief Callback для потоковой передачи токенов
     */
    using StreamCallback = std::function<void(const std::string& token, bool is_done)>;

    /**
     * @brief Конструктор
     * @param api_key API ключ OpenRouter (опционально, нужен для платных моделей)
     */
    explicit OpenRouterClient(const std::string& api_key = "");
    
    /**
     * @brief Деструктор
     */
    ~OpenRouterClient();
    
    // Запрет копирования
    OpenRouterClient(const OpenRouterClient&) = delete;
    OpenRouterClient& operator=(const OpenRouterClient&) = delete;

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
     * @param url Базовый URL (по умолчанию "https://openrouter.ai/api/v1")
     */
    void set_base_url(const std::string& url);
    
    /**
     * @brief Получение базового URL
     */
    std::string get_base_url() const { return base_url_; }

    /**
     * @brief Установка таймаута запросов
     * @param timeout_ms Таймаут в миллисекундах
     */
    void set_timeout(int timeout_ms);

    /**
     * @brief Получение текущего таймаута
     * @return Таймаут в миллисекундах
     */
    int get_timeout() const { return timeout_ms_; }

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
    OpenRouterModelsResponse get_models();

    /**
     * @brief Получение списка бесплатных моделей (асинхронно)
     * @param callback Callback для получения результата
     * @return true если запрос успешно инициирован
     */
    bool get_free_models_async(ModelsCallback callback);
    
    /**
     * @brief Получение списка бесплатных моделей (синхронно)
     * @return Результат запроса
     */
    OpenRouterModelsResponse get_free_models();

    /**
     * @brief Поиск моделей по имени (асинхронно)
     * @param query Строка поиска (регистронезависимая)
     * @param free_only Фильтр только бесплатные модели
     * @param callback Callback для получения результата
     * @return true если запрос успешно инициирован
     */
    bool search_models_async(const std::string& query, bool free_only, ModelsCallback callback);
    
    /**
     * @brief Поиск моделей по имени (синхронно)
     * @param query Строка поиска (регистронезависимая)
     * @param free_only Фильтр только бесплатные модели
     * @return Результат запроса
     */
    OpenRouterModelsResponse search_models(const std::string& query, bool free_only = false);

    /**
     * @brief Получение детальной информации о модели
     * @param model_id ID модели
     * @return Детальная информация о модели
     */
    OpenRouterModelDetails get_model_details(const std::string& model_id);

    /**
     * @brief Запрос на генерацию текста (синхронно)
     * @param params Параметры запроса
     * @return Результат генерации
     */
    OpenRouterCompletionResponse complete(const OpenRouterRequestParams& params);

    /**
     * @brief Запрос на генерацию текста с потоковой передачей (асинхронно)
     * @param params Параметры запроса
     * @param callback Callback для получения токенов
     * @return true если запрос успешно инициирован
     */
    bool complete_streaming_async(const OpenRouterRequestParams& params, StreamCallback callback);

    /**
     * @brief Проверка доступности API
     * @return true если API доступен
     */
    bool is_api_available();

    /**
     * @brief Получение информации о лимитах
     * @return Информация о лимитах
     */
    OpenRouterRateLimit get_rate_limit();

private:
    std::string api_key_;
    std::string base_url_ = "https://openrouter.ai/api/v1";
    int timeout_ms_ = 30000;
    
    // Кэш лимитов
    OpenRouterRateLimit rate_limit_cache_;
    std::time_t rate_limit_timestamp_ = 0;
    static constexpr int CACHE_DURATION_SEC = 60;  // Кэшируем на 1 минуту
    
    // Внутренние методы
    std::string make_request(const std::string& endpoint, const std::string& body = "");
    std::string build_url(const std::string& endpoint);
    std::vector<std::string> get_request_headers() const;
    
    // Статические callback функции для curl
    static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t streaming_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    
    // Вспомогательные методы
    OpenRouterModel parse_model(const nlohmann::json& json) const;
    OpenRouterModelsResponse parse_models_response(const std::string& json_str) const;
    OpenRouterCompletionResponse parse_completion_response(const std::string& json_str) const;
    
    // Фильтрация моделей
    std::vector<OpenRouterModel> filter_models(
        const std::vector<OpenRouterModel>& models,
        const std::string& query,
        bool free_only
    ) const;
};

} // namespace core
} // namespace llama_gui
