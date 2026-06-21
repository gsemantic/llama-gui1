# План реализации системы поддержки OpenRouter в RagManager для глубокого анализа документов (MapReduce)

**Дата создания:** 2 апреля 2026 г.  
**Статус:** План готов к реализации  
**Общая сложность:** ~25-30 часов работы

---

## 1. Executive Summary

### 1.1 Проблема
Текущая реализация RAG Deep Analysis (MapReduce) использует только локальный llama-server через `llama_interface_`, что вызывает:
- Таймауты (300 секунд на запрос)
- Нагрузку на CPU
- Невозможность работы без локального сервера

### 1.2 Решение
Добавить поддержку OpenRouter API для выполнения MapReduce операций с использованием бесплатных облачных моделей.

### 1.3 Ключевые возможности
- ✅ RagManager может использовать OpenRouter для MapReduce
- ✅ По умолчанию выбираются только бесплатные модели
- ✅ 4 профиля производительности (fast, balanced, quality, economy)
- ✅ Ручная настройка моделей возможна
- ✅ Логирование выбора модели
- ✅ Fallback на локальный сервер если OpenRouter не настроен
- ✅ Настройки сохраняются в settings.ini

---

## 2. Архитектурные изменения

### 2.1 Диаграмма компонентов

```
┌─────────────────────────────────────────────────────────────────┐
│                        ChatInterface                            │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │  settings_.openrouter().enabled → передача клиента      │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                         RagManager                              │
│  ┌──────────────────┐  ┌──────────────────────────────────┐    │
│  │ llama_interface_ │  │  openrouter_client_ (NEW)        │    │
│  │ (локальный)      │  │  openrouter_model_id_ (NEW)      │    │
│  └──────────────────┘  └──────────────────────────────────┘    │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │              Model Selection System (NEW)               │   │
│  │  • FreeModelInfo database                               │   │
│  │  • Performance profiles (fast/balanced/quality/economy) │   │
│  │  • Auto-selection logic                                 │   │
│  └─────────────────────────────────────────────────────────┘   │
│                                                                 │
│  ┌─────────────────────────────────────────────────────────┐   │
│  │         DeepAnalysisSettings (UPDATED)                  │   │
│  │  • auto_select_models, performance_profile              │   │
│  │  • map_model, reduce_model, free_only                   │   │
│  └─────────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                ┌─────────────┴─────────────┐
                │                           │
                ▼                           ▼
    ┌───────────────────────┐   ┌───────────────────────────┐
    │  generate_chunk_      │   │  synthesize_final_        │
    │  summary()            │   │  answer()                 │
    │                       │   │                           │
    │  → OpenRouter (если)  │   │  → OpenRouter (если)      │
    │  → Local (fallback)   │   │  → Local (fallback)       │
    └───────────────────────┘   └───────────────────────────┘
```

### 2.2 База данных бесплатных моделей OpenRouter

```cpp
struct FreeModelInfo {
    std::string model_id;
    int speed_rating;      // 1-5 (5 = fastest)
    int quality_rating;    // 1-5 (5 = best quality)
    std::string best_for;  // "map", "reduce", "both"
};

// Список бесплатных моделей:
static const std::vector<FreeModelInfo> FREE_MODELS = {
    {"google/gemma-2-2b-it:free",        5, 2, "map"},
    {"google/gemma-2-9b-it:free",        4, 4, "both"},
    {"meta-llama/llama-3-8b-instruct:free", 4, 4, "both"},
    {"nousresearch/hermes-2-pro-mistral-7b:free", 3, 5, "reduce"},
    {"microsoft/phi-3-mini-128k-instruct:free", 4, 3, "map"},
    {"huggingface/zephyr-7b-beta:free",  3, 3, "reduce"},
    {"openchat/openchat-7b:free",        3, 3, "reduce"}
};
```

### 2.3 Профили производительности

| Профиль | Map модель | Reduce модель | Описание |
|---------|------------|---------------|----------|
| **fast** | gemma-2-2b-it:free | gemma-2-9b-it:free | Максимальная скорость |
| **balanced** (default) | gemma-2-9b-it:free | llama-3-8b-instruct:free | Баланс скорость/качество |
| **quality** | llama-3-8b-instruct:free | hermes-2-pro-mistral-7b:free | Максимальное качество |
| **economy** | gemma-2-2b-it:free | gemma-2-2b-it:free | Минимальное использование лимита |

---

## 3. Детальный план задач

### Задача 1: Обновить DeepAnalysisSettings новыми полями
**Файл:** `include/core/rag_settings.h`  
**Сложность:** Низкая (1 час)  
**Зависимости:** Нет

**Изменения:**
```cpp
struct DeepAnalysisSettings {
    DeepAnalysisMode mode = DeepAnalysisMode::Disabled;
    int chunks_per_batch = 10;
    int max_iterations = 50;
    bool enable_progressive_summary = true;
    int final_synthesis_chunks = 30;
    bool auto_adjust_context_size = true;
    int target_context_size = 8192;
    
    // === НОВЫЕ ПОЛЯ ===
    bool auto_select_models = true;           // Автовыбор моделей
    std::string performance_profile = "balanced";  // Профиль производительности
    std::string map_model = "";               // Пустая = автовыбор
    std::string reduce_model = "";            // Пустая = автовыбор
    bool free_only = true;                    // ПО УМОЛЧАНИЮ true!
    double max_cost_per_million = 0.00;       // Макс. стоимость за 1M токенов
};
```

**Критерии приёмки:**
- [ ] Все поля добавлены в структуру
- [ ] Значения по умолчанию установлены корректно
- [ ] Код компилируется без ошибок

---

### Задача 2: Создать базу данных бесплатных моделей
**Файл:** `include/core/rag_model_database.h` (новый)  
**Сложность:** Низкая (1.5 часа)  
**Зависимости:** Нет

**Содержимое файла:**
```cpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>

namespace llama_gui {
namespace core {

struct FreeModelInfo {
    std::string model_id;
    int speed_rating;      // 1-5 (5 = fastest)
    int quality_rating;    // 1-5 (5 = best quality)
    std::string best_for;  // "map", "reduce", "both"
    
    bool is_suitable_for(const std::string& stage) const {
        return best_for == "both" || best_for == stage;
    }
};

class RagModelDatabase {
public:
    // Получить список всех бесплатных моделей
    static const std::vector<FreeModelInfo>& get_free_models();
    
    // Получить модель для профиля и этапа
    static std::string get_free_model_for_profile(
        const std::string& stage,      // "map" или "reduce"
        const std::string& profile     // "fast", "balanced", "quality", "economy"
    );
    
    // Получить информацию о модели
    static const FreeModelInfo* get_model_info(const std::string& model_id);
    
    // Проверить существование модели
    static bool model_exists(const std::string& model_id);
    
private:
    static const std::vector<FreeModelInfo> FREE_MODELS_;
    static const std::unordered_map<std::string, std::pair<std::string, std::string>> PROFILE_MAP_;
};

} // namespace core
} // namespace llama_gui
```

**Критерии приёмки:**
- [ ] Все 7 бесплатных моделей добавлены
- [ ] Профили работают корректно
- [ ] Написаны unit-тесты для get_free_model_for_profile

---

### Задача 3: Добавить поля OpenRouter в RagManager
**Файл:** `include/core/rag_manager.h`  
**Сложность:** Низкая (1 час)  
**Зависимости:** Задача 1

**Изменения:**
```cpp
// В private секцию добавить:
#include "openrouter_client.h"
#include "rag_model_database.h"

class RagManager {
    // ... существующие поля ...
    
    // === OpenRouter поддержка ===
    std::shared_ptr<OpenRouterClient> openrouter_client_;
    std::string openrouter_model_id_;
    bool use_openrouter_for_rag_ = false;
    
    // === Методы ===
public:
    void set_openrouter_client(std::shared_ptr<OpenRouterClient> client);
    void set_openrouter_model(const std::string& model_id);
    void enable_openrouter_for_rag(bool enable);
    bool is_openrouter_available() const;
    
private:
    std::string select_model_for_stage(
        const std::string& stage,
        const DeepAnalysisSettings& settings
    );
};
```

**Критерии приёмки:**
- [ ] Поля добавлены в класс
- [ ] Методы объявлены
- [ ] Заголовочный файл компилируется

---

### Задача 4: Реализовать систему выбора моделей
**Файл:** `src/core/rag_manager_model_selection.cpp` (новый)  
**Сложность:** Средняя (3 часа)  
**Зависимости:** Задачи 2, 3

**Методы для реализации:**
```cpp
std::string RagManager::select_model_for_stage(
    const std::string& stage,  // "map" или "reduce"
    const DeepAnalysisSettings& settings)
{
    // 1. Если ручная настройка - вернуть её
    if (!settings.auto_select_models) {
        if (stage == "map" && !settings.map_model.empty()) {
            return settings.map_model;
        }
        if (stage == "reduce" && !settings.reduce_model.empty()) {
            return settings.reduce_model;
        }
    }
    
    // 2. Автовыбор из профиля
    if (settings.free_only) {
        return RagModelDatabase::get_free_model_for_profile(
            stage, settings.performance_profile);
    }
    
    // 3. Fallback на balanced профиль
    return RagModelDatabase::get_free_model_for_profile(stage, "balanced");
}
```

**Логирование:**
```
[MODEL SELECT] Auto-select enabled (FREE ONLY), profile: balanced
[MODEL SELECT] Map stage → google/gemma-2-9b-it:free
[MODEL SELECT] Reduce stage → meta-llama/llama-3-8b-instruct:free
```

**Критерии приёмки:**
- [ ] Автовыбор работает для всех 4 профилей
- [ ] Ручная настройка переопределяет автовыбор
- [ ] Логирование выводит выбор модели

---

### Задача 5: Обновить generate_chunk_summary с поддержкой OpenRouter
**Файл:** `src/core/rag_manager_deep_analysis.cpp`  
**Сложность:** Средняя (4 часа)  
**Зависимости:** Задачи 3, 4

**Изменения:**
```cpp
std::string RagManager::generate_chunk_summary(
    const RagChunk& chunk,
    const std::string& query)
{
    // Проверка: если OpenRouter доступен → использовать его
    if (openrouter_client_ && use_openrouter_for_rag_) {
        return generate_chunk_summary_openrouter(chunk, query);
    }
    
    // Fallback на локальный сервер
    return generate_chunk_summary_local(chunk, query);
}

// Новый метод
std::string RagManager::generate_chunk_summary_openrouter(
    const RagChunk& chunk,
    const std::string& query)
{
    // Выбор модели для Map этапа
    std::string model = select_model_for_stage("map", deep_analysis_settings_);
    
    // Формирование запроса
    OpenRouterRequestParams params;
    params.model = model;
    params.messages = {
        {"system", "Ты - ассистент для анализа документов..."},
        {"user", "Текст для анализа:\n" + chunk.content + "\n\nВопрос: " + query}
    };
    params.max_tokens = 512;
    params.temperature = 0.3f;
    
    // Выполнение запроса
    auto response = openrouter_client_->complete(params);
    
    if (response.success && !response.content.empty()) {
        return trim_summary(response.content);
    }
    
    // Fallback на локальный сервер при ошибке
    std::cerr << "[SUMMARY] OpenRouter failed, falling back to local" << std::endl;
    return generate_chunk_summary_local(chunk, query);
}
```

**Критерии приёмки:**
- [ ] OpenRouter используется когда доступен
- [ ] Fallback на локальный сервер работает
- [ ] Модель выбирается согласно профилю

---

### Задача 6: Обновить synthesize_final_answer с поддержкой OpenRouter
**Файл:** `src/core/rag_manager_deep_analysis.cpp`  
**Сложность:** Средняя (4 часа)  
**Зависимости:** Задачи 3, 4, 5

**Изменения:**
```cpp
std::string RagManager::synthesize_final_answer(
    const std::string& query,
    const std::vector<std::string>& summaries,
    int target_context_size)
{
    // Проверка: если OpenRouter доступен → использовать его
    if (openrouter_client_ && use_openrouter_for_rag_) {
        return synthesize_final_answer_openrouter(query, summaries, target_context_size);
    }
    
    // Fallback на локальный сервер
    return synthesize_final_answer_local(query, summaries, target_context_size);
}

// Новый метод
std::string RagManager::synthesize_final_answer_openrouter(
    const std::string& query,
    const std::vector<std::string>& summaries,
    int target_context_size)
{
    // Выбор модели для Reduce этапа
    std::string model = select_model_for_stage("reduce", deep_analysis_settings_);
    
    // Сборка контекста
    std::string context = build_context_from_summaries(summaries, target_context_size);
    
    // Формирование запроса
    OpenRouterRequestParams params;
    params.model = model;
    params.messages = {
        {"system", "Ты - ассистент для синтеза информации..."},
        {"user", "Вопрос: " + query + "\n\nРезюме:\n" + context}
    };
    params.max_tokens = 2048;
    params.temperature = 0.5f;
    
    // Выполнение запроса
    auto response = openrouter_client_->complete(params);
    
    if (response.success && !response.content.empty()) {
        return response.content;
    }
    
    // Fallback на локальный сервер
    std::cerr << "[SYNTHESIS] OpenRouter failed, falling back to local" << std::endl;
    return synthesize_final_answer_local(query, summaries, target_context_size);
}
```

**Критерии приёмки:**
- [ ] OpenRouter используется для Reduce этапа
- [ ] Модель выбирается согласно профилю
- [ ] Fallback работает корректно

---

### Задача 7: Обновить сериализацию настроек
**Файлы:** 
- `src/core/settings_serialization_basic.cpp`
- `include/core/settings.h`

**Сложность:** Низкая (2 часа)  
**Зависимости:** Задача 1

**Изменения в serializeRagSettings:**
```cpp
void Settings::serializeRagSettings(json& j) const {
    j["rag"] = {
        // ... существующие поля ...
        
        // Настройки глубокого анализа (обновлено)
        {"deep_analysis_mode", static_cast<int>(rag_settings_.deep_analysis.mode)},
        {"deep_analysis_chunks_per_batch", rag_settings_.deep_analysis.chunks_per_batch},
        {"deep_analysis_max_iterations", rag_settings_.deep_analysis.max_iterations},
        {"deep_analysis_enable_progressive_summary", rag_settings_.deep_analysis.enable_progressive_summary},
        {"deep_analysis_final_synthesis_chunks", rag_settings_.deep_analysis.final_synthesis_chunks},
        {"deep_analysis_auto_adjust_context_size", rag_settings_.deep_analysis.auto_adjust_context_size},
        {"deep_analysis_target_context_size", rag_settings_.deep_analysis.target_context_size},
        
        // === НОВЫЕ ПОЛЯ ===
        {"deep_analysis_auto_select_models", rag_settings_.deep_analysis.auto_select_models},
        {"deep_analysis_performance_profile", rag_settings_.deep_analysis.performance_profile},
        {"deep_analysis_map_model", rag_settings_.deep_analysis.map_model},
        {"deep_analysis_reduce_model", rag_settings_.deep_analysis.reduce_model},
        {"deep_analysis_free_only", rag_settings_.deep_analysis.free_only},
        {"deep_analysis_max_cost_per_million", rag_settings_.deep_analysis.max_cost_per_million}
    };
}
```

**Изменения в deserializeRagSettings:**
```cpp
void Settings::deserializeRagSettings(const json& j) {
    if (j.contains("rag")) {
        auto& r = j["rag"];
        // ... существующие поля ...
        
        // === НОВЫЕ ПОЛЯ ===
        rag_settings_.deep_analysis.auto_select_models = r.value("deep_analysis_auto_select_models", true);
        rag_settings_.deep_analysis.performance_profile = r.value("deep_analysis_performance_profile", "balanced");
        rag_settings_.deep_analysis.map_model = r.value("deep_analysis_map_model", "");
        rag_settings_.deep_analysis.reduce_model = r.value("deep_analysis_reduce_model", "");
        rag_settings_.deep_analysis.free_only = r.value("deep_analysis_free_only", true);
        rag_settings_.deep_analysis.max_cost_per_million = r.value("deep_analysis_max_cost_per_million", 0.0);
    }
}
```

**Критерии приёмки:**
- [ ] Сериализация работает
- [ ] Десериализация работает
- [ ] Значения по умолчанию сохраняются

---

### Задача 8: Интеграция с ChatInterface при инициализации
**Файл:** `src/ui/main_window_init_constructor.cpp`  
**Сложность:** Средняя (2 часа)  
**Зависимости:** Задачи 3, 4

**Изменения:**
```cpp
// После инициализации rag_manager_:
if (rag_manager_) {
    // === Передача OpenRouter клиента в RagManager ===
    if (settings_.openrouter().enabled && !settings_.openrouter().api_key.empty()) {
        auto openrouter_client = std::make_shared<llama_gui::core::OpenRouterClient>(
            settings_.openrouter().api_key);
        
        openrouter_client->set_timeout(settings_.openrouter().timeout_ms);
        
        if (!settings_.openrouter().custom_base_url.empty()) {
            openrouter_client->set_base_url(settings_.openrouter().custom_base_url);
        }
        
        rag_manager_->set_openrouter_client(openrouter_client);
        rag_manager_->enable_openrouter_for_rag(true);
        
        // Установка модели из настроек
        if (!settings_.openrouter().selected_model.empty()) {
            rag_manager_->set_openrouter_model(settings_.openrouter().selected_model);
        }
        
        std::cout << "[RAG] OpenRouter enabled for deep analysis" << std::endl;
    }
    // ============================================================
}
```

**Критерии приёмки:**
- [ ] OpenRouter клиент передаётся в RagManager
- [ ] Настройки применяются корректно
- [ ] Логирование инициализации

---

### Задача 9: Обновить settings.ini настройки по умолчанию
**Файл:** `settings.ini` (или создание шаблона)  
**Сложность:** Низкая (0.5 часа)  
**Зависимости:** Задача 1

**Добавить в секцию [rag]:**
```ini
[rag]
; Настройки глубокого анализа
deep_analysis_mode = 1                    ; 0=Disabled, 1=MapReduce, 2=Iterative, 3=Hierarchical
deep_analysis_chunks_per_batch = 10
deep_analysis_max_iterations = 50
deep_analysis_enable_progressive_summary = true
deep_analysis_final_synthesis_chunks = 30
deep_analysis_auto_adjust_context_size = true
deep_analysis_target_context_size = 8192

; === НОВЫЕ НАСТРОЙКИ OPENROUTER ===
deep_analysis_auto_select_models = true
deep_analysis_performance_profile = balanced
deep_analysis_map_model =
deep_analysis_reduce_model =
deep_analysis_free_only = true
deep_analysis_max_cost_per_million = 0.00
```

**Критерии приёмки:**
- [ ] Настройки добавлены в settings.ini
- [ ] Значения по умолчанию корректны

---

### Задача 10: Добавить логирование выбора модели
**Файл:** `src/core/rag_manager_model_selection.cpp`  
**Сложность:** Низкая (1 час)  
**Зависимости:** Задача 4

**Формат логирования:**
```cpp
std::cout << "[MODEL SELECT] Auto-select enabled (FREE ONLY), profile: " 
          << settings.performance_profile << std::endl;

std::string map_model = select_model_for_stage("map", settings);
std::cout << "[MODEL SELECT] Map stage → " << map_model << std::endl;

std::string reduce_model = select_model_for_stage("reduce", settings);
std::cout << "[MODEL SELECT] Reduce stage → " << reduce_model << std::endl;
```

**Пример вывода:**
```
[MODEL SELECT] Auto-select enabled (FREE ONLY), profile: balanced
[MODEL SELECT] Map stage → google/gemma-2-9b-it:free
[MODEL SELECT] Reduce stage → meta-llama/llama-3-8b-instruct:free
[DEEP ANALYSIS] Starting deep document analysis
[DEEP ANALYSIS] Query: "..."
[DEEP ANALYSIS] Total chunks: 25
[DEEP ANALYSIS] Mode: MapReduce, chunks_per_batch: 10
```

**Критерии приёмки:**
- [ ] Логирование на каждом этапе выбора
- [ ] Формат соответствует спецификации

---

### Задача 11: Тестирование и отладка
**Файлы:** Ручное тестирование  
**Сложность:** Высокая (6 часов)  
**Зависимости:** Все предыдущие задачи

**План тестирования:**

#### 11.1 Unit-тесты
- [ ] Тест RagModelDatabase::get_free_model_for_profile для всех профилей
- [ ] Тест select_model_for_stage с ручной настройкой
- [ ] Тест select_model_for_stage с автовыбором

#### 11.2 Интеграционные тесты
- [ ] Тест generate_chunk_summary с OpenRouter
- [ ] Тест generate_chunk_summary с fallback на локальный
- [ ] Тест synthesize_final_answer с OpenRouter
- [ ] Тест полного MapReduce цикла

#### 11.3 Сценарии использования
1. **Сценарий A: OpenRouter включён, бесплатные модели**
   - Настроить API ключ OpenRouter
   - Выбрать профиль "balanced"
   - Запустить глубокий анализ документа
   - Проверить логи выбора модели
   - Проверить использование бесплатных моделей

2. **Сценарий B: OpenRouter выключен, локальный сервер**
   - Отключить OpenRouter в настройках
   - Запустить глубокий анализ
   - Проверить использование локального сервера

3. **Сценарий C: Ручная настройка моделей**
   - Отключить auto_select_models
   - Указать map_model и reduce_model вручную
   - Проверить применение ручных настроек

4. **Сценарий D: Fallback при ошибке OpenRouter**
   - Ввести неверный API ключ
   - Запустить анализ
   - Проверить fallback на локальный сервер

**Критерии приёмки:**
- [ ] Все сценарии работают корректно
- [ ] Нет утечек памяти
- [ ] Логирование информативно

---

## 4. Зависимости между задачами

```
Задача 1 (DeepAnalysisSettings) ─────────┬─→ Задача 3 (RagManager поля)
                                          │       │
Задача 2 (FreeModelInfo) ─────────────────┘       │
                                                  ↓
                                   Задача 4 (Система выбора моделей)
                                                  │
                    ┌─────────────────────────────┼─────────────────────────────┐
                    ↓                             ↓                             ↓
        Задача 5 (generate_chunk)    Задача 7 (Сериализация)    Задача 10 (Логирование)
                    │                             │                             │
                    ↓                             ↓                             │
        Задача 6 (synthesize_final)               │                             │
                    │                             ↓                             │
                    └──────────────────→ Задача 8 (ChatInterface) ←─────────────┘
                                                  │
                                                  ↓
                                   Задача 9 (settings.ini)
                                                  │
                                                  ↓
                                   Задача 11 (Тестирование)
```

---

## 5. Оценка времени

| Задача | Описание | Сложность | Время (часы) |
|--------|----------|-----------|--------------|
| 1 | Обновить DeepAnalysisSettings | Низкая | 1.0 |
| 2 | База данных бесплатных моделей | Низкая | 1.5 |
| 3 | Добавить поля OpenRouter в RagManager | Низкая | 1.0 |
| 4 | Система выбора моделей | Средняя | 3.0 |
| 5 | Обновить generate_chunk_summary | Средняя | 4.0 |
| 6 | Обновить synthesize_final_answer | Средняя | 4.0 |
| 7 | Обновить сериализацию настроек | Низкая | 2.0 |
| 8 | Интеграция с ChatInterface | Средняя | 2.0 |
| 9 | Обновить settings.ini | Низкая | 0.5 |
| 10 | Добавить логирование | Низкая | 1.0 |
| 11 | Тестирование и отладка | Высокая | 6.0 |
| **ИТОГО** | | | **26.0 часов** |

---

## 6. Риски и стратегии смягчения

| Риск | Вероятность | Влияние | Стратегия смягчения |
|------|-------------|---------|---------------------|
| API OpenRouter недоступно | Средняя | Высокое | Fallback на локальный сервер реализован |
| Бесплатные модели имеют лимиты | Высокая | Среднее | Логирование использования, настройка max_cost |
| Таймауты при больших документах | Средняя | Среднее | Батчинг чанков, прогрессивное суммирование |
| Конфликты с существующим кодом | Низкая | Высокое | Тщательное тестирование, code review |
| Утечки памяти | Низкая | Среднее | Использовать smart pointers, тестирование |

---

## 7. Чеклист готовности

### Код
- [ ] Все файлы изменены согласно плану
- [ ] Код компилируется без ошибок и предупреждений
- [ ] Стиль кода соответствует проекту

### Функциональность
- [ ] RagManager использует OpenRouter для MapReduce
- [ ] По умолчанию выбираются только бесплатные модели
- [ ] 4 профиля производительности работают
- [ ] Ручная настройка моделей возможна
- [ ] Логирование выбора модели реализовано
- [ ] Fallback на локальный сервер работает
- [ ] Настройки сохраняются в settings.ini

### Тестирование
- [ ] Unit-тесты написаны
- [ ] Интеграционные тесты пройдены
- [ ] Все сценарии использования проверены

### Документация
- [ ] README обновлён
- [ ] Комментарии в коде добавлены
- [ ] Примеры настроек предоставлены

---

## 8. Следующие шаги

1. **Немедленно:** Начать с Задачи 1 (обновление DeepAnalysisSettings)
2. **После Задачи 1-4:** Промежуточное тестирование системы выбора моделей
3. **После Задачи 6:** Тестирование MapReduce с OpenRouter
4. **После Задачи 11:** Финальный code review и merge

---

**Примечание:** Этот план предполагает, что OpenRouterClient уже существует и работает корректно. Если есть проблемы с OpenRouterClient, их следует устранить до начала реализации.
