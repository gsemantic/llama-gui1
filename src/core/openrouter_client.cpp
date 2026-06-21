#include "../include/core/openrouter_client.h"
#include "../include/core/logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <algorithm>
#include <iostream>
#include <ctime>
#include <cctype>

using namespace llama_gui::core;

namespace llama_gui {
namespace core {

using json = nlohmann::json;

// ============================================================================
// Утилиты
// ============================================================================

// Очистка текста от непечатных символов и контрольних последовательностей
std::string sanitize_response_text(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        // ASCII печатные символы (пробел до ~)
        if (c >= 0x20 && c <= 0x7E) {
            output += c;
            i++;
            continue;
        }

        // Разрешаем переводы строк и табуляцию
        if (c == '\n' || c == '\r' || c == '\t') {
            output += c;
            i++;
            continue;
        }

        // UTF-8 последовательности
        if (c >= 0xC0) {
            int seq_len = 0;
            
            // Определяем длину последовательности по первому байту
            if ((c & 0xE0) == 0xC0) seq_len = 2;
            else if ((c & 0xF0) == 0xE0) seq_len = 3;
            else if ((c & 0xF8) == 0xF0) seq_len = 4;
            else {
                // Невалидный байт - пропускаем
                i++;
                continue;
            }
            
            // Проверяем, что вся последовательность корректна
            bool valid = true;
            if (i + seq_len <= input.size()) {
                for (int j = 1; j < seq_len; j++) {
                    if ((static_cast<unsigned char>(input[i + j]) & 0xC0) != 0x80) {
                        valid = false;
                        break;
                    }
                }
            } else {
                valid = false;
            }
            
            if (valid) {
                // Проверяем на контрольные символы Unicode (U+0000-U+001F, U+007F-U+009F)
                if (seq_len == 1 || (seq_len == 2 && c < 0xC2)) {
                    // Контрольные символы или overlong encoding - пропускаем
                    i += seq_len;
                    continue;
                }
                
                // Добавляем валидную UTF-8 последовательность
                output += input.substr(i, seq_len);
                i += seq_len;
            } else {
                // Невалидная последовательность - пропускаем
                i++;
            }
            continue;
        }

        // Все остальные символы (контрольные ASCII) - пропускаем
        i++;
    }

    return output;
}

// ============================================================================
// Конструктор/деструктор
// ============================================================================

OpenRouterClient::OpenRouterClient(const std::string& api_key)
    : api_key_(api_key) {
    std::cout << "[OpenRouter Client] Инициализация с ключом: " 
              << (api_key.empty() ? "НЕТ" : api_key.substr(0, 8) + "...") << std::endl;
    // Инициализация curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

OpenRouterClient::~OpenRouterClient() {
    curl_global_cleanup();
}

// ============================================================================
// Настройки
// ============================================================================

void OpenRouterClient::set_api_key(const std::string& api_key) {
    api_key_ = api_key;
}

void OpenRouterClient::set_base_url(const std::string& url) {
    // Если URL пустой, используем URL по умолчанию
    if (url.empty()) {
        base_url_ = "https://openrouter.ai/api/v1";
    } else {
        base_url_ = url;
        // Убираем trailing slash
        if (!base_url_.empty() && base_url_.back() == '/') {
            base_url_.pop_back();
        }
    }
}

void OpenRouterClient::set_timeout(int timeout_ms) {
    timeout_ms_ = timeout_ms;
}

// ============================================================================
// HTTP запросы
// ============================================================================

std::string OpenRouterClient::build_url(const std::string& endpoint) {
    return base_url_ + "/" + endpoint;
}

std::vector<std::string> OpenRouterClient::get_request_headers() const {
    std::vector<std::string> headers;
    
    // Content-Type
    headers.push_back("Content-Type: application/json");
    
    // Authorization (если есть API ключ)
    if (!api_key_.empty()) {
        headers.push_back("Authorization: Bearer " + api_key_);
    }
    
    // HTTP-Referer (требуется OpenRouter для рейтинга)
    headers.push_back("HTTP-Referer: https://github.com/llama-gui");
    
    // X-Title (требуется OpenRouter для рейтинга)
    headers.push_back("X-Title: llama-gui");
    
    return headers;
}

size_t OpenRouterClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t OpenRouterClient::streaming_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    auto* stream_data = static_cast<std::pair<std::string*, bool*>*>(userp);
    
    if (!stream_data || !stream_data->first) {
        return 0;
    }
    
    stream_data->first->append(static_cast<char*>(contents), total_size);
    return total_size;
}

std::string OpenRouterClient::make_request(const std::string& endpoint, const std::string& body) {
    std::string response;
    std::string url = build_url(endpoint);

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("OpenRouter: Не удалось инициализировать CURL");
        return "";
    }

    struct curl_slist* headers = nullptr;
    auto header_list = get_request_headers();
    for (const auto& h : header_list) {
        headers = curl_slist_append(headers, h.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Отключаем HTTP/2, используем HTTP/1.1 для совместимости
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    // Для POST запросов
    if (!body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        
        // Логирование запроса для отладки
        std::cout << "[OpenRouter] POST " << url << std::endl;
        std::cout << "[OpenRouter] Request body: " << body.substr(0, 200) << "..." << std::endl;
    }

    // Выполнение запроса
    CURLcode res = curl_easy_perform(curl);
    
    // Получение HTTP кода ответа
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    // Очистка
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::string error_msg = curl_easy_strerror(res);
        LOG_ERROR("OpenRouter: Ошибка запроса: " + error_msg);
        std::cerr << "[OpenRouter] CURL Error: " << error_msg << std::endl;
        std::cerr << "[OpenRouter] URL: " << url << std::endl;
        return "";
    }
    
    if (response_code != 200) {
        std::cerr << "[OpenRouter] HTTP Error: " << response_code << std::endl;
        std::cerr << "[OpenRouter] Response: " << response.substr(0, 500) << std::endl;
    }

    return response;
}

// ============================================================================
// Получение моделей
// ============================================================================

OpenRouterModel OpenRouterClient::parse_model(const json& json) const {
    OpenRouterModel model;
    
    model.id = json.value("id", "");
    model.name = json.value("name", model.id);
    
    // Провайдер
    if (json.contains("provider")) {
        const auto& provider = json["provider"];
        model.provider = provider.value("name", "");
    }
    
    model.description = json.value("description", "");
    model.context_length = json.value("context_length", static_cast<int64_t>(0));

    // Проверка на бесплатность - проверяем несколько источников
    model.is_free = false;
    
    // Источник 1: top_provider.is_free
    if (json.contains("top_provider") && json["top_provider"].is_object()) {
        model.is_free = json["top_provider"].value("is_free", false);
    }
    
    // Источник 2: pricing.prompt == "0"
    if (json.contains("pricing") && json["pricing"].is_object()) {
        const auto& pricing = json["pricing"];
        std::string prompt_price = pricing.value("prompt", "0");
        if (prompt_price == "0" || prompt_price == "0.0" || prompt_price.empty()) {
            model.is_free = true;
        }
    }
    
    // Цены
    if (json.contains("pricing")) {
        const auto& pricing = json["pricing"];
        try {
            std::string prompt_price = pricing.value("prompt", "0");
            std::string completion_price = pricing.value("completion", "0");
            model.prompt_price_usd_per_million = std::stod(prompt_price);
            model.completion_price_usd_per_million = std::stod(completion_price);
        } catch (...) {
            model.prompt_price_usd_per_million = 0.0;
            model.completion_price_usd_per_million = 0.0;
        }
    }
    
    // Топология
    if (json.contains("topology")) {
        const auto& topology = json["topology"];
        model.topology = topology.value("type", "");
    }
    
    // Модальности
    if (json.contains("modality")) {
        const auto& modality = json["modality"];
        if (modality.is_array()) {
            for (const auto& m : modality) {
                model.modality.push_back(m.get<std::string>());
            }
        }
    }
    
    return model;
}

OpenRouterModelsResponse OpenRouterClient::parse_models_response(const std::string& json_str) const {
    OpenRouterModelsResponse response;
    
    try {
        json data = json::parse(json_str);
        
        if (data.contains("data") && data["data"].is_array()) {
            for (const auto& model_json : data["data"]) {
                OpenRouterModel model = parse_model(model_json);
                response.models.push_back(model);
            }
            
            // Сортировка по имени
            std::sort(response.models.begin(), response.models.end());
            response.success = true;
        } else {
            response.error = "Неверный формат ответа API";
        }
        
    } catch (const json::parse_error& e) {
        response.error = "Ошибка парсинга JSON: " + std::string(e.what());
    } catch (const std::exception& e) {
        response.error = "Ошибка обработки: " + std::string(e.what());
    }
    
    return response;
}

OpenRouterModelsResponse OpenRouterClient::get_models() {
    std::string response = make_request("models");
    return parse_models_response(response);
}

bool OpenRouterClient::get_models_async(ModelsCallback callback) {
    if (!callback) {
        return false;
    }
    
    std::thread([this, callback]() {
        auto response = get_models();
        callback(response);
    }).detach();
    
    return true;
}

OpenRouterModelsResponse OpenRouterClient::get_free_models() {
    auto response = get_models();
    
    if (response.success) {
        // Фильтрация только бесплатных моделей
        std::vector<OpenRouterModel> free_models;
        for (const auto& model : response.models) {
            if (model.is_free) {
                free_models.push_back(model);
            }
        }
        response.models = free_models;
    }
    
    return response;
}

bool OpenRouterClient::get_free_models_async(ModelsCallback callback) {
    if (!callback) {
        return false;
    }
    
    std::thread([this, callback]() {
        auto response = get_free_models();
        callback(response);
    }).detach();
    
    return true;
}

// ============================================================================
// Поиск моделей
// ============================================================================

std::vector<OpenRouterModel> OpenRouterClient::filter_models(
    const std::vector<OpenRouterModel>& models,
    const std::string& query,
    bool free_only
) const {
    std::vector<OpenRouterModel> result;
    
    std::string query_lower = query;
    std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);
    
    for (const auto& model : models) {
        // Фильтр по бесплатности
        if (free_only && !model.is_free) {
            continue;
        }
        
        // Поиск по имени и ID
        if (query.empty()) {
            result.push_back(model);
            continue;
        }
        
        std::string name_lower = model.name;
        std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
        
        std::string id_lower = model.id;
        std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);
        
        std::string provider_lower = model.provider;
        std::transform(provider_lower.begin(), provider_lower.end(), provider_lower.begin(), ::tolower);
        
        if (name_lower.find(query_lower) != std::string::npos ||
            id_lower.find(query_lower) != std::string::npos ||
            provider_lower.find(query_lower) != std::string::npos) {
            result.push_back(model);
        }
    }
    
    return result;
}

OpenRouterModelsResponse OpenRouterClient::search_models(const std::string& query, bool free_only) {
    auto response = get_models();
    
    if (response.success) {
        response.models = filter_models(response.models, query, free_only);
    }
    
    return response;
}

bool OpenRouterClient::search_models_async(const std::string& query, bool free_only, ModelsCallback callback) {
    if (!callback) {
        return false;
    }
    
    std::thread([this, query, free_only, callback]() {
        auto response = search_models(query, free_only);
        callback(response);
    }).detach();
    
    return true;
}

// ============================================================================
// Детали модели
// ============================================================================

OpenRouterModelDetails OpenRouterClient::get_model_details(const std::string& model_id) {
    OpenRouterModelDetails details;
    details.id = model_id;
    
    // Получаем список всех моделей и ищем нужную
    auto response = get_models();
    
    if (!response.success) {
        details.error = response.error;
        details.success = false;
        return details;
    }
    
    // Поиск модели по ID
    for (const auto& model : response.models) {
        if (model.id == model_id) {
            details.name = model.name;
            details.description = model.description;
            details.provider_name = model.provider;
            details.context_length = model.context_length;
            details.prompt_price_usd_per_million = model.prompt_price_usd_per_million;
            details.completion_price_usd_per_million = model.completion_price_usd_per_million;
            details.is_free = model.is_free;
            details.modality = model.modality;
            details.success = true;
            return details;
        }
    }
    
    details.error = "Модель не найдена";
    details.success = false;
    return details;
}

// ============================================================================
// Генерация текста
// ============================================================================

OpenRouterCompletionResponse OpenRouterClient::parse_completion_response(const std::string& json_str) const {
    OpenRouterCompletionResponse response;

    try {
        json data = json::parse(json_str);

        if (data.contains("error")) {
            const auto& error = data["error"];
            std::string error_msg = error.value("message", "Неизвестная ошибка API");
            int error_code = error.value("code", -1);
            
            // Формируем понятное сообщение об ошибке
            std::string user_friendly_msg;
            
            if (error_code == 429) {
                // Rate limit
                std::string provider_name = "API";
                std::string model_name = "";
                
                if (error.contains("metadata")) {
                    const auto& metadata = error["metadata"];
                    if (metadata.contains("provider_name")) {
                        provider_name = metadata.value("provider_name", "API");
                    }
                    if (metadata.contains("raw")) {
                        // Извлекаем имя модели из сообщения
                        std::string raw_msg = metadata.value("raw", "");
                        size_t pos = raw_msg.find(":");
                        if (pos != std::string::npos) {
                            model_name = raw_msg.substr(0, pos);
                        }
                    }
                }
                
                user_friendly_msg = "⏱️ Превышен лимит запросов (Rate Limit)\n\n";
                user_friendly_msg += "Модель: " + model_name + "\n";
                user_friendly_msg += "Провайдер: " + provider_name + "\n\n";
                user_friendly_msg += "Причины:\n";
                user_friendly_msg += "• Бесплатные модели имеют ограничения по количеству запросов\n";
                user_friendly_msg += "• Слишком много запросов за короткое время\n\n";
                user_friendly_msg += "Решения:\n";
                user_friendly_msg += "1. Подождите 5-10 минут и попробуйте снова\n";
                user_friendly_msg += "2. Используйте локальную модель (llama-server)\n";
                user_friendly_msg += "3. Добавьте свой API ключ OpenRouter (платно)\n";
                
                LOG_ERROR("[OpenRouter] Rate limit exceeded: " + error_msg);
                
            } else if (error_code == 401) {
                user_friendly_msg = "🔐 Ошибка авторизации\n\n";
                user_friendly_msg += "Проверьте API ключ OpenRouter:\n";
                user_friendly_msg += "• Настройки → OpenRouter → API Key\n";
                user_friendly_msg += "• Или используйте локальную модель";
                
                LOG_ERROR("[OpenRouter] Authentication failed: " + error_msg);
                
            } else if (error_code == 403) {
                user_friendly_msg = "🚫 Доступ запрещён\n\n";
                user_friendly_msg += "Возможные причины:\n";
                user_friendly_msg += "• Модель недоступна в вашем регионе\n";
                user_friendly_msg += "• Требуется платная подписка\n\n";
                user_friendly_msg += "Попробуйте другую модель или локальную.";
                
                LOG_ERROR("[OpenRouter] Access forbidden: " + error_msg);
                
            } else if (error_code == 404) {
                user_friendly_msg = "❌ Модель не найдена\n\n";
                user_friendly_msg += "Проверьте название модели или выберите другую.";
                
                LOG_ERROR("[OpenRouter] Model not found: " + error_msg);
                
            } else if (error_code >= 500) {
                user_friendly_msg = "🔧 Ошибка сервера (" + std::to_string(error_code) + ")\n\n";
                user_friendly_msg += "Проблема на стороне провайдера.\n";
                user_friendly_msg += "Попробуйте позже или используйте другую модель.";
                
                LOG_ERROR("[OpenRouter] Server error " + std::to_string(error_code) + ": " + error_msg);
                
            } else {
                user_friendly_msg = "⚠️ Ошибка API (код: " + std::to_string(error_code) + ")\n\n";
                user_friendly_msg += error_msg;
                
                LOG_ERROR("[OpenRouter] API error " + std::to_string(error_code) + ": " + error_msg);
            }
            
            response.error = user_friendly_msg;
            response.success = false;
            return response;
        }

        response.id = data.value("id", "");
        response.model = data.value("model", "");

        // Извлечение контента
        if (data.contains("choices") && data["choices"].is_array() && !data["choices"].empty()) {
            const auto& choice = data["choices"][0];
            if (choice.contains("message") && choice["message"].contains("content")) {
                std::string raw_content = choice["message"]["content"].get<std::string>();
                // Очищаем контент от непечатных символов
                response.content = sanitize_response_text(raw_content);
            }
            response.finish_reason = choice.value("finish_reason", "");
        }

        // Статистика токенов
        if (data.contains("usage")) {
            const auto& usage = data["usage"];
            response.prompt_tokens = usage.value("prompt_tokens", 0);
            response.completion_tokens = usage.value("completion_tokens", 0);
            response.total_tokens = usage.value("total_tokens", 0);
        }

        // Стоимость
        if (data.contains("cost")) {
            response.cost_usd = data.value("cost", 0.0);
        }

        response.success = true;

    } catch (const json::parse_error& e) {
        response.error = "❌ Ошибка парсинга ответа\n\n";
        response.error += "Не удалось обработать ответ от сервера.\n";
        response.error += "Проверьте подключение к интернету.\n\n";
        response.error += "Детали: " + std::string(e.what());
        response.success = false;
    } catch (const std::exception& e) {
        response.error = "❌ Внутренняя ошибка\n\n";
        response.error += "Произошла непредвиденная ошибка.\n\n";
        response.error += "Детали: " + std::string(e.what());
        response.success = false;
    }

    return response;
}

OpenRouterCompletionResponse OpenRouterClient::complete(const OpenRouterRequestParams& params) {
    json request_body;

    // Обязательные параметры
    request_body["model"] = params.model;
    request_body["messages"] = json::array();
    
    // Формирование сообщений
    if (!params.messages.empty()) {
        for (const auto& msg : params.messages) {
            json message;
            message["role"] = msg.role;
            message["content"] = msg.content;
            request_body["messages"].push_back(message);
        }
    } else if (!params.prompt.empty()) {
        // Legacy format с prompt
        if (!params.system_prompt.empty()) {
            json system_msg;
            system_msg["role"] = "system";
            system_msg["content"] = params.system_prompt;
            request_body["messages"].push_back(system_msg);
        }
        json user_msg;
        user_msg["role"] = "user";
        user_msg["content"] = params.prompt;
        request_body["messages"].push_back(user_msg);
    }
    
    // Опциональные параметры (только если не default значения)
    if (params.max_tokens != 1024) {
        request_body["max_tokens"] = params.max_tokens;
    }
    if (params.temperature != 0.7f) {
        request_body["temperature"] = params.temperature;
    }
    if (params.top_p != 0.9f) {
        request_body["top_p"] = params.top_p;
    }
    if (params.presence_penalty != 0.0f) {
        request_body["presence_penalty"] = params.presence_penalty;
    }
    if (params.frequency_penalty != 0.0f) {
        request_body["frequency_penalty"] = params.frequency_penalty;
    }
    
    request_body["stream"] = params.stream;
    
    std::string response_str = make_request("chat/completions", request_body.dump());
    return parse_completion_response(response_str);
}

bool OpenRouterClient::complete_streaming_async(const OpenRouterRequestParams& params, StreamCallback callback) {
    if (!callback) {
        return false;
    }
    
    std::thread([this, params, callback]() {
        // TODO: Реализовать потоковую передачу
        auto response = complete(params);
        if (response.success) {
            callback(response.content, true);
        }
    }).detach();
    
    return true;
}

// ============================================================================
// Проверка доступности
// ============================================================================

bool OpenRouterClient::is_api_available() {
    std::string response = make_request("models");
    return !response.empty();
}

// ============================================================================
// Информация о лимитах
// ============================================================================

OpenRouterRateLimit OpenRouterClient::get_rate_limit() {
    // Проверяем кэш
    std::time_t now = std::time(nullptr);
    if (rate_limit_timestamp_ > 0 && 
        (now - rate_limit_timestamp_) < CACHE_DURATION_SEC &&
        rate_limit_cache_.remaining_requests > 0) {
        return rate_limit_cache_;
    }
    
    // Запрашиваем информацию через GET /api/v1/auth/key
    std::string url = build_url("auth/key");
    std::string response;
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        return rate_limit_cache_;
    }
    
    struct curl_slist* headers = nullptr;
    auto header_list = get_request_headers();
    for (const auto& h : header_list) {
        headers = curl_slist_append(headers, h.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000);  // Короткий таймаут
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    
    CURLcode res = curl_easy_perform(curl);
    
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (res == CURLE_OK && response_code == 200 && !response.empty()) {
        try {
            json data = json::parse(response);
            
            // Парсим информацию о лимитах
            if (data.contains("data")) {
                const auto& key_data = data["data"];
                
                // Лимиты
                if (key_data.contains("usage")) {
                    const auto& usage = key_data["usage"];
                    if (usage.contains("requests")) {
                        rate_limit_cache_.total_requests = usage.value("requests", 0);
                    }
                }
                
                // Остаток (вычисляем)
                if (key_data.contains("limit")) {
                    rate_limit_cache_.limit = key_data.value("limit", 50);
                }
                
                // Бесплатный ли тариф
                if (key_data.contains("is_free")) {
                    rate_limit_cache_.is_free_tier = key_data.value("is_free", true);
                }
                
                // Вычисляем остаток
                rate_limit_cache_.remaining_requests = 
                    std::max(0, rate_limit_cache_.limit - rate_limit_cache_.total_requests);
            }
        } catch (...) {
            // Ошибка парсинга - возвращаем кэш
        }
    }
    
    rate_limit_timestamp_ = now;
    return rate_limit_cache_;
}

} // namespace core
} // namespace llama_gui
