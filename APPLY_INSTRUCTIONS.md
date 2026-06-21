# Инструкция по применению улучшений OpenRouter RAG Deep Analysis

## Быстрый старт

Этот документ содержит пошаговую инструкцию по внедрению всех улучшений.

---

## Шаг 1: Резервное копирование

```bash
cd /home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-gui

# Создаём резервные копии
cp include/core/rag_manager.h include/core/rag_manager.h.bak
cp src/core/rag_manager_deep_analysis.cpp src/core/rag_manager_deep_analysis.cpp.bak
cp src/core/openrouter_client.cpp src/core/openrouter_client.cpp.bak
```

---

## Шаг 2: Обновление заголовочного файла

**Файл:** `include/core/rag_manager.h`

**Действие:** Добавить объявления методов в public секцию класса RagManager

**Где:** После строки ~160 (после объявления `synthesize_final_answer_openrouter`)

**Что добавить:** (см. `patches/rag_manager_header_patch.h`)

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

## Шаг 3: Обновление openrouter_client.cpp

**Файл:** `src/core/openrouter_client.cpp`

### 3.1: Улучшение is_api_available()

**Где:** Строка ~710

**Найти:**
```cpp
bool OpenRouterClient::is_api_available() {
    std::string response = make_request("models");
    return !response.empty();
}
```

**Заменить на:** (см. `patches/openrouter_client_improvements.cpp`)
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

### 3.2: Улучшение get_rate_limit()

**Где:** Строка ~760 (внутри метода get_rate_limit)

**Найти:** Блок парсинга JSON (if (res == CURLE_OK && response_code == 200 && !response.empty()))

**Заменить на:** (см. `patches/openrouter_client_improvements.cpp`)

Улучшенный парсинг с поддержкой различных форматов ответа API.

---

## Шаг 4: Обновление rag_manager_deep_analysis.cpp

**Файл:** `src/core/rag_manager_deep_analysis.cpp`

### 4.1: Добавление новых методов

**Где:** Перед методом `generate_chunk_summary()` (строка ~530)

**Добавить:** Все методы из `patches/rag_manager_fallback_methods.cpp`

Методы для добавления:
1. `check_openrouter_availability()` - проверка доступности API
2. `check_openrouter_rate_limit()` - проверка лимитов
3. `generate_chunk_summary_with_fallback()` - основной метод с fallback
4. `generate_chunk_summary_local()` - локальный сервер

### 4.2: Изменение generate_chunk_summary()

**Где:** Строка ~530

**Найти:**
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

### 4.3: Увеличение таймаута в process_deep_analysis_mapreduce()

**Где:** Строка ~240 (начало метода)

**Найти:**
```cpp
std::string RagManager::process_deep_analysis_mapreduce(
    const std::string& query,
    std::vector<RagChunk>& all_chunks,
    const DeepAnalysisSettings& settings)
{
    std::cout << "\n[MAP-REDUCE] === PHASE 1: MAP (Creating batch summaries) ===" << std::endl;
```

**Заменить на:** (см. `patches/mapreduce_timeout_patch.cpp`)

Добавить код увеличения таймаута до 120 секунд для Map-этапа с RAII восстановлением.

### 4.4: Увеличение таймаута в synthesize_final_answer_openrouter()

**Где:** Строка ~860 (начало метода)

**Найти:**
```cpp
std::string RagManager::synthesize_final_answer_openrouter(
    const std::string& query,
    const std::vector<std::string>& summaries,
    int target_context_size)
{
    std::cout << "[SYNTHESIS] Using OpenRouter for final synthesis from " << summaries.size()
              << " summaries" << std::endl;
```

**Заменить на:** (см. `patches/reduce_timeout_patch.cpp`)

Добавить код увеличения таймаута до 180 секунд для Reduce-этапа с RAII восстановлением.

### 4.5: Выбор модели для Map-этапа

**Файл:** `src/core/rag_manager_deep_analysis.cpp`

**Где:** Метод `generate_chunk_summary_openrouter()`, строка ~630

**Найти:**
```cpp
    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
```

**Заменить на:** (см. `patches/map_model_selection_patch.cpp`)
```cpp
    // === ВЫБОР МОДЕЛИ ДЛЯ MAP-ЭТАПА ===
    DeepAnalysisSettings default_settings;
    std::string model_id = select_model_for_stage(RagAnalysisStage::Map, default_settings);
    
    std::cout << "[MODEL SELECT] Map stage using model: " << model_id << std::endl;

    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = model_id;  // Используем выбранную модель
```

### 4.6: Выбор модели для Reduce-этапа

**Файл:** `src/core/rag_manager_deep_analysis.cpp`

**Где:** Метод `synthesize_final_answer_openrouter()`, строка ~870

**Найти:**
```cpp
    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
```

**Заменить на:** (см. `patches/reduce_model_selection_patch.cpp`)
```cpp
    // === ВЫБОР МОДЕЛИ ДЛЯ REDUCE-ЭТАПА ===
    DeepAnalysisSettings default_settings;
    std::string model_id = select_model_for_stage(RagAnalysisStage::Reduce, default_settings);
    
    std::cout << "[MODEL SELECT] Reduce stage using model: " << model_id << std::endl;

    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = model_id;  // Используем выбранную модель
```

---

## Шаг 5: Сборка и тестирование

```bash
# Сборка проекта
cd /home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-gui
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Запуск тестов
./llama-gui
```

---

## Шаг 6: Проверка улучшений

### Тест 1: Нормальная работа OpenRouter

```
Ожидается в логе:
✅ [OPENROUTER CHECK] API is available
✅ [RATE LIMIT CHECK] Rate limit check passed
✅ [MODEL SELECT] Map stage using model: google/gemma-2-9b-it:free
✅ [SUMMARY] OpenRouter summary: 215 chars (model: google/gemma-2-9b-it:free)
✅ [MODEL SELECT] Reduce stage using model: meta-llama/llama-3-8b-instruct:free
✅ [SYNTHESIS] OpenRouter final answer: ... (model: meta-llama/llama-3-8b-instruct:free)
```

### Тест 2: Fallback при недоступности API

```
Ожидается в логе:
✅ [OPENROUTER CHECK] ❌ API is NOT available
✅ [FALLBACK] OpenRouter unavailable, switching to local server
✅ [SUMMARY] Using LOCAL server for summary
```

### Тест 3: Fallback при исчерпании лимита

```
Ожидается в логе:
✅ [RATE LIMIT CHECK] ❌ Insufficient requests
✅ [FALLBACK] Rate limit exceeded, switching to local server
```

### Тест 4: Retry при таймауте

```
Ожидается в логе:
✅ [FALLBACK] Timeout, retrying (1/1)...
✅ [SUMMARY] OpenRouter summary: ... (успех)
```

---

## Альтернативный вариант: Автоматическое применение патчей

Если у вас установлен `patch`:

```bash
cd /home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-gui

# Применить патчи (требуется ручная проверка!)
patch -p1 < patches/rag_manager_header_patch.h
patch -p1 < patches/openrouter_client_improvements.cpp
patch -p1 < patches/rag_manager_fallback_methods.cpp
patch -p1 < patches/mapreduce_timeout_patch.cpp
patch -p1 < patches/reduce_timeout_patch.cpp
patch -p1 < patches/map_model_selection_patch.cpp
patch -p1 < patches/reduce_model_selection_patch.cpp
```

---

## Откат изменений

Если что-то пошло не так:

```bash
cd /home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-gui

# Восстановление из резервных копий
cp include/core/rag_manager.h.bak include/core/rag_manager.h
cp src/core/rag_manager_deep_analysis.cpp.bak src/core/rag_manager_deep_analysis.cpp
cp src/core/openrouter_client.cpp.bak src/core/openrouter_client.cpp

# Пересборка
cd build
make clean
make -j$(nproc)
```

---

## Проверка критериев приёмки

| Критерий | Как проверить |
|----------|---------------|
| ✅ Проверка API перед MapReduce | Лог `[OPENROUTER CHECK]` |
| ✅ Проверка лимитов | Лог `[RATE LIMIT CHECK]` |
| ✅ Таймауты 120с/180с | Лог `[MAP-REDUCE] Timeout set to 120s` |
| ✅ Логирование модели | Лог `(model: ...)` в summary |
| ✅ Fallback на локальный | Лог `[FALLBACK] Switching to local server` |
| ✅ Разные модели Map/Reduce | Лог `[MODEL SELECT] Map/Reduce stage` |
| ✅ Retry при таймауте | Лог `[FALLBACK] Timeout, retrying` |

---

## Дополнительные файлы

- `IMPROVEMENTS_PLAN.md` - Полный план с обоснованием
- `patches/*.cpp` - Готовые фрагменты кода
- `patches/*.h` - Изменения заголовков

---

## Поддержка

При возникновении проблем:
1. Проверьте логи компиляции на ошибки
2. Убедитесь что все методы объявлены в `.h` файле
3. Проверьте что `#include <thread>` присутствует в `rag_manager_deep_analysis.cpp`
4. Убедитесь что `DeepAnalysisSettings` импортирован корректно
