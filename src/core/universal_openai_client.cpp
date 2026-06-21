#include "../include/core/universal_openai_client.h"
#include "../include/core/logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <algorithm>
#include <iostream>
#include <ctime>

using namespace llama_gui::core;

namespace llama_gui {
namespace core {

using json = nlohmann::json;

// ============================================================================
// Конструктор/деструктор
// ============================================================================

UniversalOpenAIClient::UniversalOpenAIClient(const std::string& api_key)
    : api_key_(api_key) {
    std::cout << "[UniversalOpenAI Client] Инициализация с ключом: "
              << (api_key.empty() ? "НЕТ" : api_key.substr(0, 8) + "...") << std::endl;
    // Инициализация curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

UniversalOpenAIClient::~UniversalOpenAIClient() {
    curl_global_cleanup();
}

// ============================================================================
// Настройки
// ============================================================================

void UniversalOpenAIClient::set_api_key(const std::string& api_key) {
    api_key_ = api_key;
}

void UniversalOpenAIClient::set_base_url(const std::string& url) {
    if (url.empty()) {
        base_url_ = "https://api.openai.com/v1";
    } else {
        base_url_ = url;
        // Убираем trailing slash
        if (!base_url_.empty() && base_url_.back() == '/') {
            base_url_.pop_back();
        }
    }
}

void UniversalOpenAIClient::set_endpoint(const std::string& endpoint) {
    if (!endpoint.empty()) {
        endpoint_ = endpoint;
        // Убираем leading slash
        if (endpoint_.front() == '/') {
            endpoint_.erase(0, 1);
        }
    }
}

void UniversalOpenAIClient::set_api_version(const std::string& version) {
    if (!base_url_.empty() && !version.empty()) {
        if (base_url_.back() == '/') {
            base_url_ += version;
        } else {
            base_url_ += "/" + version;
        }
    }
}

void UniversalOpenAIClient::set_timeout(int timeout_ms) {
    timeout_ms_ = timeout_ms;
}

// ============================================================================
// HTTP запросы
// ============================================================================

std::vector<std::string> UniversalOpenAIClient::get_request_headers() const {
    std::vector<std::string> headers;

    // Content-Type
    headers.push_back("Content-Type: application/json");

    // Authorization (если есть API ключ)
    if (!api_key_.empty()) {
        headers.push_back("Authorization: Bearer " + api_key_);
    }

    // X-Client-Version (опционально)
    headers.push_back("X-Client-Version: llama-gui");

    return headers;
}

size_t UniversalOpenAIClient::write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t UniversalOpenAIClient::streaming_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    auto* stream_data = static_cast<std::pair<std::string*, bool*>*>(userp);

    if (!stream_data || !stream_data->first) {
        return 0;
    }

    stream_data->first->append(static_cast<char*>(contents), total_size);
    return total_size;
}

std::string UniversalOpenAIClient::make_request(const std::string& body) {
    std::string response;
    std::string url = base_url_ + "/" + endpoint_;

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("UniversalOpenAI: Не удалось инициализировать CURL");
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
        std::cout << "[UniversalOpenAI] POST " << url << std::endl;
        std::cout << "[UniversalOpenAI] Request body: " << body.substr(0, 200) << "..." << std::endl;
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
        LOG_ERROR("UniversalOpenAI: Ошибка запроса: " + error_msg);
        std::cerr << "[UniversalOpenAI] CURL Error: " << error_msg << std::endl;
        std::cerr << "[UniversalOpenAI] URL: " << url << std::endl;
        return "";
    }

    if (response_code != 200) {
        std::cerr << "[UniversalOpenAI] HTTP Error: " << response_code << std::endl;
        std::cerr << "[UniversalOpenAI] Response: " << response.substr(0, 500) << std::endl;
    }

    return response;
}

// ============================================================================
// Получение моделей
// ============================================================================

UniversalOpenAIModel UniversalOpenAIClient::parse_model(const json& json) const {
    UniversalOpenAIModel model;

    model.id = json.value("id", "");
    model.object = json.value("object", "");
    model.created = json.value("created", "");
    model.owned_by = json.value("owned_by", "");

    // Дополнительные параметры
    model.context_length = json.value("context_length", static_cast<int64_t>(0));
    model.stop = json.value("stop", std::vector<std::string>{});
    model.pricing = json.value("pricing", "");

    return model;
}

UniversalOpenAIModelsResponse UniversalOpenAIClient::parse_models_response(const std::string& json_str) const {
    UniversalOpenAIModelsResponse response;

    try {
        json data = json::parse(json_str);

        if (data.contains("data") && data["data"].is_array()) {
            for (const auto& model_json : data["data"]) {
                UniversalOpenAIModel model = parse_model(model_json);
                response.models.push_back(model);
            }

            // Сортировка по ID
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

UniversalOpenAIModelsResponse UniversalOpenAIClient::get_models() {
    std::string response = make_request("models");
    return parse_models_response(response);
}

bool UniversalOpenAIClient::get_models_async(ModelsCallback callback) {
    if (!callback) {
        return false;
    }

    std::thread([this, callback]() {
        auto response = get_models();
        callback(response);
    }).detach();

    return true;
}

// ============================================================================
// Генерация текста
// ============================================================================

UniversalOpenAICompletionResponse UniversalOpenAIClient::parse_completion_response(const std::string& json_str) const {
    UniversalOpenAICompletionResponse response;

    try {
        json data = json::parse(json_str);

        // Проверка на ошибку
        if (data.contains("error")) {
            const auto& error = data["error"];
            std::string error_msg = error.value("message", "Неизвестная ошибка API");
            int error_code = error.value("code", -1);

            // Формируем понятное сообщение об ошибке
            std::string user_friendly_msg;

            if (error_code == 429) {
                user_friendly_msg = "⏱️ Превышен лимит запросов (Rate Limit)\n\n" + error_msg;
            } else if (error_code == 401) {
                user_friendly_msg = "🔐 Ошибка авторизации\n\nПроверьте API ключ.";
            } else if (error_code == 403) {
                user_friendly_msg = "🚫 Доступ запрещён\n\nПроверьте права доступа к API.";
            } else if (error_code == 404) {
                user_friendly_msg = "❌ Модель или endpoint не найден\n\nПроверьте URL и название модели.";
            } else if (error_code >= 500) {
                user_friendly_msg = "🔧 Ошибка сервера (" + std::to_string(error_code) + ")\n\nПроблема на стороне провайдера.";
            } else {
                user_friendly_msg = "⚠️ Ошибка API (код: " + std::to_string(error_code) + ")\n\n" + error_msg;
            }

            response.error = user_friendly_msg;
            response.success = false;
            return response;
        }

        response.id = data.value("id", "");
        response.object = data.value("object", "");
        response.created = data.value("created", 0LL);
        response.model = data.value("model", "");
        response.choices_str = data.dump();
        response.success = true;

        // Парсинг choices
        if (data.contains("choices") && data["choices"].is_array()) {
            for (const auto& choice_json : data["choices"]) {
                UniversalOpenAICompletionResponse::Choice choice;
                choice.index = choice_json.value("index", 0);
                choice.finish_reason = choice_json.value("finish_reason", "");

                // Парсинг message
                if (choice_json.contains("message") && choice_json["message"].is_object()) {
                    const auto& msg_json = choice_json["message"];
                    choice.message.role = msg_json.value("role", "");
                    choice.message.content = msg_json.value("content", "");
                }

                response.choices.push_back(choice);
            }
        }

        // Парсинг usage
        if (data.contains("usage")) {
            const auto& usage = data["usage"];
            response.usage.prompt_tokens = usage.value("prompt_tokens", 0);
            response.usage.completion_tokens = usage.value("completion_tokens", 0);
            response.usage.total_tokens = usage.value("total_tokens", 0);
        }

    } catch (const json::parse_error& e) {
        response.error = "❌ Ошибка парсинга ответа\n\nНе удалось обработать ответ от сервера.";
        response.success = false;
    } catch (const std::exception& e) {
        response.error = "❌ Внутренняя ошибка\n\nПроизошла непредвиденная ошибка.";
        response.success = false;
    }

    return response;
}

UniversalOpenAICompletionResponse UniversalOpenAIClient::complete(const UniversalOpenAIRequestParams& params) {
    json request_body;

    // Обязательные параметры
    request_body["model"] = params.model;
    request_body["messages"] = json::array();

    // Формирование сообщений
    for (const auto& msg : params.messages) {
        json message;
        message["role"] = msg.role;
        message["content"] = msg.content;
        request_body["messages"].push_back(message);
    }

    // Опциональные параметры
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

    // Дополнительные стоп-последовательности
    if (!params.stop.empty()) {
        request_body["stop"] = params.stop;
    }

    // Функции (опционально)
    if (!params.functions.empty()) {
        request_body["functions"] = params.functions;
    }

    // User ID для трекинга
    if (!params.user_id.empty()) {
        request_body["user"] = params.user_id;
    }

    request_body["stream"] = params.stream;

    std::string response_str = make_request(request_body.dump());
    return parse_completion_response(response_str);
}

// ============================================================================
// Потоковая генерация
// ============================================================================

UniversalOpenAIStreamChunk UniversalOpenAIClient::parse_stream_chunk(const std::string& line) const {
    UniversalOpenAIStreamChunk chunk;

    // Удаляем "data: " префикс
    std::string data;
    if (line.substr(0, 6) == "data: ") {
        data = line.substr(6);
    } else {
        data = line;
    }

    // Проверяем на [DONE]
    if (data == "[DONE]") {
        chunk.is_error = false;
        return chunk;
    }

    try {
        json data_json = json::parse(data);

        chunk.id = data_json.value("id", "");
        chunk.object = data_json.value("object", "");
        chunk.created = data_json.value("created", 0LL);
        chunk.model = data_json.value("model", "");
        chunk.choices_str = data_json.dump();

        // Парсинг choices
        if (data_json.contains("choices") && data_json["choices"].is_array()) {
            for (const auto& choice_json : data_json["choices"]) {
                UniversalOpenAIStreamChunk::Choice choice;
                choice.index = choice_json.value("index", 0);
                choice.finish_reason = choice_json.value("finish_reason", "");

                // Парсинг delta
                if (choice_json.contains("delta") && choice_json["delta"].is_object()) {
                    const auto& delta = choice_json["delta"];
                    choice.delta.role = delta.value("role", "");
                    choice.delta.content = delta.value("content", "");
                }

                chunk.choices.push_back(choice);
            }
        }

    } catch (const json::parse_error& e) {
        chunk.is_error = true;
        chunk.error = "Ошибка парсинга: " + std::string(e.what());
    } catch (const std::exception& e) {
        chunk.is_error = true;
        chunk.error = "Ошибка обработки: " + std::string(e.what());
    }

    return chunk;
}

bool UniversalOpenAIClient::complete_streaming_async(const UniversalOpenAIRequestParams& params, StreamCallback callback) {
    if (!callback) {
        return false;
    }

    std::thread([this, params, callback]() {
        std::string url = base_url_ + "/" + endpoint_;
        std::string body;

        // Формирование request body (без stream=true для синхронного запроса)
        json request_body;
        request_body["model"] = params.model;
        request_body["messages"] = json::array();

        for (const auto& msg : params.messages) {
            json message;
            message["role"] = msg.role;
            message["content"] = msg.content;
            request_body["messages"].push_back(message);
        }

        // Опциональные параметры
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

        // Дополнительные стоп-последовательности
        if (!params.stop.empty()) {
            request_body["stop"] = params.stop;
        }

        // Функции (опционально)
        if (!params.functions.empty()) {
            request_body["functions"] = params.functions;
        }

        // User ID для трекинга
        if (!params.user_id.empty()) {
            request_body["user"] = params.user_id;
        }

        body = request_body.dump();

        CURL* curl = curl_easy_init();
        if (!curl) {
            LOG_ERROR("UniversalOpenAI: Не удалось инициализировать CURL для потоковой передачи");
            callback({{}, {}, 0, "", {}, {}, true, "Не удалось инициализировать CURL"});
            return;
        }

        struct curl_slist* headers = nullptr;
        auto header_list = get_request_headers();
        for (const auto& h : header_list) {
            headers = curl_slist_append(headers, h.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout_ms_);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, streaming_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(new std::pair<std::string*, bool*>(new std::string(), new bool(false))));
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

        CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            LOG_ERROR("UniversalOpenAI: CURL ошибка: " + std::string(curl_easy_strerror(res)));
            callback({{}, {}, 0, "", {}, {}, true, "CURL ошибка: " + std::string(curl_easy_strerror(res))});
        }

        // Очистка
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        // Отправляем финальный чанк [DONE]
        callback({{}, {}, 0, "", {}, {}, false, ""});

    }).detach();

    return true;
}

// ============================================================================
// Проверка доступности
// ============================================================================

bool UniversalOpenAIClient::is_api_available() {
    std::string response = make_request("models");
    return !response.empty();
}

// ============================================================================
// Информация о лимитах
// ============================================================================

UniversalOpenAIRateLimit UniversalOpenAIClient::get_rate_limit() {
    UniversalOpenAIRateLimit limit;

    // TODO: Получить информацию о лимитах из заголовков или специального endpoint
    limit.limit = 50;
    limit.remaining_requests = 50;
    limit.total_requests = 0;
    limit.reset_time = "";
    limit.is_free_tier = true;

    return limit;
}

} // namespace core
} // namespace llama_gui
