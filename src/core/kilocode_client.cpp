#include "../include/core/kilocode_client.h"
#include "../include/core/logger.h"
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <thread>
#include <algorithm>
#include <iostream>
#include <sstream>

using namespace llama_gui::core;

namespace llama_gui {
namespace core {

using json = nlohmann::json;

// ============================================================================
// Конструктор/деструктор
// ============================================================================

KiloCodeClient::KiloCodeClient(const std::string& api_key)
    : api_key_(api_key) {
    std::cout << "[KiloCode Client] Инициализация с ключом: "
              << (api_key.empty() ? "НЕТ" : api_key.substr(0, 8) + "...") << std::endl;
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

KiloCodeClient::~KiloCodeClient() {
    curl_global_cleanup();
}

// ============================================================================
// Настройки
// ============================================================================

void KiloCodeClient::set_api_key(const std::string& api_key) {
    api_key_ = api_key;
}

void KiloCodeClient::set_timeout(int timeout_ms) {
    timeout_ms_ = timeout_ms;
}

void KiloCodeClient::use_proxy(bool enabled) {
    use_proxy_ = enabled;
    std::cout << "[KiloCode Client] Proxy " << (enabled ? "ENABLED" : "DISABLED")
              << " (" << proxy_url_ << ")" << std::endl;
}

void KiloCodeClient::set_proxy_url(const std::string& proxy_url) {
    proxy_url_ = proxy_url;
    std::cout << "[KiloCode Client] Proxy URL set to: " << proxy_url_ << std::endl;
}

void KiloCodeClient::set_base_url(const std::string& base_url) {
    if (base_url.empty()) {
        base_url_ = "https://api.kilo.ai/api/gateway";
    } else {
        base_url_ = base_url;
        if (!base_url_.empty() && base_url_.back() == '/') {
            base_url_.pop_back();
        }
    }
}

// ============================================================================
// HTTP запросы
// ============================================================================

std::string KiloCodeClient::build_url(const std::string& endpoint) const {
    return base_url_ + "/" + endpoint;
}

std::vector<std::string> KiloCodeClient::get_request_headers() const {
    std::vector<std::string> headers;
    headers.push_back("Content-Type: application/json");

    if (!api_key_.empty()) {
        headers.push_back("Authorization: Bearer " + api_key_);
    }

    return headers;
}

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

static size_t streaming_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    auto* stream_data = static_cast<std::pair<std::string*, bool*>*>(userp);

    if (!stream_data || !stream_data->first) {
        return 0;
    }

    stream_data->first->append(static_cast<char*>(contents), total_size);
    return total_size;
}

std::string KiloCodeClient::make_request(const std::string& endpoint, const std::string& body) {
    if (use_proxy_) {
        return make_request_with_proxy(endpoint, body);
    }

    std::string response;
    std::string url = build_url(endpoint);

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("KiloCode: Не удалось инициализировать CURL");
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
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    if (!body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());

        std::cout << "[KiloCode] POST " << url << std::endl;
        std::cout << "[KiloCode] Request body: " << body.substr(0, 200) << "..." << std::endl;
    }

    CURLcode res = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::string error_msg = curl_easy_strerror(res);
        LOG_ERROR("KiloCode: Ошибка запроса: " + error_msg);
        std::cerr << "[KiloCode] CURL Error: " << error_msg << std::endl;
        return "";
    }

    if (response_code != 200) {
        std::cerr << "[KiloCode] HTTP Error: " << response_code << std::endl;
        std::cerr << "[KiloCode] Response: " << response.substr(0, 500) << std::endl;
    }

    return response;
}

std::string KiloCodeClient::make_request_with_proxy(const std::string& endpoint, const std::string& body) {
    std::string response;
    std::string url = build_url(endpoint);

    CURL* curl = curl_easy_init();
    if (!curl) {
        LOG_ERROR("KiloCode: Не удалось инициализировать CURL");
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
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    // Настройка SOCKS5 прокси
    curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url_.c_str());

    std::cout << "[KiloCode] POST " << url << " через SOCKS5: " << proxy_url_ << std::endl;

    if (!body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    }

    CURLcode res = curl_easy_perform(curl);

    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        std::string error_msg = curl_easy_strerror(res);
        LOG_ERROR("KiloCode: Ошибка запроса через прокси: " + error_msg);
        std::cerr << "[KiloCode] CURL Proxy Error: " << error_msg << std::endl;
        return "";
    }

    if (response_code != 200) {
        std::cerr << "[KiloCode] HTTP Error: " << response_code << std::endl;
        std::cerr << "[KiloCode] Response: " << response.substr(0, 500) << std::endl;
    }

    return response;
}

// ============================================================================
// Парсинг моделей
// ============================================================================

KiloCodeModel KiloCodeClient::parse_model(const json& j) const {
    KiloCodeModel model;

    model.id = j.value("id", "");
    model.name = j.value("name", model.id);
    model.provider = j.value("provider", "Unknown");
    model.description = j.value("description", "");

    // Устойчивый парсинг числовых полей (могут быть строками)
    auto parse_int64 = [](const json& obj, const std::string& key, int64_t default_val) -> int64_t {
        if (!obj.contains(key)) return default_val;
        const auto& val = obj[key];
        if (val.is_number()) return val.get<int64_t>();
        if (val.is_string()) {
            try { return std::stoll(val.get<std::string>()); }
            catch (...) { return default_val; }
        }
        return default_val;
    };

    model.context_length = parse_int64(j, "context_length", 0);
    model.max_output_tokens = parse_int64(j, "max_output_tokens", 0);

    model.is_free = j.value("isFree", false) || j.value("is_free", false);

    // Цены — тоже могут быть строками
    auto parse_double = [](const json& obj, const std::string& key, double default_val) -> double {
        if (!obj.contains(key)) return default_val;
        const auto& val = obj[key];
        if (val.is_number()) return val.get<double>();
        if (val.is_string()) {
            try { return std::stod(val.get<std::string>()); }
            catch (...) { return default_val; }
        }
        return default_val;
    };

    if (j.contains("pricing")) {
        const auto& pricing = j["pricing"];
        model.prompt_price_usd_per_million = parse_double(pricing, "prompt", 0.0);
        model.completion_price_usd_per_million = parse_double(pricing, "completion", 0.0);
    }

    return model;
}

KiloCodeModelsResponse KiloCodeClient::parse_models_response(const std::string& json_str) {
    KiloCodeModelsResponse response;

    if (json_str.empty()) {
        response.error = "Пустой ответ от API";
        return response;
    }

    // Отладка: логировать первые 1000 символов ответа
    std::cout << "[KiloCode] Raw response (first 1000): " << json_str.substr(0, 1000) << std::endl;

    try {
        json data = json::parse(json_str);

        if (data.is_array()) {
            for (const auto& item : data) {
                response.models.push_back(parse_model(item));
            }
        } else if (data.contains("data")) {
            for (const auto& item : data["data"]) {
                response.models.push_back(parse_model(item));
            }
        } else if (data.contains("models")) {
            for (const auto& item : data["models"]) {
                response.models.push_back(parse_model(item));
            }
        } else {
            response.error = "Неизвестный формат ответа API";
            return response;
        }

        response.success = true;
    } catch (const json::parse_error& e) {
        response.error = std::string("Ошибка парсинга JSON: ") + e.what();
    } catch (const std::exception& e) {
        response.error = std::string("Ошибка: ") + e.what();
    }

    return response;
}

// ============================================================================
// Получение моделей
// ============================================================================

KiloCodeModelsResponse KiloCodeClient::get_models() {
    std::string response = make_request("models", "");
    return parse_models_response(response);
}

void KiloCodeClient::get_models_async(std::function<void(const KiloCodeModelsResponse&)> callback) {
    if (!callback) return;

    std::thread([this, callback]() {
        auto response = get_models();
        callback(response);
    }).detach();
}

KiloCodeModelsResponse KiloCodeClient::get_free_models() {
    auto response = get_models();

    if (response.success) {
        std::vector<KiloCodeModel> free_models;
        for (const auto& model : response.models) {
            if (model.is_free) {
                free_models.push_back(model);
            }
        }
        response.models = free_models;
    }

    return response;
}

void KiloCodeClient::get_free_models_async(std::function<void(const KiloCodeModelsResponse&)> callback) {
    if (!callback) return;

    std::thread([this, callback]() {
        auto response = get_free_models();
        callback(response);
    }).detach();
}

// ============================================================================
// Поиск моделей
// ============================================================================

void KiloCodeClient::search_models_async(
    const std::string& query,
    bool free_only,
    std::function<void(const KiloCodeModelsResponse&)> callback
) {
    if (!callback) return;

    std::thread([this, query, free_only, callback]() {
        auto response = get_models();

        if (response.success) {
            std::vector<KiloCodeModel> filtered;
            std::string query_lower = query;
            std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

            for (const auto& model : response.models) {
                if (free_only && !model.is_free) continue;

                if (!query.empty()) {
                    std::string name_lower = model.name;
                    std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
                    std::string id_lower = model.id;
                    std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);

                    if (name_lower.find(query_lower) == std::string::npos &&
                        id_lower.find(query_lower) == std::string::npos) {
                        continue;
                    }
                }

                filtered.push_back(model);
            }
            response.models = filtered;
        }

        callback(response);
    }).detach();
}

// ============================================================================
// Генерация текста
// ============================================================================

KiloCodeCompletionResponse KiloCodeClient::parse_completion_response(const std::string& json_str) {
    KiloCodeCompletionResponse response;

    if (json_str.empty()) {
        response.error = "Пустой ответ от API";
        return response;
    }

    try {
        json data = json::parse(json_str);

        if (data.contains("error")) {
            response.error = data["error"].value("message", "Неизвестная ошибка API");
            return response;
        }

        if (data.contains("choices")) {
            const auto& choice = data["choices"][0];
            if (choice.contains("message")) {
                response.content = choice["message"].value("content", "");
            } else if (choice.contains("text")) {
                response.content = choice["text"].get<std::string>();
            }
            response.finish_reason = choice.value("finish_reason", "");
        }

        if (data.contains("usage")) {
            const auto& usage = data["usage"];
            response.prompt_tokens = usage.value("prompt_tokens", 0);
            response.completion_tokens = usage.value("completion_tokens", 0);
            response.total_tokens = usage.value("total_tokens", 0);
        }

        response.id = data.value("id", "");
        response.model = data.value("model", "");
        response.success = true;
    } catch (const json::parse_error& e) {
        response.error = std::string("Ошибка парсинга JSON: ") + e.what();
    } catch (const std::exception& e) {
        response.error = std::string("Ошибка: ") + e.what();
    }

    return response;
}

KiloCodeCompletionResponse KiloCodeClient::complete(const KiloCodeRequestParams& params) {
    json request_body;
    request_body["model"] = params.model;
    request_body["max_tokens"] = params.max_tokens;
    request_body["temperature"] = params.temperature;
    request_body["top_p"] = params.top_p;
    request_body["stream"] = params.stream;

    if (!params.messages.empty()) {
        json messages = json::array();
        for (const auto& msg : params.messages) {
            messages.push_back({
                {"role", msg.role},
                {"content", msg.content}
            });
        }
        request_body["messages"] = messages;
    } else if (!params.prompt.empty()) {
        request_body["prompt"] = params.prompt;
    }

    std::string response = make_request("chat/completions", request_body.dump());
    return parse_completion_response(response);
}

void KiloCodeClient::complete_async(
    const KiloCodeRequestParams& params,
    std::function<void(const KiloCodeCompletionResponse&)> callback
) {
    if (!callback) return;

    std::thread([this, params, callback]() {
        auto response = complete(params);
        callback(response);
    }).detach();
}

void KiloCodeClient::complete_streaming_async(
    const KiloCodeRequestParams& params,
    std::function<void(const std::string&)> chunk_callback,
    std::function<void(const KiloCodeCompletionResponse&)> complete_callback
) {
    if (!chunk_callback || !complete_callback) return;

    std::thread([this, params, chunk_callback, complete_callback]() {
        KiloCodeRequestParams stream_params = params;
        stream_params.stream = true;

        json request_body;
        request_body["model"] = stream_params.model;
        request_body["max_tokens"] = stream_params.max_tokens;
        request_body["temperature"] = stream_params.temperature;
        request_body["top_p"] = stream_params.top_p;
        request_body["stream"] = true;

        if (!stream_params.messages.empty()) {
            json messages = json::array();
            for (const auto& msg : stream_params.messages) {
                messages.push_back({
                    {"role", msg.role},
                    {"content", msg.content}
                });
            }
            request_body["messages"] = messages;
        }

        std::string response;
        std::string url = build_url("chat/completions");

        CURL* curl = curl_easy_init();
        if (!curl) {
            KiloCodeCompletionResponse error_resp;
            error_resp.error = "Не удалось инициализировать CURL";
            complete_callback(error_resp);
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
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

        if (use_proxy_) {
            curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
            curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url_.c_str());
        }

        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request_body.dump().c_str());

        CURLcode res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            KiloCodeCompletionResponse error_resp;
            error_resp.error = curl_easy_strerror(res);
            complete_callback(error_resp);
            return;
        }

        // Парсинг SSE stream
        std::istringstream stream(response);
        std::string line;
        std::string full_content;

        while (std::getline(stream, line)) {
            if (line.find("data: ") == 0) {
                std::string data = line.substr(6);
                if (data == "[DONE]") break;

                try {
                    json chunk = json::parse(data);
                    if (chunk.contains("choices")) {
                        const auto& delta = chunk["choices"][0].value("delta", json::object());
                        if (delta.contains("content") && !delta["content"].is_null()) {
                            std::string content = delta["content"].get<std::string>();
                            full_content += content;
                            chunk_callback(content);
                        }
                    }
                } catch (...) {
                    // Игнорируем ошибки парсинга отдельных чанков
                }
            }
        }

        KiloCodeCompletionResponse final_resp;
        final_resp.content = full_content;
        final_resp.success = true;
        complete_callback(final_resp);
    }).detach();
}

// ============================================================================
// Проверка доступности
// ============================================================================

bool KiloCodeClient::is_api_available() {
    auto response = get_models();
    return response.success && !response.models.empty();
}

KiloCodeRateLimit KiloCodeClient::get_rate_limit() {
    // KiloCode API не предоставляет отдельного endpoint для rate limit
    // Возвращаем заглушку
    KiloCodeRateLimit limit;
    limit.is_free_tier = true;
    return limit;
}

bool KiloCodeClient::is_proxy_available() {
    if (!use_proxy_) return true;

    CURL* curl = curl_easy_init();
    if (!curl) return false;

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.kilo.ai/api/gateway/models");
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 5000);
    curl_easy_setopt(curl, CURLOPT_PROXYTYPE, CURLPROXY_SOCKS5);
    curl_easy_setopt(curl, CURLOPT_PROXY, proxy_url_.c_str());

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    return res == CURLE_OK;
}

} // namespace core
} // namespace llama_gui
