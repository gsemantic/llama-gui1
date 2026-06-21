---
name: universal-openai-api-implementation
description: Создание универсального клиента для произвольных OpenAI-совместимых API с настраиваемым endpoint и API ключом
source: auto-skill
extracted_at: '2026-06-20T00:00:00.000Z'
---

# Универсальный OpenAI-совместимый API

## Когда использовать

Создавать универсальный клиент, когда нужно поддерживать **любой** OpenAI-совместимый API (OpenRouter, KiloCode, OpenAI, локальные инстансы и др.) с произвольным endpoint и API ключом.

## Подход

1. **Создать типы данных** — структуры для моделей, запросов, ответов (аналогично OpenRouter/KiloCode)
2. **Создать клиент** — класс с методами для получения моделей и генерации текста
3. **Создать UI диалог** — окно с настройкой API ключа, URL, endpoint и выбором модели
4. **Интегрировать в Settings** — добавить структуру настроек и методы доступа к API ключам
5. **Добавить в CMakeLists.txt** — подключить новые файлы в сборку

## Структура файлов

```
include/core/
├── universal_openai_client.h      # Интерфейс клиента
├── universal_openai_types.h       # Структуры данных
└── universal_openai_settings.h    # Настройки

src/core/
└── universal_openai_client.cpp    # Реализация

include/ui/
└── universal_openai_dialog.h      # UI диалог

src/ui/
└── universal_openai_dialog.cpp    # Реализация UI
```

## Реализация

### 1. Типы данных (`universal_openai_types.h`)

```cpp
struct UniversalOpenAIModel {
    std::string id;
    std::string object;
    std::string created;
    std::string owned_by;
    int64_t context_length = 0;
    std::vector<std::string> stop;
    std::string pricing;
};

struct UniversalOpenAIRequestParams {
    std::string model;
    std::vector<UniversalOpenAIMessage> messages;
    int max_tokens = 1024;
    float temperature = 0.7f;
    float top_p = 0.9f;
    bool stream = false;
    // ... дополнительные параметры
};

struct UniversalOpenAICompletionResponse {
    std::string id;
    std::string model;
    std::string content;
    int prompt_tokens = 0;
    int completion_tokens = 0;
    int total_tokens = 0;
    bool success = false;
    std::string error;
};
```

### 2. Клиент (`universal_openai_client.h/cpp`)

**Ключевые методы:**
- `set_base_url()` — установка базового URL
- `set_endpoint()` — установка endpoint (например, "chat/completions")
- `get_models_async()` — получение списка моделей
- `complete()` — синхронная генерация текста
- `complete_streaming_async()` — потоковая генерация

**Поддерживаемые провайдеры:**
- OpenRouter: `https://openrouter.ai/api/v1`
- KiloCode: `https://api.kilo.ai/api/gateway`
- OpenAI: `https://api.openai.com/v1`
- Локальные инстансы: `http://localhost:8081`

### 3. UI диалог (`universal_openai_dialog.h/cpp`)

**Функциональность:**
- Вкладка "Настройки" и "Модели"
- Выбор провайдера (preset): Custom, OpenRouter, KiloCode, OpenAI
- Поле для API ключа с маской и кнопкой "Paste"
- Поле для базового URL
- Поле для endpoint
- Загрузка списка моделей из API
- Поиск и фильтрация моделей
- Выбор модели из таблицы
- Недавние модели

**Ключевые методы:**
- `render()` — отрисовка диалога
- `render_general_tab()` — вкладка настроек
- `render_models_tab()` — вкладка моделей
- `load_models()` — загрузка списка моделей
- `search_models()` — поиск моделей

### 4. Интеграция в Settings

**Добавить структуру настроек:**
```cpp
struct UniversalOpenAISettings {
    bool enabled = false;
    std::string selected_model = "";
    std::string custom_base_url = "";
    std::string custom_endpoint = "";
    int timeout_ms = 30000;
    std::vector<std::string> recent_models;
};
```

**Добавить методы доступа к API ключам:**
```cpp
std::string get_universal_openai_api_key() const;
bool update_universal_openai_api_key(const std::string& api_key);
```

**Добавить в CMakeLists.txt:**
```cmake
src/core/universal_openai_client.cpp
src/ui/universal_openai_dialog.cpp
include/core/universal_openai_client.h
include/core/universal_openai_types.h
include/core/universal_openai_settings.h
include/ui/universal_openai_dialog.h
```

## Безопасность

- **API ключи хранятся в `.env` файле**, не в `settings.json`
- **`.env` добавлен в `.gitignore`**
- **Поле для ключа использует маску** (`****`)
- **Кнопка "Paste"** для вставки из буфера обмена

## Пример конфигурации

### OpenRouter
```
Custom Base URL: https://openrouter.ai/api/v1
Endpoint: chat/completions
API Key: sk-or-...
```

### KiloCode
```
Custom Base URL: https://api.kilo.ai/api/gateway
Endpoint: chat/completions
API Key: sk-kilo-...
```

### OpenAI
```
Custom Base URL: https://api.openai.com/v1
Endpoint: chat/completions
API Key: sk-...
```

## Совместимость

Полная совместимость с [OpenAI Chat Completions API](https://platform.openai.com/docs/api-reference/chat):
- ✅ Chat completions
- ✅ Messages (system, user, assistant)
- ✅ Temperature, top_p, top_k
- ✅ Presence/frequency penalty
- ✅ Stop sequences
- ✅ Stream responses
- ✅ Usage statistics

## Troubleshooting

**Ошибка авторизации:**
- Проверьте API ключ в `.env` файле
- Убедитесь, что ключ правильный
- Попробуйте перезагрузить приложение

**Модель не найдена:**
- Проверьте название модели в списке
- Попробуйте обновить список моделей
- Убедитесь, что API доступен

**Rate limit exceeded:**
- Подождите несколько минут
- Используйте бесплатную модель
- Добавьте свой API ключ

## Дополнительные ресурсы

- [OpenAI API Documentation](https://platform.openai.com/docs/api-reference/chat)
- [OpenRouter Documentation](https://openrouter.ai/docs)
- [KiloCode Documentation](https://kilo.ai/docs)
