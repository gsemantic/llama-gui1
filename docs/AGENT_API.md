# API Системы Агентов

**Версия:** 1.0.0  
**Дата:** 20 февраля 2026

## Обзор

Система агентов позволяет расширять функциональность llama-gui через плагины. Каждый агент реализует интерфейс `IAgent` и может быть встроенным или загружаться динамически.

## Архитектура

```
┌─────────────────────────────────────────────────────────┐
│                    Ваше приложение                      │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│  AgentRegistry  │  AgentContext  │  PluginLoader       │
└─────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────┐
│  IAgent (интерфейс)                                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────────────────────┐ │
│  │ rag_agent│ │file_agent│ │ (пользовательские агенты)│ │
│  └──────────┘ └──────────┘ └──────────────────────────┘ │
└─────────────────────────────────────────────────────────┘
```

## Базовый интерфейс

### IAgent

```cpp
#include <agents/agents.h>

class MyAgent : public agents::IAgent {
public:
    const char* name() const override {
        return "my_agent";
    }
    
    const char* description() const override {
        return "Мой пользовательский агент";
    }
    
    const char* version() const override {
        return "1.0.0";
    }
    
    bool initialize(agents::AgentContext* context) override {
        context_ = context;
        return true;
    }
    
    agents::AgentResult execute(const agents::AgentRequest& request) override {
        // Обработка запроса
        std::string action = request.action();
        
        if (action == "greet") {
            std::string name = request.get_param<std::string>("name", "World");
            return agents::AgentResult::success({
                {"message", "Hello, " + name + "!"}
            });
        }
        
        return agents::AgentResult::error("Unknown action: " + action);
    }
    
    void shutdown() override {
        // Очистка ресурсов
    }
    
    agents::AgentCapability capabilities() const override {
        return agents::AgentCapability::NONE;
    }
    
private:
    agents::AgentContext* context_ = nullptr;
};
```

## Регистрация агента

```cpp
#include <agents/agents.h>

using namespace agents;

// Создаём реестр и контекст
AgentRegistry registry;
AgentContext context;

// Создаём агента
auto agent = std::make_unique<MyAgent>();

// Регистрируем (builtin = true для встроенных агентов)
registry.register_agent(std::move(agent), true);

// Инициализируем все агенты
registry.initialize_all(&context);

// Выполняем запрос
AgentRequest request("my_agent", "greet");
request.with_param("name", "Alex");

AgentResult result = registry.execute(request);

if (result.is_ok()) {
    std::string message = result.get<std::string>("message");
    std::cout << message << std::endl;  // Hello, Alex!
} else {
    std::cerr << "Error: " << result.message() << std::endl;
}

// Остановка
registry.shutdown_all();
```

## Взаимодействие между агентами

Агент может вызывать другие агенты через контекст:

```cpp
agents::AgentResult MyAgent::execute(const agents::AgentRequest& request) {
    // Вызываем другой агент
    agents::AgentResult other_result = context_->call_agent(
        "rag_agent",           // имя агента
        "search",              // действие
        {{"query", "текст"}}   // параметры
    );
    
    if (!other_result.is_ok()) {
        return other_result;
    }
    
    // Используем результат
    auto documents = other_result.data()["documents"];
    
    return agents::AgentResult::success({
        {"answer", process(documents)}
    });
}
```

## Возможности (Capabilities)

```cpp
agents::AgentCapability capabilities() const override {
    return agents::AgentCapability::FILE_READ | 
           agents::AgentCapability::HTTP_GET;
}

// Проверка возможности
if (has_capability(capabilities(), agents::AgentCapability::FILE_READ)) {
    // Агент может читать файлы
}
```

### Список возможностей

| Возможность | Описание |
|-------------|----------|
| `FILE_READ` | Чтение файлов |
| `FILE_WRITE` | Запись файлов |
| `FILE_DELETE` | Удаление файлов |
| `DIRECTORY_LIST` | Список файлов |
| `HTTP_GET` | HTTP GET запросы |
| `HTTP_POST` | HTTP POST запросы |
| `RAG_SEARCH` | Поиск по RAG |
| `WEB_SEARCH` | Поиск в интернете |
| `CODE_GENERATION` | Генерация кода |
| `TERMINAL_EXEC` | Выполнение команд |
| `TEXT_SUMMARY` | Суммаризация текста |
| `CALL_OTHER_AGENTS` | Вызов других агентов |

## Создание плагина

Плагин компилируется как разделяемая библиотека (.so/.dll):

### plugin.cpp

```cpp
#include <agents/agents.h>
#include <agents/plugin_c_api.h>

class HelloAgent : public agents::IAgent {
public:
    const char* name() const override { return "hello_agent"; }
    const char* description() const override { return "Пример плагина"; }
    const char* version() const override { return "1.0.0"; }
    
    bool initialize(agents::AgentContext* ctx) override { return true; }
    
    agents::AgentResult execute(const agents::AgentRequest& req) override {
        return agents::AgentResult::success({
            {"message", "Hello from plugin!"}
        });
    }
    
    void shutdown() override {}
    
    agents::AgentCapability capabilities() const override {
        return agents::AgentCapability::NONE;
    }
};

// C-API экспорт
extern "C" {

AGENT_PLUGIN_EXPORT const char* plugin_get_name() {
    return "hello_agent";
}

AGENT_PLUGIN_EXPORT const char* plugin_get_version() {
    return "1.0.0";
}

AGENT_PLUGIN_EXPORT const char* plugin_get_api_version() {
    return AGENT_PLUGIN_API_VERSION;
}

AGENT_PLUGIN_EXPORT AgentHandle* plugin_create_agent() {
    return reinterpret_cast<AgentHandle*>(new HelloAgent());
}

AGENT_PLUGIN_EXPORT void plugin_destroy_agent(AgentHandle* agent) {
    delete reinterpret_cast<IAgent*>(agent);
}

AGENT_PLUGIN_EXPORT PluginExports* plugin_get_exports() {
    static PluginExports exports = {
        AGENT_PLUGIN_API_VERSION,
        plugin_get_name,
        plugin_get_version,
        plugin_get_api_version,
        plugin_create_agent,
        plugin_destroy_agent,
        nullptr,  // initialize (опционально)
        nullptr   // shutdown (опционально)
    };
    return &exports;
}

} // extern "C"
```

### CMakeLists.txt для плагина

```cmake
cmake_minimum_required(VERSION 3.14)
project(hello_agent_plugin CXX)

set(CMAKE_CXX_STANDARD 17)

add_library(hello_agent_plugin SHARED plugin.cpp)

# Включаем заголовки агентов
target_include_directories(hello_agent_plugin PRIVATE 
    ${CMAKE_SOURCE_DIR}/../../include)

# Экспорт символов
set_target_properties(hello_agent_plugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)
```

## Загрузка плагинов

```cpp
#include <agents/agents.h>

agents::PluginLoader loader;
agents::AgentRegistry registry;

// Загрузка плагина из файла
if (loader.load_plugin("plugins/hello_agent_plugin.so")) {
    loader.create_agent_from_plugin("hello_agent", &registry);
}

// Или загрузка из директории
int loaded = loader.load_plugins_from_directory("plugins", &registry);
std::cout << "Loaded " << loaded << " plugins" << std::endl;

// Список плагинов
for (const auto& info : loader.list_plugins()) {
    std::cout << "Plugin: " << info.name 
              << " v" << info.version << std::endl;
}
```

## AgentContext

Контекст предоставляет агенту доступ к сервисам:

```cpp
// Логирование
context->info("my_agent", "Operation started");
context->debug("my_agent", "Debug info");
context->warning("my_agent", "Warning message");
context->error("my_agent", "Error occurred");

// Проверка отмены
if (context->is_cancelled()) {
    return AgentResult::cancelled("Operation cancelled");
}

// Проверка таймаута
if (context->is_timeout()) {
    return AgentResult::error("Timeout exceeded");
}

// Хранилище состояния
context->set_state("my_key", json_value);
auto value = context->get_state("my_key");

// Директории
std::string plugins_dir = context->get_plugins_dir();
std::string data_dir = context->get_data_dir();
```

## Best Practices

1. **Быстрая обработка**: Агенты должны выполнять задачи быстро
2. **Отмена**: Проверяйте `is_cancelled()` периодически
3. **Таймауты**: Устанавливайте разумные таймауты
4. **Логирование**: Логируйте важные события
5. **Изоляция**: Не храните глобальное состояние между вызовами
6. **Безопасность**: Проверяйте права доступа через capabilities

## Примеры использования

### Поиск по документам (RAG)

```cpp
AgentRequest request("rag_agent", "search");
request.with_param("query", "Как настроить сервер?");
request.with_param("k", 5);  // количество результатов

AgentResult result = registry.execute(request);
if (result.is_ok()) {
    auto documents = result.data()["documents"];
    for (const auto& doc : documents) {
        std::cout << doc["content"] << std::endl;
    }
}
```

### Чтение файла

```cpp
AgentRequest request("file_agent", "read");
request.with_param("file_path", "/path/to/file.txt");

AgentResult result = registry.execute(request);
if (result.is_ok()) {
    std::string content = result.get<std::string>("content");
    std::cout << content << std::endl;
}
```

### HTTP запрос

```cpp
AgentRequest request("web_search_agent", "get");
request.with_param("url", "https://api.example.com/data");
request.with_param("timeout_ms", 5000);

AgentResult result = registry.execute(request);
if (result.is_ok()) {
    auto response = result.data();
    std::cout << response["body"] << std::endl;
}
```

## Ссылки

- [`i_agent.h`](../include/agents/i_agent.h) - Базовый интерфейс
- [`agent_context.h`](../include/agents/agent_context.h) - Контекст
- [`agent_registry.h`](../include/agents/agent_registry.h) - Реестр
- [`plugin_c_api.h`](../include/agents/plugin_c_api.h) - C-API для плагинов
- [`plugin_loader.h`](../include/agents/plugin_loader.h) - Загрузчик плагинов
