// ============================================================================
// УЛУЧШЕНИЯ В openrouter_client.cpp
// ============================================================================

// -----------------------------------------------------------------------------
// 1. Улучшение метода is_api_available() - строка ~710
// -----------------------------------------------------------------------------

bool OpenRouterClient::is_api_available() {
    std::cout << "[OPENROUTER] Checking API availability via GET /api/v1/models..." << std::endl;
    
    // Проверка через GET /api/v1/models - НЕ тратит лимиты генерации!
    std::string response = make_request("models");
    
    if (response.empty()) {
        std::cerr << "[OPENROUTER] API check failed: empty response" << std::endl;
        return false;
    }
    
    // Проверяем что это валидный JSON с моделями
    try {
        json data = json::parse(response);
        if (data.contains("data") && data["data"].is_array()) {
            std::cout << "[OPENROUTER] ✅ API is available, " << data["data"].size() << " models found" << std::endl;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "[OPENROUTER] API check failed: invalid JSON" << std::endl;
        return false;
    }
    
    return false;
}

// -----------------------------------------------------------------------------
// 2. Улучшение метода get_rate_limit() - парсинг ответа (строка ~760)
// -----------------------------------------------------------------------------

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

            // Парсим информацию о лимитах - УЛУЧШЕННЫЙ ПАРСИНГ
            if (data.contains("data")) {
                const auto& key_data = data["data"];

                // Лимиты - проверяем несколько форматов
                if (key_data.contains("usage")) {
                    const auto& usage = key_data["usage"];
                    if (usage.is_object()) {
                        if (usage.contains("requests")) {
                            rate_limit_cache_.total_requests = usage.value("requests", 0);
                        }
                        // Некоторые API возвращают remaining напрямую
                        if (usage.contains("remaining")) {
                            rate_limit_cache_.remaining_requests = usage.value("remaining", 0);
                        }
                    }
                }

                // Остаток (вычисляем если не получен напрямую)
                if (key_data.contains("limit")) {
                    rate_limit_cache_.limit = key_data.value("limit", 50);
                    // Пересчитываем остаток если не получен напрямую
                    if (rate_limit_cache_.remaining_requests == 0) {
                        rate_limit_cache_.remaining_requests =
                            std::max(0, rate_limit_cache_.limit - rate_limit_cache_.total_requests);
                    }
                }

                // Бесплатный ли тариф
                if (key_data.contains("is_free")) {
                    rate_limit_cache_.is_free_tier = key_data.value("is_free", true);
                }
                
                // Время сброса
                if (key_data.contains("reset_at")) {
                    rate_limit_cache_.reset_time = key_data.value("reset_at", "");
                }
            }
            
            std::cout << "[RATE LIMIT] Updated cache: " << rate_limit_cache_.remaining_requests 
                      << "/" << rate_limit_cache_.limit << " requests remaining" << std::endl;
        } catch (...) {
            // Ошибка парсинга - возвращаем кэш
        }
    }

    rate_limit_timestamp_ = now;
    return rate_limit_cache_;
}
