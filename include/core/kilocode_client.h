#pragma once

#include "kilocode_types.h"
#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>

namespace llama_gui {
namespace core {

/**
 * @brief Клиент для работы с KiloCode API
 *
 * API KiloCode совместим с OpenAI API, но требует SOCKS5 прокси (Tor)
 * Endpoint: https://api.kilo.ai/api/gateway
 */
class KiloCodeClient {
public:
    /**
     * @brief Конструктор
     * @param api_key API ключ KiloCode
     */
    explicit KiloCodeClient(const std::string& api_key = "");

    /**
     * @brief Деструктор
     */
    ~KiloCodeClient();

    // =========================================================================
    // Настройки подключения
    // =========================================================================

    /**
     * @brief Установить API ключ
     */
    void set_api_key(const std::string& api_key);

    /**
     * @brief Установить таймаут запросов (мс)
     */
    void set_timeout(int timeout_ms);

    /**
     * @brief Включить/выключить использование SOCKS5 прокси
     */
    void use_proxy(bool enabled);

    /**
     * @brief Установить адрес SOCKS5 прокси
     * @param proxy_url Формат: socks5://127.0.0.1:9050
     */
    void set_proxy_url(const std::string& proxy_url);

    /**
     * @brief Установить кастомный базовый URL
     */
    void set_base_url(const std::string& base_url);

    // =========================================================================
    // Получение списка моделей
    // =========================================================================

    /**
     * @brief Получить список всех моделей
     * @param callback Callback функция для асинхронного получения
     */
    void get_models_async(
        std::function<void(const KiloCodeModelsResponse&)> callback
    );

    /**
     * @brief Получить список бесплатных моделей
     * @param callback Callback функция для асинхронного получения
     */
    void get_free_models_async(
        std::function<void(const KiloCodeModelsResponse&)> callback
    );

    /**
     * @brief Поиск моделей
     * @param query Строка поиска
     * @param free_only Только бесплатные модели
     * @param callback Callback функция
     */
    void search_models_async(
        const std::string& query,
        bool free_only,
        std::function<void(const KiloCodeModelsResponse&)> callback
    );

    // =========================================================================
    // Генерация текста (chat completions)
    // =========================================================================

    /**
     * @brief Запрос на генерацию текста
     * @param params Параметры запроса
     * @return Ответ от API
     */
    KiloCodeCompletionResponse complete(const KiloCodeRequestParams& params);

    /**
     * @brief Асинхронный запрос на генерацию текста
     * @param params Параметры запроса
     * @param callback Callback функция
     */
    void complete_async(
        const KiloCodeRequestParams& params,
        std::function<void(const KiloCodeCompletionResponse&)> callback
    );

    /**
     * @brief Асинхронный запрос с потоковой генерацией
     * @param params Параметры запроса
     * @param chunk_callback Callback для каждого чанка
     * @param complete_callback Callback при завершении
     */
    void complete_streaming_async(
        const KiloCodeRequestParams& params,
        std::function<void(const std::string&)> chunk_callback,
        std::function<void(const KiloCodeCompletionResponse&)> complete_callback
    );

    // =========================================================================
    // Проверка доступности
    // =========================================================================

    /**
     * @brief Проверить доступность API
     * @return true если API доступен
     */
    bool is_api_available();

    /**
     * @brief Получить информацию о лимитах
     * @return RateLimit информация
     */
    KiloCodeRateLimit get_rate_limit();

    /**
     * @brief Проверить, работает ли Tor прокси
     * @return true если прокси доступен
     */
    bool is_proxy_available();

private:
    std::string api_key_;
    std::string base_url_ = "https://api.kilo.ai/api/gateway";
    int timeout_ms_ = 30000;
    bool use_proxy_ = false;
    std::string proxy_url_ = "socks5://127.0.0.1:9050";

    // Внутренние методы
    KiloCodeModelsResponse get_models();
    KiloCodeModelsResponse get_free_models();
    std::string build_url(const std::string& endpoint) const;
    std::vector<std::string> get_request_headers() const;
    std::string make_request(const std::string& endpoint, const std::string& body);
    std::string make_request_with_proxy(const std::string& endpoint, const std::string& body);
    KiloCodeModelsResponse parse_models_response(const std::string& json);
    KiloCodeCompletionResponse parse_completion_response(const std::string& json);
    KiloCodeModel parse_model(const nlohmann::json& j) const;
};

} // namespace core
} // namespace llama_gui
