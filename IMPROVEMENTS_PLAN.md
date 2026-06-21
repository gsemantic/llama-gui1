# План улучшений системы OpenRouter RAG Deep Analysis

## Обзор проблем и решений

| Проблема | Решение | Файлы | Статус |
|----------|---------|-------|--------|
| Таймауты 30 сек | Увеличить до 120с (Map), 180с (Reduce) | `openrouter_client.cpp`, `rag_manager_deep_analysis.cpp` | ⏳ |
| Ошибки API 400 | Fallback на локальный сервер | `rag_manager_deep_analysis.cpp` | ⏳ |
| Нет проверки API | Добавить проверку перед MapReduce | `rag_manager_deep_analysis.cpp` | ⏳ |
| Нет проверки лимитов | Проверка через `/api/v1/auth/key` | `rag_manager_deep_analysis.cpp` | ⏳ |
| Нет логирования модели | Логировать `response.model` | `rag_manager_deep_analysis.cpp` | ⏳ |
| Нет fallback | Реализовать fallback на локальный сервер | `rag_manager_deep_analysis.cpp` | ⏳ |
| Одна модель для Map/Reduce | Использовать `select_model_for_stage()` | `rag_manager_model_selection.cpp` | ⏳ |

---

## Улучшение 1: Проверка доступности OpenRouter API

**Цель:** Проверить доступность API перед запуском MapReduce (НЕ тратит лимиты!)

**Файл:** `src/core/rag_manager_deep_analysis.cpp`

### Код для добавления:

```cpp
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
```

**Место вставки:** Перед методом `process_deep_analysis_mapreduce()` (после строки ~230)

---

## Улучшение 2: Проверка лимитов перед MapReduce

**Цель:** Проверить лимиты бесплатных запросов перед запуском (НЕ тратит лимиты!)

**Файл:** `src/core/rag_manager_deep_analysis.cpp`

### Код для добавления:

```cpp
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
```

**Место вставки:** После метода `check_openrouter_availability()`

---

## Улучшение 3: Увеличение таймаута для Map и Reduce этапов

**Цель:** Увеличить таймаут с 30 секунд до 120с (Map) и 180с (Reduce)

**Файл:** `src/core/rag_manager_deep_analysis.cpp`

### Изменения в методе `process_deep_analysis_mapreduce`:

**Найти:** (строка ~240, начало метода `process_deep_analysis_mapreduce`)

```cpp
std::string RagManager::process_deep_analysis_mapreduce(
    const std::string& query,
    std::vector<RagChunk>& all_chunks,
    const DeepAnalysisSettings& settings)
{
    std::cout << "\n[MAP-REDUCE] === PHASE 1: MAP (Creating batch summaries) ===" << std::endl;
```

**Заменить на:**

```cpp
std::string RagManager::process_deep_analysis_mapreduce(
    const std::string& query,
    std::vector<RagChunk>& all_chunks,
    const DeepAnalysisSettings& settings)
{
    std::cout << "\n[MAP-REDUCE] === PHASE 1: MAP (Creating batch summaries) ===" << std::endl;

    // === УВЕЛИЧЕНИЕ ТАЙМАУТА ДЛЯ MAP-ЭТАПА (120 секунд) ===
    int original_timeout = 0;
    if (openrouter_client_) {
        original_timeout = 30000;  // Сохраняем оригинальный таймаут
        openrouter_client_->set_timeout(120000);  // 120 секунд для Map
        std::cout << "[MAP-REDUCE] Timeout set to 120s for MAP phase" << std::endl;
    }

    // Восстановление таймаута при выходе (RAII)
    class TimeoutRestorer {
    public:
        TimeoutRestorer(OpenRouterClient* client, int timeout) 
            : client_(client), timeout_(timeout) {}
        ~TimeoutRestorer() {
            if (client_ && timeout_ > 0) {
                client_->set_timeout(timeout_);
                std::cout << "[MAP-REDUCE] Timeout restored to " << timeout_ << "ms" << std::endl;
            }
        }
    private:
        OpenRouterClient* client_;
        int timeout_;
    };
    TimeoutRestorer restorer(openrouter_client_.get(), original_timeout);
```

### Изменения в методе `synthesize_final_answer_openrouter`:

**Найти:** (строка ~860, начало метода)

```cpp
std::string RagManager::synthesize_final_answer_openrouter(
    const std::string& query,
    const std::vector<std::string>& summaries,
    int target_context_size)
{
    std::cout << "[SYNTHESIS] Using OpenRouter for final synthesis from " << summaries.size()
              << " summaries" << std::endl;
```

**Заменить на:**

```cpp
std::string RagManager::synthesize_final_answer_openrouter(
    const std::string& query,
    const std::vector<std::string>& summaries,
    int target_context_size)
{
    std::cout << "[SYNTHESIS] Using OpenRouter for final synthesis from " << summaries.size()
              << " summaries" << std::endl;

    // === УВЕЛИЧЕНИЕ ТАЙМАУТА ДЛЯ REDUCE-ЭТАПА (180 секунд) ===
    int original_timeout = 0;
    if (openrouter_client_) {
        original_timeout = 30000;  // Сохраняем оригинальный таймаут
        openrouter_client_->set_timeout(180000);  // 180 секунд для Reduce
        std::cout << "[SYNTHESIS] Timeout set to 180s for REDUCE phase" << std::endl;
    }

    // Восстановление таймаута при выходе (RAII)
    class TimeoutRestorer {
    public:
        TimeoutRestorer(OpenRouterClient* client, int timeout) 
            : client_(client), timeout_(timeout) {}
        ~TimeoutRestorer() {
            if (client_ && timeout_ > 0) {
                client_->set_timeout(timeout_);
                std::cout << "[SYNTHESIS] Timeout restored to " << timeout_ << "ms" << std::endl;
            }
        }
    private:
        OpenRouterClient* client_;
        int timeout_;
    };
    TimeoutRestorer restorer(openrouter_client_.get(), original_timeout);
```

---

## Улучшение 4: Логирование фактической модели от OpenRouter

**Цель:** Логировать какую именно модель выбрал OpenRouter из response.model

**Файл:** `src/core/rag_manager_deep_analysis.cpp`

### Изменения в методе `generate_chunk_summary_openrouter`:

**Найти:** (строка ~660)

```cpp
            std::cout << "[SUMMARY] OpenRouter summary: " << summary.size() << " chars"
                      << " (~" << estimate_tokens(summary) << " tokens)"
                      << " (model: " << response.model << ")" << std::endl;
```

**Заменить на:** (уже реализовано ✅)

### Изменения в методе `synthesize_final_answer_openrouter`:

**Найти:** (строка ~920)

```cpp
            std::cout << "[SYNTHESIS] OpenRouter final answer: " << final_answer.size() << " chars"
                      << " (~" << estimate_tokens(final_answer) << " tokens)"
                      << " (model: " << response.model << ")" << std::endl;
```

**Заменить на:** (уже реализовано ✅)

---

## Улучшение 5: Fallback на локальный сервер при ошибках OpenRouter

**Цель:** При ошибках OpenRouter автоматически переключаться на локальный сервер

**Файл:** `src/core/rag_manager_deep_analysis.cpp`

### Код для добавления (вспомогательный метод):

```cpp
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
```

**Место вставки:** Перед методом `generate_chunk_summary()` (строка ~530)

### Изменения в методе `generate_chunk_summary`:

**Найти:** (строка ~530)

```cpp
std::string RagManager::generate_chunk_summary(
    const RagChunk& chunk,
    const std::string& query)
{
    // === ПРОВЕРКА: используем ли OpenRouter? ===
    if (use_openrouter_for_rag_ && openrouter_client_) {
        return generate_chunk_summary_openrouter(chunk, query);
    }
```

**Заменить на:**

```cpp
std::string RagManager::generate_chunk_summary(
    const RagChunk& chunk,
    const std::string& query)
{
    // === ИСПОЛЬЗУЕМ FALLBACK МЕТОД ===
    return generate_chunk_summary_with_fallback(chunk, query);
}
```

---

## Улучшение 6: Разные модели для Map и Reduce этапов

**Цель:** Использовать `select_model_for_stage()` для выбора разных моделей

**Файл:** `src/core/rag_manager_deep_analysis.cpp`

### Изменения в методе `generate_chunk_summary_openrouter`:

**Найти:** (строка ~630)

```cpp
    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
    params.max_tokens = 512;
```

**Заменить на:**

```cpp
    // === ВЫБОР МОДЕЛИ ДЛЯ MAP-ЭТАПА ===
    DeepAnalysisSettings default_settings;
    std::string model_id = select_model_for_stage(RagAnalysisStage::Map, default_settings);
    
    std::cout << "[MODEL SELECT] Map stage using model: " << model_id << std::endl;

    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = model_id;
    params.max_tokens = 512;
```

### Изменения в методе `synthesize_final_answer_openrouter`:

**Найти:** (строка ~870)

```cpp
    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
    params.max_tokens = std::min(target_context_size / 2, 1024);
```

**Заменить на:**

```cpp
    // === ВЫБОР МОДЕЛИ ДЛЯ REDUCE-ЭТАПА ===
    DeepAnalysisSettings default_settings;
    std::string model_id = select_model_for_stage(RagAnalysisStage::Reduce, default_settings);
    
    std::cout << "[MODEL SELECT] Reduce stage using model: " << model_id << std::endl;

    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = model_id;
    params.max_tokens = std::min(target_context_size / 2, 1024);
```

---

## Улучшение 7: Обновление заголовка rag_manager.h

**Файл:** `include/core/rag_manager.h`

### Код для добавления (в public секцию класса RagManager):

**Найти:** (после объявления `synthesize_final_answer_openrouter`, строка ~160)

**Добавить:**

```cpp
    /**
     * @brief Проверка доступности OpenRouter API (НЕ тратит лимиты!)
     * @return true если API доступен
     */
    bool check_openrouter_availability();

    /**
     * @brief Проверка лимитов OpenRouter перед MapReduce (НЕ тратит лимиты!)
     * @param required_requests Требуемое количество запросов
     * @return true если лимиты позволяют
     */
    bool check_openrouter_rate_limit(int required_requests);

    /**
     * @brief Генерация резюме с fallback на локальный сервер
     * @param chunk Чанк для суммаризации
     * @param query Исходный запрос
     * @return Текст резюме
     */
    std::string generate_chunk_summary_with_fallback(const RagChunk& chunk, const std::string& query);

    /**
     * @brief Генерация резюме через локальный сервер
     * @param chunk Чанк для суммаризации
     * @param query Исходный запрос
     * @return Текст резюме
     */
    std::string generate_chunk_summary_local(const RagChunk& chunk, const std::string& query);
```

---

## Улучшение 8: Обновление openrouter_client.cpp

**Файл:** `src/core/openrouter_client.cpp`

### Проверка метода `get_rate_limit()`:

Метод уже реализован (строки ~720-796), но нужно убедиться что он корректно парсит ответ API.

**Найти:** (строка ~760, парсинг ответа)

```cpp
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
```

**Заменить на:** (более надёжный парсинг)

```cpp
            // Парсим информацию о лимитах
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
```

---

## Проверка метода `is_api_available()`

**Файл:** `src/core/openrouter_client.cpp`

**Найти:** (строка ~710)

```cpp
bool OpenRouterClient::is_api_available() {
    std::string response = make_request("models");
    return !response.empty();
}
```

**Заменить на:** (более надёжная проверка)

```cpp
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
```

---

## Итоговый чеклист изменений

### Файл: `include/core/rag_manager.h`
- [ ] Добавить объявление `check_openrouter_availability()`
- [ ] Добавить объявление `check_openrouter_rate_limit()`
- [ ] Добавить объявление `generate_chunk_summary_with_fallback()`
- [ ] Добавить объявление `generate_chunk_summary_local()`

### Файл: `src/core/rag_manager_deep_analysis.cpp`
- [ ] Добавить метод `check_openrouter_availability()`
- [ ] Добавить метод `check_openrouter_rate_limit()`
- [ ] Добавить метод `generate_chunk_summary_with_fallback()`
- [ ] Добавить метод `generate_chunk_summary_local()`
- [ ] Изменить `generate_chunk_summary()` для использования fallback
- [ ] Добавить RAII класс `TimeoutRestorer` в `process_deep_analysis_mapreduce()`
- [ ] Добавить RAII класс `TimeoutRestorer` в `synthesize_final_answer_openrouter()`
- [ ] Изменить `generate_chunk_summary_openrouter()` для использования `select_model_for_stage(Map)`
- [ ] Изменить `synthesize_final_answer_openrouter()` для использования `select_model_for_stage(Reduce)`

### Файл: `src/core/openrouter_client.cpp`
- [ ] Улучшить `is_api_available()` с валидацией JSON
- [ ] Улучшить парсинг в `get_rate_limit()`

### Файл: `src/core/rag_manager_model_selection.cpp`
- [ ] Изменения не требуются (метод `select_model_for_stage()` уже готов)

---

## Критерии приёмки

| Критерий | Статус | Проверка |
|----------|--------|----------|
| ✅ Проверка API перед MapReduce (GET /api/v1/models) | ⏳ | Лог `[OPENROUTER CHECK] ✅ API is available` |
| ✅ Проверка лимитов (GET /api/v1/auth/key) | ⏳ | Лог `[RATE LIMIT CHECK] ✅ Rate limit check passed` |
| ✅ Увеличенные таймауты (120с Map, 180с Reduce) | ⏳ | Лог `[MAP-REDUCE] Timeout set to 120s for MAP phase` |
| ✅ Логирование фактической модели от OpenRouter | ⏳ | Лог `[SUMMARY] OpenRouter summary: ... (model: liquid/lfm-2.5-1.2b-instruct:free)` |
| ✅ Fallback на локальный сервер при ошибках | ⏳ | Лог `[FALLBACK] Switching to local server after N failed attempts` |
| ✅ Разные модели для Map и Reduce этапов | ⏳ | Лог `[MODEL SELECT] Map stage using model: ...` и `[MODEL SELECT] Reduce stage using model: ...` |
| ✅ Повтор запроса при таймауте (1 retry) | ⏳ | Лог `[FALLBACK] Timeout, retrying (1/1)...` |

---

## Порядок применения изменений

1. **Сначала** обновите `include/core/rag_manager.h` (добавить объявления методов)
2. **Затем** обновите `src/core/openrouter_client.cpp` (улучшить проверку API и лимитов)
3. **После** обновите `src/core/rag_manager_deep_analysis.cpp` (основные изменения)
4. **В конце** соберите проект и протестируйте

---

## Тестирование

### Сценарий 1: Нормальная работа OpenRouter
```
1. Включить OpenRouter в настройках
2. Запустить глубокий анализ документа
3. Ожидать логи:
   - [OPENROUTER CHECK] ✅ API is available
   - [RATE LIMIT CHECK] ✅ Rate limit check passed
   - [MODEL SELECT] Map stage using model: google/gemma-2-9b-it:free
   - [SUMMARY] OpenRouter summary: 215 chars (model: google/gemma-2-9b-it:free)
   - [MODEL SELECT] Reduce stage using model: meta-llama/llama-3-8b-instruct:free
   - [SYNTHESIS] OpenRouter final answer: ... (model: meta-llama/llama-3-8b-instruct:free)
```

### Сценарий 2: Fallback при недоступности API
```
1. Отключить интернет или заблокировать openrouter.ai
2. Запустить глубокий анализ
3. Ожидать логи:
   - [OPENROUTER CHECK] ❌ API is NOT available
   - [FALLBACK] OpenRouter unavailable, switching to local server
   - [SUMMARY] Using LOCAL server for summary
```

### Сценарий 3: Fallback при исчерпании лимита
```
1. Исчерпать дневной лимит бесплатных запросов
2. Запустить глубокий анализ
3. Ожидать логи:
   - [RATE LIMIT CHECK] ❌ Insufficient requests
   - [FALLBACK] Rate limit exceeded, switching to local server
```

### Сценарий 4: Retry при таймауте
```
1. Создать условия для таймаута (медленное соединение)
2. Запустить глубокий анализ
3. Ожидать логи:
   - [FALLBACK] Timeout, retrying (1/1)...
   - [SUMMARY] OpenRouter summary: ... (успех со второй попытки)
```

---

## Риски и mitigation

| Риск | Mitigation |
|------|------------|
| Ломается обратная совместимость | Сохранены старые методы `generate_chunk_summary_openrouter()` и `synthesize_final_answer_openrouter()` |
| Увеличивается время компиляции | Небольшое изменение, ~200 строк кода |
| Возможны утечки памяти | RAII класс `TimeoutRestorer` гарантирует восстановление таймаута |
| Ошибки парсинга API | Добавлена обработка исключений и fallback |

---

## Дополнительные рекомендации

1. **Логирование:** Добавить уровень логирования DEBUG для отладки
2. **Метрики:** Добавить счётчики успешных/неуспешных запросов к OpenRouter
3. **Кэширование:** Кэшировать результаты проверки API на 5 минут
4. **Конфигурация:** Вынести таймауты и количество retry в настройки
