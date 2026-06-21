// ============================================================================
// ПРОВЕРКА ДОСТУПНОСТИ OPENROUTER API (НЕ тратит лимиты!)
// ============================================================================

bool RagManager::check_openrouter_availability() {
    if (!openrouter_client_) {
        std::cerr << "[OPENROUTER CHECK] Client not initialized" << std::endl;
        return false;
    }

    std::cout << "[OPENROUTER CHECK] Checking API availability..." << std::endl;

    // Проверка через GET /api/v1/models - НЕ тратит лимиты генерации!
    bool available = openrouter_client_->is_api_available();

    if (available) {
        std::cout << "[OPENROUTER CHECK] ✅ API is available" << std::endl;
    } else {
        std::cerr << "[OPENROUTER CHECK] ❌ API is NOT available" << std::endl;
    }

    return available;
}

// ============================================================================
// ПРОВЕРКА ЛИМИТОВ OPENROUTER (НЕ тратит лимиты!)
// ============================================================================

bool RagManager::check_openrouter_rate_limit(int required_requests) {
    if (!openrouter_client_) {
        return false;
    }

    std::cout << "[RATE LIMIT CHECK] Checking OpenRouter rate limits..." << std::endl;

    // Получаем информацию о лимитах через GET /api/v1/auth/key
    OpenRouterRateLimit rate_limit = openrouter_client_->get_rate_limit();

    std::cout << "[RATE LIMIT CHECK] Limit: " << rate_limit.limit
              << ", Used: " << rate_limit.total_requests
              << ", Remaining: " << rate_limit.remaining_requests
              << " (Free tier: " << (rate_limit.is_free_tier ? "yes" : "no") << ")"
              << std::endl;

    // Проверяем, достаточно ли запросов
    if (rate_limit.remaining_requests < required_requests) {
        std::cerr << "[RATE LIMIT CHECK] ❌ Insufficient requests: need "
                  << required_requests << ", have " << rate_limit.remaining_requests
                  << std::endl;
        return false;
    }

    // Дополнительная проверка: если осталось мало запросов (< 10)
    if (rate_limit.remaining_requests < 10) {
        std::cout << "[RATE LIMIT CHECK] ⚠️  Warning: Low requests remaining ("
                  << rate_limit.remaining_requests << ")" << std::endl;
    } else {
        std::cout << "[RATE LIMIT CHECK] ✅ Rate limit check passed" << std::endl;
    }

    return true;
}

// ============================================================================
// FALLBACK НА ЛОКАЛЬНЫЙ СЕРВЕР ПРИ ОШИБКАХ OPENROUTER
// ============================================================================

/**
 * @brief Попытка выполнения запроса к OpenRouter с fallback на локальный сервер
 * @param chunk Чанк для суммаризации
 * @param query Исходный запрос
 * @param use_openrouter Флаг использования OpenRouter
 * @return Текст резюме
 */
std::string RagManager::generate_chunk_summary_with_fallback(
    const RagChunk& chunk, 
    const std::string& query)
{
    // Если OpenRouter не включен, сразу используем локальный сервер
    if (!use_openrouter_for_rag_ || !openrouter_client_) {
        return generate_chunk_summary_local(chunk, query);
    }

    // === ПРОВЕРКА ДОСТУПНОСТИ API ===
    if (!check_openrouter_availability()) {
        std::cerr << "[FALLBACK] OpenRouter unavailable, switching to local server" << std::endl;
        return generate_chunk_summary_local(chunk, query);
    }

    // === ПРОВЕРКА ЛИМИТОВ ===
    // Для Map-этапа нужно примерно: количество батчей + 1 Reduce
    if (!check_openrouter_rate_limit(10)) {
        std::cerr << "[FALLBACK] Rate limit exceeded, switching to local server" << std::endl;
        return generate_chunk_summary_local(chunk, query);
    }

    // === ПОПЫТКА OPENROUTER С RETRY ===
    int retry_count = 0;
    const int max_retries = 1;  // 1 повтор при таймауте

    while (retry_count <= max_retries) {
        try {
            std::cout << "[FALLBACK] Attempt " << (retry_count + 1) 
                      << " to OpenRouter (chunk " << chunk.chunk_index << ")" << std::endl;

            // Формируем запрос к OpenRouter
            std::string system_prompt =
                "Ты - ассистент для анализа документов. Твоя задача - создавать краткие резюме текстов.\n"
                "Создавай резюме (3-5 предложений), которое содержит ключевую информацию и фокусируется на аспектах, релевантных вопросу пользователя.\n"
                "Сохраняй важные факты, цифры, имена. Игнорируй второстепенные детали.\n"
                "Пиши только резюме, без вводных фраз.";

            std::string user_message =
                "Текст для анализа:\n" + chunk.content + "\n\n"
                "Вопрос пользователя: " + query + "\n\n"
                "Создай краткое резюме текста, релевантное вопросу.";

            OpenRouterRequestParams params;
            params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
            params.max_tokens = 512;
            params.temperature = 0.3f;
            params.top_p = 0.9f;
            params.stream = false;

            params.messages.push_back({"system", system_prompt});
            params.messages.push_back({"user", user_message});

            // Выполняем запрос
            auto response = openrouter_client_->complete(params);

            if (response.success && !response.content.empty()) {
                std::string summary = trim_summary(response.content);

                // ✅ ЛОГИРОВАНИЕ ФАКТИЧЕСКОЙ МОДЕЛИ
                std::cout << "[SUMMARY] OpenRouter summary: " << summary.size() << " chars"
                          << " (~" << estimate_tokens(summary) << " tokens)"
                          << " (model: " << response.model << ")" << std::endl;

                return summary;
            }

            // Обработка ошибок API
            if (response.error.find("Rate Limit") != std::string::npos ||
                response.error.find("429") != std::string::npos) {
                std::cerr << "[FALLBACK] Rate limit exceeded, switching to local server" << std::endl;
                return generate_chunk_summary_local(chunk, query);
            }

            if (response.error.find("400") != std::string::npos ||
                response.error.find("401") != std::string::npos ||
                response.error.find("403") != std::string::npos ||
                response.error.find("500") != std::string::npos) {
                std::cerr << "[FALLBACK] API error (" << response.error.substr(0, 50) << "), switching to local server" << std::endl;
                return generate_chunk_summary_local(chunk, query);
            }

            // Таймаут - пробуем retry
            if (response.error.find("таймаут") != std::string::npos ||
                response.error.find("timeout") != std::string::npos ||
                response.error.find("CURL") != std::string::npos) {
                
                retry_count++;
                if (retry_count <= max_retries) {
                    std::cout << "[FALLBACK] Timeout, retrying (" << retry_count << "/" << max_retries << ")..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    continue;
                }
            }

            // Другие ошибки - сразу fallback
            std::cerr << "[FALLBACK] OpenRouter error: " << response.error << std::endl;
            break;

        } catch (const std::exception& e) {
            std::cerr << "[FALLBACK] Exception: " << e.what() << std::endl;
            retry_count++;
            if (retry_count > max_retries) {
                break;
            }
        }
    }

    // === FALLBACK НА ЛОКАЛЬНЫЙ СЕРВЕР ===
    std::cout << "[FALLBACK] Switching to local server after " << retry_count << " failed attempts" << std::endl;
    return generate_chunk_summary_local(chunk, query);
}

/**
 * @brief Генерация резюме через локальный сервер (старый код)
 * @param chunk Чанк для суммаризации
 * @param query Исходный запрос
 * @return Текст резюме
 */
std::string RagManager::generate_chunk_summary_local(
    const RagChunk& chunk,
    const std::string& query)
{
    std::cout << "[SUMMARY] Using LOCAL server for summary (chunk " << chunk.chunk_index
              << ", doc: " << chunk.document_id << ")" << std::endl;

    std::string system_prompt =
        "Ты - ассистент для анализа документов. Твоя задача - создавать краткие резюме текстов.\n"
        "Создавай резюме (3-5 предложений), которое содержит ключевую информацию и фокусируется на аспектах, релевантных вопросу пользователя.\n"
        "Сохраняй важные факты, цифры, имена. Игнорируй второстепенные детали.\n"
        "Пиши только резюме, без вводных фраз.";

    std::string user_message =
        "Текст для анализа:\n" + chunk.content + "\n\n"
        "Вопрос пользователя: " + query + "\n\n"
        "Создай краткое резюме текста, релевантное вопросу.";

    if (!llama_interface_) {
        std::cerr << "[SUMMARY] Error: LlamaInterface not initialized" << std::endl;
        return "";
    }

    std::cout << "[SUMMARY] Sending summary request to local server..." << std::endl;

    ChatCompletionRequest request;
    request.max_tokens = 512;
    request.temperature = 0.3f;
    request.top_p = 0.9f;
    request.repeat_penalty = 1.0f;
    request.stream = false;

    ChatMessage system_msg(MessageRole::System, system_prompt);
    request.messages.push_back(system_msg);

    ChatMessage user_msg(MessageRole::User, user_message);
    request.messages.push_back(user_msg);

    try {
        auto future_response = llama_interface_->create_chat_completion_async(request);
        auto status = future_response.wait_for(std::chrono::seconds(90));

        if (status == std::future_status::ready) {
            auto response = future_response.get();

            if (!response.choices.empty() && !response.choices[0].message.content.empty()) {
                std::string summary = response.choices[0].message.content;
                summary = trim_summary(summary);

                std::cout << "[SUMMARY] Local server summary: " << summary.size() << " chars"
                          << " (~" << estimate_tokens(summary) << " tokens)" << std::endl;

                return summary;
            } else {
                std::cerr << "[SUMMARY] Error: Empty response from local server" << std::endl;
                return "[Пустое резюме]";
            }
        } else {
            std::cerr << "[SUMMARY] Error: Request timeout (90s)" << std::endl;
            return "[Таймаут генерации резюме]";
        }
    } catch (const std::exception& e) {
        std::cerr << "[SUMMARY] Error: Request failed: " << e.what() << std::endl;
        return "[Ошибка: " + std::string(e.what()) + "]";
    }
}
