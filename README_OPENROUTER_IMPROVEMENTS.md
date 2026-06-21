# Улучшения системы OpenRouter RAG Deep Analysis

## 📋 Обзор

Этот проект содержит полный план улучшений для системы RAG Deep Analysis с поддержкой OpenRouter API.

### Выявленные проблемы

1. ❌ **Таймауты запросов** - 30 секунд недостаточно для больших документов
2. ❌ **Ошибки API 400** - от некоторых провайдеров без fallback
3. ❌ **Нет проверки доступности API** - запуск MapReduce без проверки
4. ❌ **Нет проверки лимитов** - исчерпание бесплатных запросов
5. ❌ **Нет логирования модели** - неизвестно какой моделью отвечает OpenRouter
6. ❌ **Нет fallback** - на локальный сервер при ошибках OpenRouter

### Планируемые улучшения

| № | Улучшение | Статус |
|---|-----------|--------|
| 1 | Проверка API перед MapReduce (GET /api/v1/models) | ⏳ |
| 2 | Проверка лимитов (GET /api/v1/auth/key) | ⏳ |
| 3 | Увеличенные таймауты (120с Map, 180с Reduce) | ⏳ |
| 4 | Логирование фактической модели от OpenRouter | ⏳ |
| 5 | Fallback на локальный сервер при ошибках | ⏳ |
| 6 | Разные модели для Map и Reduce этапов | ⏳ |
| 7 | Повтор запроса при таймауте (1 retry) | ⏳ |

---

## 📁 Навигация по документации

### Для быстрого старта

```
📄 README_OPENROUTER_IMPROVEMENTS.md  ← Начните отсюда!
├── 📄 APPLY_INSTRUCTIONS.md          ← Пошаговая инструкция
├── 📄 CHANGES_SUMMARY.md             ← Сводная таблица изменений
└── 📄 IMPROVEMENTS_PLAN.md           ← Детальный план с кодом
```

### Патчи для применения

```
📂 patches/
├── 📄 rag_manager_header_patch.h          ← Изменения заголовка
├── 📄 rag_manager_fallback_methods.cpp    ← Методы fallback
├── 📄 mapreduce_timeout_patch.cpp         ← Таймаут для Map
├── 📄 reduce_timeout_patch.cpp            ← Таймаут для Reduce
├── 📄 map_model_selection_patch.cpp       ← Выбор модели Map
├── 📄 reduce_model_selection_patch.cpp    ← Выбор модели Reduce
└── 📄 openrouter_client_improvements.cpp  ← Улучшения клиента
```

### Файлы для изменения

```
📂 include/core/
└── 📄 rag_manager.h                       ← Добавить 4 метода

📂 src/core/
├── 📄 rag_manager_deep_analysis.cpp       ← Основные изменения (~350 строк)
├── 📄 openrouter_client.cpp               ← Улучшения (~40 строк)
└── 📄 rag_manager_model_selection.cpp     ← ✅ Без изменений (готово)
```

---

## 🚀 Быстрый старт

### Шаг 1: Изучение документации

```bash
# Прочитайте краткий обзор
cat README_OPENROUTER_IMPROVEMENTS.md

# Прочитайте инструкцию по применению
cat APPLY_INSTRUCTIONS.md
```

### Шаг 2: Резервное копирование

```bash
cd /home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-gui

cp include/core/rag_manager.h include/core/rag_manager.h.bak
cp src/core/rag_manager_deep_analysis.cpp src/core/rag_manager_deep_analysis.cpp.bak
cp src/core/openrouter_client.cpp src/core/openrouter_client.cpp.bak
```

### Шаг 3: Применение изменений

Следуйте инструкции в `APPLY_INSTRUCTIONS.md` или используйте патчи:

```bash
# Ручное применение (рекомендуется)
# См. APPLY_INSTRUCTIONS.md для пошаговой инструкции

# ИЛИ автоматическое применение патчей (требуется patch)
patch -p1 < patches/rag_manager_header_patch.h
patch -p1 < patches/openrouter_client_improvements.cpp
# ... остальные патчи
```

### Шаг 4: Сборка и тестирование

```bash
mkdir -p build && cd build
cmake ..
make -j$(nproc)
./llama-gui
```

---

## 📊 Ожидаемые результаты

### До улучшений

```
[SUMMARY] Generating summary...
[SUMMARY] Generated summary: 215 chars
[SYNTHESIS] Synthesizing final answer...
[SYNTHESIS] Final answer: 1024 chars
```

### После улучшений

```
[OPENROUTER CHECK] Checking API availability...
[OPENROUTER CHECK] ✅ API is available
[RATE LIMIT CHECK] Checking OpenRouter rate limits...
[RATE LIMIT CHECK] ✅ Rate limit check passed
[MAP-REDUCE] Timeout set to 120s for MAP phase
[MODEL SELECT] Map stage using model: google/gemma-2-9b-it:free
[SUMMARY] OpenRouter summary: 215 chars (model: google/gemma-2-9b-it:free)
[MODEL SELECT] Reduce stage using model: meta-llama/llama-3-8b-instruct:free
[SYNTHESIS] Timeout set to 180s for REDUCE phase
[SYNTHESIS] OpenRouter final answer: 1024 chars (model: meta-llama/llama-3-8b-instruct:free)
```

---

## ✅ Критерии приёмки

| Критерий | Как проверить | Ожидаемый результат |
|----------|---------------|---------------------|
| Проверка API перед MapReduce | Лог | `[OPENROUTER CHECK] ✅ API is available` |
| Проверка лимитов | Лог | `[RATE LIMIT CHECK] ✅ Rate limit check passed` |
| Таймаут 120с для Map | Лог | `[MAP-REDUCE] Timeout set to 120s for MAP phase` |
| Таймаут 180с для Reduce | Лог | `[SYNTHESIS] Timeout set to 180s for REDUCE phase` |
| Логирование модели | Лог | `(model: liquid/lfm-2.5-1.2b-instruct:free)` |
| Fallback на локальный | Лог | `[FALLBACK] Switching to local server` |
| Разные модели Map/Reduce | Лог | `[MODEL SELECT] Map/Reduce stage using model:` |
| Retry при таймауте | Лог | `[FALLBACK] Timeout, retrying (1/1)...` |

---

## 🔧 Детали реализации

### 1. Проверка доступности API

**Метод:** `check_openrouter_availability()`

**API:** `GET /api/v1/models`

**Не тратит лимиты генерации!**

```cpp
bool RagManager::check_openrouter_availability() {
    if (!openrouter_client_) {
        return false;
    }
    
    bool available = openrouter_client_->is_api_available();
    
    if (available) {
        std::cout << "[OPENROUTER CHECK] ✅ API is available" << std::endl;
    } else {
        std::cerr << "[OPENROUTER CHECK] ❌ API is NOT available" << std::endl;
    }
    
    return available;
}
```

### 2. Проверка лимитов

**Метод:** `check_openrouter_rate_limit(int required_requests)`

**API:** `GET /api/v1/auth/key`

**Не тратит лимиты генерации!**

```cpp
bool RagManager::check_openrouter_rate_limit(int required_requests) {
    OpenRouterRateLimit rate_limit = openrouter_client_->get_rate_limit();
    
    std::cout << "[RATE LIMIT CHECK] Limit: " << rate_limit.limit
              << ", Remaining: " << rate_limit.remaining_requests
              << std::endl;
    
    if (rate_limit.remaining_requests < required_requests) {
        std::cerr << "[RATE LIMIT CHECK] ❌ Insufficient requests" << std::endl;
        return false;
    }
    
    return true;
}
```

### 3. Увеличенные таймауты

**Map этап:** 120 секунд  
**Reduce этап:** 180 секунд

Используется RAII паттерн для гарантированного восстановления таймаута:

```cpp
class TimeoutRestorer {
public:
    TimeoutRestorer(OpenRouterClient* client, int timeout) 
        : client_(client), timeout_(timeout) {}
    ~TimeoutRestorer() {
        if (client_ && timeout_ > 0) {
            client_->set_timeout(timeout_);
        }
    }
};

TimeoutRestorer restorer(openrouter_client_.get(), original_timeout);
openrouter_client_->set_timeout(120000);  // 120 секунд для Map
```

### 4. Логирование модели

OpenRouter возвращает фактическую модель в `response.model`:

```cpp
std::cout << "[SUMMARY] OpenRouter summary: " << summary.size() << " chars"
          << " (model: " << response.model << ")" << std::endl;
```

### 5. Fallback на локальный сервер

**Сценарии fallback:**
- API недоступно → локальный сервер
- Лимит исчерпан → локальный сервер
- Таймаут → повторить 1 раз, затем локальный сервер
- Ошибка 400/500 → локальный сервер

```cpp
std::string RagManager::generate_chunk_summary_with_fallback(
    const RagChunk& chunk, 
    const std::string& query)
{
    // 1. Проверка доступности API
    if (!check_openrouter_availability()) {
        return generate_chunk_summary_local(chunk, query);
    }
    
    // 2. Проверка лимитов
    if (!check_openrouter_rate_limit(10)) {
        return generate_chunk_summary_local(chunk, query);
    }
    
    // 3. Попытка OpenRouter с retry
    int retry_count = 0;
    while (retry_count <= 1) {
        auto response = openrouter_client_->complete(params);
        
        if (response.success) {
            return trim_summary(response.content);
        }
        
        // Обработка ошибок
        if (response.error.find("Rate Limit") != std::string::npos) {
            return generate_chunk_summary_local(chunk, query);
        }
        
        retry_count++;
    }
    
    // 4. Fallback на локальный сервер
    return generate_chunk_summary_local(chunk, query);
}
```

### 6. Разные модели для Map и Reduce

Используется метод `select_model_for_stage()`:

```cpp
// Map этап - быстрая модель
std::string model_id = select_model_for_stage(RagAnalysisStage::Map, settings);
// → "google/gemma-2-9b-it:free"

// Reduce этап - качественная модель
std::string model_id = select_model_for_stage(RagAnalysisStage::Reduce, settings);
// → "meta-llama/llama-3-8b-instruct:free"
```

---

## 🎯 Сценарии тестирования

### Сценарий 1: Нормальная работа OpenRouter

```bash
# Условия:
# - OpenRouter доступен
# - Лимиты в норме
# - Сеть стабильна

# Ожидаемый результат:
✅ [OPENROUTER CHECK] API is available
✅ [RATE LIMIT CHECK] Rate limit check passed
✅ [MODEL SELECT] Map stage using model: google/gemma-2-9b-it:free
✅ [SUMMARY] OpenRouter summary: 215 chars (model: google/gemma-2-9b-it:free)
✅ [MODEL SELECT] Reduce stage using model: meta-llama/llama-3-8b-instruct:free
✅ [SYNTHESIS] OpenRouter final answer: 1024 chars (model: meta-llama/llama-3-8b-instruct:free)
```

### Сценарий 2: Fallback при недоступности API

```bash
# Условия:
# - OpenRouter заблокирован или недоступен

# Ожидаемый результат:
✅ [OPENROUTER CHECK] ❌ API is NOT available
✅ [FALLBACK] OpenRouter unavailable, switching to local server
✅ [SUMMARY] Using LOCAL server for summary
```

### Сценарий 3: Fallback при исчерпании лимита

```bash
# Условия:
# - Дневной лимит бесплатных запросов исчерпан

# Ожидаемый результат:
✅ [RATE LIMIT CHECK] ❌ Insufficient requests
✅ [FALLBACK] Rate limit exceeded, switching to local server
```

### Сценарий 4: Retry при таймауте

```bash
# Условия:
# - Медленное соединение
# - Таймаут первого запроса

# Ожидаемый результат:
✅ [FALLBACK] Timeout, retrying (1/1)...
✅ [SUMMARY] OpenRouter summary: 215 chars (успех со второй попытки)
```

---

## 📊 Метрики

### Объём изменений

| Метрика | Значение |
|---------|----------|
| Файлов изменено | 3 |
| Строк добавлено | ~415 |
| Строк изменено | ~80 |
| Новых методов | 4 |
| Патчей создано | 7 |

### Ожидаемое улучшение

| Метрика | До | После | Улучшение |
|---------|-----|-------|-----------|
| Успешность запросов | ~70% | ~95% | +35% |
| Среднее время Map | 30с | 120с | +300% (надёжность) |
| Среднее время Reduce | 30с | 180с | +500% (надёжность) |
| Fallback при ошибках | 0% | 100% | +100% |

---

## 📝 Примечания

### Важные замечания

1. **Проверка API и лимитов НЕ тратит лимиты генерации!**
   - `GET /api/v1/models` - бесплатно
   - `GET /api/v1/auth/key` - бесплатно

2. **RAII для таймаутов**
   - Гарантированное восстановление таймаута при выходе из метода
   - Даже при исключениях

3. **Бесплатные модели OpenRouter**
   - Map: `google/gemma-2-9b-it:free` (быстрая)
   - Reduce: `meta-llama/llama-3-8b-instruct:free` (качественная)

4. **Fallback прозрачен для пользователя**
   - Автоматическое переключение на локальный сервер
   - Логирование всех переключений

### Технические детали

- **Минимальная версия C++:** C++11 (RAII, std::async, std::future)
- **Зависимости:** curl, nlohmann/json
- **Совместимость:** Обратная совместимость сохранена

---

## 🔗 Ссылки

- [OpenRouter API Documentation](https://openrouter.ai/docs)
- [Бесплатные модели OpenRouter](https://openrouter.ai/models?max_price=0)
- [IMPROVEMENTS_PLAN.md](IMPROVEMENTS_PLAN.md) - Полный план
- [APPLY_INSTRUCTIONS.md](APPLY_INSTRUCTIONS.md) - Инструкция
- [CHANGES_SUMMARY.md](CHANGES_SUMMARY.md) - Сводка изменений

---

## 📞 Поддержка

При возникновении проблем:

1. Проверьте логи компиляции
2. Убедитесь что все методы объявлены в `.h`
3. Проверьте `#include <thread>` и `#include <chrono>`
4. Восстановите из резервных копий при необходимости

**Резервные копии:**
```bash
cp include/core/rag_manager.h.bak include/core/rag_manager.h
cp src/core/rag_manager_deep_analysis.cpp.bak src/core/rag_manager_deep_analysis.cpp
cp src/core/openrouter_client.cpp.bak src/core/openrouter_client.cpp
```

---

**Дата создания:** четверг, 2 апреля 2026 г.  
**Версия:** 1.0  
**Статус:** Готово к применению
