# Интеграция оптимизаций RAG и эмбеддинга

## 📦 Что входит в патч

### Новые файлы

1. **`include/ui/embedding_settings_dialog.h`** — Заголовочный файл диалога настроек эмбеддингов
2. **`src/ui/embedding_settings_dialog.cpp`** — Реализация диалога настроек эмбеддингов

### Изменённые файлы

1. **`include/core/embedding_generator.h`** — Обновлён интерфейс с поддержкой гибридного режима
2. **`src/core/embedding_generator.cpp`** — Полная реализация облачного и локального эмбеддинга
3. **`include/core/rag_settings.h`** — Оптимизированные настройки RAG
4. **`CMakeLists.txt`** — Добавлены новые файлы в сборку

### Документация

1. **`EMBEDDING_OPTIMIZATION_GUIDE.md`** — Подробное руководство по оптимизации
2. **`OPTIMIZATION_SUMMARY.md`** — Техническое резюме изменений
3. **`QUICK_START_RAG_OPTIMIZATION.md`** — Быстрый старт (5 минут)
4. **`INTEGRATION_NOTES.md`** — Этот файл

---

## 🔧 Как интегрировать в существующий проект

### Вариант 1: Полная интеграция (рекомендуется)

Если у вас проект `llama-gui`:

```bash
# Файлы уже на месте, просто пересоберите
cd build
make clean
cmake ..
make -j4
```

### Вариант 2: Частичная интеграция

Если нужны только изменения эмбеддинга без UI:

#### 1. Скопируйте файлы

```bash
# Основные файлы
cp include/core/embedding_generator.h /path/to/your/project/include/core/
cp src/core/embedding_generator.cpp /path/to/your/project/src/core/

# Настройки RAG
# Внесите изменения вручную из include/core/rag_settings.h
```

#### 2. Обновите `rag_settings.h`

Измените настройки вручную (см. раздел "Изменения в rag_settings.h" ниже)

#### 3. Обновите `CMakeLists.txt`

Добавьте в SOURCES:
```cmake
src/core/embedding_generator.cpp
```

---

## 📝 Изменения в rag_settings.h

### Найдите и замените

**БЫЛО:**
```cpp
struct RagSettings {
    std::string embedding_model_path = "";
    int max_chunks_in_memory = 100;
    float similarity_threshold = 0.70f;
    int max_embedding_cache_size = 50;
    int embedding_dimension = 384;
    int max_sequence_length = 512;
    int max_tokens_per_chunk = 256;
    int search_k = 5;
    float mmr_lambda = 0.5f;
    bool enable_mmr = false;
    bool enable_rag = true;
    bool enable_caching = true;
    bool enable_kv_cache = true;
    bool enable_hybrid_search = true;
    float keyword_boost_weight = 2.0f;
    bool enable_query_expansion = true;
    int max_rag_chunks = 20;
    // ...
};
```

**СТАЛО:**
```cpp
struct RagSettings {
    std::string embedding_model_path = "";
    
    // === ОПТИМИЗИРОВАННЫЕ НАСТРОЙКИ ===
    int max_chunks_in_memory = 500;         // ↑ Увеличено
    float similarity_threshold = 0.50f;     // ↓ Снижено
    int max_embedding_cache_size = 500;     // ↑ Увеличено
    int embedding_dimension = 384;
    int max_sequence_length = 1024;         // ↑ Увеличено
    int max_tokens_per_chunk = 512;         // ↑ Увеличено
    int search_k = 10;                      // ↑ Увеличено
    float mmr_lambda = 0.3f;                // Изменено
    bool enable_mmr = true;                 // ✅ Включено
    bool enable_rag = true;
    bool enable_caching = true;
    bool enable_kv_cache = true;
    bool enable_hybrid_search = true;
    float keyword_boost_weight = 3.0f;      // ↑ Увеличено
    bool enable_query_expansion = true;
    int max_rag_chunks = 50;                // ↑ Увеличено
    
    // === НАСТРОЙКИ ЭМБЕДДИНГА ===
    EmbeddingMode embedding_mode = EmbeddingMode::Hybrid;
    CloudEmbeddingProvider cloud_embedding_provider = CloudEmbeddingProvider::HuggingFace;
    std::string cloud_embedding_api_key = "";
    std::string cloud_embedding_model = "all-MiniLM-L6-v2";
    bool cloud_embedding_priority = true;
    bool enable_embedding_cache = true;
    int embedding_cache_size = 500;
    // ...
};
```

---

## 🎯 Минимальная конфигурация для работы

### 1. Настройка HuggingFace API

Получите токен: https://huggingface.co/settings/tokens

### 2. Код инициализации

```cpp
#include "core/embedding_generator.h"
#include "core/rag_manager.h"

using namespace llama_gui::core;

// Создание генератора эмбеддингов
auto embedding_gen = std::make_shared<EmbeddingGenerator>();

// Настройка облачного провайдера
CloudEmbeddingConfig config;
config.provider = CloudEmbeddingProvider::HuggingFace;
config.api_key = "hf_xxx";  // Ваш токен
config.model_name = "all-MiniLM-L6-v2";
config.timeout_ms = 30000;
config.max_retries = 2;

embedding_gen->configure_cloud_provider(config);
embedding_gen->set_mode(EmbeddingMode::Hybrid);
embedding_gen->set_cloud_priority(true);
embedding_gen->enable_cache(true);
embedding_gen->set_cache_size(500);

// Передача в RagManager
RagManager rag_manager;
// ... инициализация rag_manager с embedding_gen
```

---

## 🧪 Тестирование после интеграции

### Быстрый тест

```cpp
#include <iostream>
#include "core/embedding_generator.h"

using namespace llama_gui::core;

int main() {
    auto emb_gen = std::make_shared<EmbeddingGenerator>();
    
    // Настройка
    CloudEmbeddingConfig config;
    config.provider = CloudEmbeddingProvider::HuggingFace;
    config.api_key = "hf_xxx";
    config.model_name = "all-MiniLM-L6-v2";
    
    emb_gen->configure_cloud_provider(config);
    emb_gen->set_mode(EmbeddingMode::Hybrid);
    
    // Генерация
    auto emb1 = emb_gen->generate_embedding("Юпитер — планета");
    auto emb2 = emb_gen->generate_embedding("Jupiter is a planet");
    
    std::cout << "Embedding 1 size: " << emb1.size() << std::endl;
    std::cout << "Embedding 2 size: " << emb2.size() << std::endl;
    
    // Проверка размерности
    if (emb1.size() == 384 && emb2.size() == 384) {
        std::cout << "✅ Dimension OK" << std::endl;
    }
    
    // Статистика
    auto stats = emb_gen->get_stats();
    std::cout << "Total requests: " << stats.total_requests << std::endl;
    std::cout << "Cache hits: " << stats.cache_hits << std::endl;
    
    return 0;
}
```

### Ожидаемый вывод

```
[EmbeddingGenerator] Initialized in Hybrid mode
[EmbeddingGenerator] Cloud provider configured: HuggingFace, Model: all-MiniLM-L6-v2
[EmbeddingGenerator] Using cloud provider: all-MiniLM-L6-v2
[EmbeddingGenerator] HuggingFace embedding: 384 dimensions
Embedding 1 size: 384
Embedding 2 size: 384
✅ Dimension OK
Total requests: 2
Cache hits: 0
```

---

## 🐛 Отладка

### Включение логирования

Убедитесь, что в коде есть:

```cpp
std::cout << "[EmbeddingGenerator] ..." << std::endl;
```

### Проверка доступности API

```cpp
if (emb_gen->check_cloud_availability()) {
    std::cout << "✅ Cloud API available" << std::endl;
} else {
    std::cout << "❌ Cloud API unavailable" << std::endl;
}
```

### Проверка кэша

```cpp
// Первый запрос (не в кэше)
auto emb1 = emb_gen->generate_embedding("тест");
auto stats1 = emb_gen->get_stats();
std::cout << "Request 1: cache_hits = " << stats1.cache_hits << std::endl;

// Повторный запрос (должен быть в кэше)
auto emb2 = emb_gen->generate_embedding("тест");
auto stats2 = emb_gen->get_stats();
std::cout << "Request 2: cache_hits = " << stats2.cache_hits << std::endl;
// Ожидается: cache_hits = 1
```

---

## 📊 Метрики производительности

### До оптимизации

```
Запрос: "Что такое Юпитер?"
- Найдено чанков: 0
- Порог: 0.70
- Эмбеддинг: псевдо-хеш
- Результат: ❌ "Информация не найдена"
```

### После оптимизации

```
Запрос: "Что такое Юпитер?"
- Найдено чанков: 12
- Порог: 0.50
- Эмбеддинг: HuggingFace all-MiniLM-L6-v2
- Результат: ✅ "Юпитер — пятая планета от Солнца..."
```

---

## ⚠ Известные ограничения

### 1. Rate Limits HuggingFace

- Бесплатный API: ~30 запросов/минуту
- Решение: кэширование, локальная модель

### 2. Совместимость моделей

- Облако и локально: ТОЛЬКО одинаковые модели
- `all-MiniLM-L6-v2` (384 dim) — рекомендуется

### 3. Размер индекса Faiss

- Должен соответствовать размерности эмбеддинга (384)
- Изменение модели требует пересоздания индекса

---

## 🔄 Откат изменений

Если что-то пошло не так:

### 1. Верните старые настройки RAG

```cpp
// В rag_settings.h
float similarity_threshold = 0.70f;  // Было
int max_chunks_in_memory = 100;      // Было
int max_rag_chunks = 20;             // Было
```

### 2. Отключите облачный эмбеддинг

```cpp
embedding_mode = EmbeddingMode::LocalOnly;
```

### 3. Используйте старую версию embedding_generator.cpp

Восстановите из git:
```bash
git checkout HEAD~1 -- src/core/embedding_generator.cpp
```

---

## 📞 Поддержка

### Логи для отладки

При возникновении проблем предоставьте:

1. Вывод `[EmbeddingGenerator] ...`
2. Статистику: `emb_gen->get_stats()`
3. Результат `check_cloud_availability()`
4. Версию модели: `emb_gen->get_model_name()`

### Чеклист диагностики

- [ ] API ключ корректен
- [ ] Интернет подключение есть
- [ ] Модель указана верно (`all-MiniLM-L6-v2`)
- [ ] Размерность 384
- [ ] Кэш включён
- [ ] Режим Hybrid или CloudOnly

---

## 📚 Дополнительные ресурсы

- **Документация:** `EMBEDDING_OPTIMIZATION_GUIDE.md`
- **Быстрый старт:** `QUICK_START_RAG_OPTIMIZATION.md`
- **Резюме:** `OPTIMIZATION_SUMMARY.md`

---

**Дата интеграции:** четверг, 2 апреля 2026 г.  
**Статус:** ✅ Готово к применению
