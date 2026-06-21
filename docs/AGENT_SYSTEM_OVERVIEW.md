# Система агентов llama-gui: Полный обзор

**Версия:** 1.0.0  
**Дата:** 20 февраля 2026

---

## 📖 Содержание

1. [Введение](#введение)
2. [Архитектура](#архитектура)
3. [Компоненты системы](#компоненты-системы)
4. [Официальные агенты](#официальные-агенты)
5. [Создание плагинов](#создание-плагинов)
6. [Расширение существующих агентов](#расширение-существующих-агентов)
7. [Примеры использования](#примеры-использования)

---

## Введение

Система агентов llama-gui — это модульная платформа для расширения функциональности приложения через плагины. Каждый агент представляет собой специализированный модуль для выполнения определённых задач.

### Ключевые возможности

- ✅ **6 официальных агентов** — RAG, файлы, веб-поиск, код, терминал, суммаризация
- ✅ **Динамические плагины** — загрузка .so/.dll без перезапуска
- ✅ **Безопасность** — песочница, таймауты, белый список команд
- ✅ **UI интеграция** — панель управления, команды, статус
- ✅ **Локализация** — поддержка en/ru

---

## Архитектура

```
┌─────────────────────────────────────────────────────────────────┐
│                        UI Layer (ImGui)                         │
│  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────────┐   │
│  │ AgentPanel  │ │AgentStatus  │ │ AgentCommands           │   │
│  └─────────────┘ └─────────────┘ └─────────────────────────┘   │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Agent System Core                            │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │ AgentRegistry│  │ AgentContext │  │ PluginLoader         │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────────┐  │
│  │ IAgent (API) │  │SecurityManager│ │ AgentLogger          │  │
│  └──────────────┘  └──────────────┘  └──────────────────────┘  │
│  ┌──────────────┐  ┌──────────────┐                            │
│  │ Sandbox      │  │ AgentConfig  │                            │
│  └──────────────┘  └──────────────┘                            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                    Official Plugins                             │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────────────┐   │
│  │ rag_agent│ │code_agent│ │file_agent│ │web_search_agent  │   │
│  └──────────┘ └──────────┘ └──────────┘ └──────────────────┘   │
│  ┌──────────┐ ┌──────────┐                                    │
│  │term_agent│ │summarize │                                    │
│  └──────────┘ └──────────┘                                    │
└─────────────────────────────────────────────────────────────────┘
```

---

## Компоненты системы

### 1. Ядро (Agent Core)

| Компонент | Файл | Описание |
|-----------|------|----------|
| `IAgent` | `include/agents/i_agent.h` | Базовый интерфейс всех агентов |
| `AgentRegistry` | `include/agents/agent_registry.h` | Реестр для регистрации и поиска |
| `AgentContext` | `include/agents/agent_context.h` | Контекст выполнения |
| `PluginLoader` | `include/agents/plugin_loader.h` | Загрузка .so/.dll |

### 2. Безопасность

| Компонент | Файл | Описание |
|-----------|------|----------|
| `SecurityManager` | `include/agents/security_manager.h` | Проверка прав доступа |
| `Sandbox` | `include/agents/sandbox.h` | Изоляция выполнения |

### 3. Конфигурация

| Компонент | Файл | Описание |
|-----------|------|----------|
| `AgentConfigManager` | `include/agents/agent_config.h` | Управление конфигурацией |
| `AgentLogger` | `include/agents/agent_logger.h` | Логирование работы |

### 4. UI

| Компонент | Файл | Описание |
|-----------|------|----------|
| `AgentPanel` | `include/ui/agent_panel.h` | Панель управления |
| `AgentStatusWidget` | `include/ui/agent_status_widget.h` | Виджет статуса |
| `AgentCommands` | `include/ui/agent_commands.h` | Текстовые команды |

---

## Официальные агенты

### 1. RAG Agent

**Назначение:** Поиск по документам с использованием эмбеддингов

**Действия:**
- `search` — поиск по запросу
- `add_document` — добавление документа
- `list_documents` — список документов
- `clear` — очистка кэша

**Возможности:**
```cpp
AgentCapability::RAG_SEARCH
AgentCapability::EMBEDDING
AgentCapability::FILE_READ
```

---

### 2. File Agent

**Назначение:** Работа с файловой системой

**Действия:**
- `read` — чтение файла
- `write` — запись файла
- `delete` — удаление
- `list` — список файлов
- `copy` / `move` — операции с файлами

**Возможности:**
```cpp
AgentCapability::FILE_READ
AgentCapability::FILE_WRITE
AgentCapability::FILE_DELETE
AgentCapability::DIRECTORY_LIST
```

---

### 3. Web Search Agent

**Назначение:** HTTP запросы и поиск в интернете

**Действия:**
- `get` / `post` / `put` / `delete` — HTTP методы
- `search` — поиск через DuckDuckGo/Google

**Возможности:**
```cpp
AgentCapability::HTTP_GET
AgentCapability::HTTP_POST
AgentCapability::HTTP_PUT
AgentCapability::HTTP_DELETE
AgentCapability::WEB_SEARCH
```

---

### 4. Code Agent

**Назначение:** Генерация и анализ кода

**Действия:**
- `generate` — генерация по описанию
- `analyze` — анализ метрик
- `format` — форматирование
- `explain` — объяснение
- `lint` — проверка стиля

**Языки:** C++, Python, JavaScript, TypeScript, Java, Go, Rust, Ruby, PHP, Swift, Kotlin

**Возможности:**
```cpp
AgentCapability::CODE_GENERATION
AgentCapability::CODE_ANALYSIS
```

---

### 5. Terminal Agent

**Назначение:** Выполнение команд терминала с защитой

**Действия:**
- `exec` — выполнение команды
- `exec_safe` — только белый список
- `list_commands` — список разрешённых
- `add_command` / `remove_command` — управление списком

**Безопасность:**
- Белый список команд
- Блокировка опасных операций
- Таймауты через Sandbox

**Возможности:**
```cpp
AgentCapability::TERMINAL_EXEC
```

---

### 6. Summarization Agent

**Назначение:** Суммаризация текста

**Действия:**
- `summarize` — суммаризация
- `summarize_file` — суммаризация файла
- `extract_key_points` — ключевые точки
- `generate_abstract` — абстракт
- `tldr` — очень кратко

**Возможности:**
```cpp
AgentCapability::TEXT_SUMMARY
AgentCapability::TEXT_EXTRACTION
```

---

## Создание плагинов

### Минимальный плагин

```cpp
// my_agent.cpp
#include <agents/agents.h>
#include <agents/plugin_c_api.h>

class MyAgent : public agents::IAgent {
public:
    const char* name() const override { return "my_agent"; }
    const char* description() const override { return "My custom agent"; }
    const char* version() const override { return "1.0.0"; }
    
    bool initialize(agents::AgentContext* ctx) override { return true; }
    
    agents::AgentResult execute(const agents::AgentRequest& req) override {
        return agents::AgentResult::success({{"message", "Hello!"}});
    }
    
    void shutdown() override {}
    
    agents::AgentCapability capabilities() const override {
        return agents::AgentCapability::NONE;
    }
};

// C-API экспорт
extern "C" {
    AGENT_PLUGIN_EXPORT const char* plugin_get_name() { return "my_agent"; }
    AGENT_PLUGIN_EXPORT const char* plugin_get_version() { return "1.0.0"; }
    AGENT_PLUGIN_EXPORT const char* plugin_get_api_version() { 
        return AGENT_PLUGIN_API_VERSION; 
    }
    AGENT_PLUGIN_EXPORT agents::IAgent* plugin_create_agent() { 
        return new MyAgent(); 
    }
    AGENT_PLUGIN_EXPORT void plugin_destroy_agent(agents::IAgent* agent) { 
        delete agent; 
    }
    AGENT_PLUGIN_EXPORT PluginExports* plugin_get_exports() {
        static PluginExports exports = {
            AGENT_PLUGIN_API_VERSION,
            plugin_get_name, plugin_get_version, plugin_get_api_version,
            plugin_create_agent, plugin_destroy_agent, nullptr, nullptr
        };
        return &exports;
    }
}
```

### Сборка плагина

```cmake
cmake_minimum_required(VERSION 3.14)
project(my_agent_plugin CXX)
set(CMAKE_CXX_STANDARD 17)

add_library(my_agent_plugin SHARED my_agent.cpp)

target_include_directories(my_agent_plugin PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/agents)

set_target_properties(my_agent_plugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)
```

---

## Расширение существующих агентов

### Добавление действия в Code Agent

```cpp
// В code_agent.cpp
AgentResult CodeAgent::execute(const AgentRequest& request) {
    std::string action = request.action();
    
    if (action == "generate") { ... }
    else if (action == "analyze") { ... }
    // НОВОЕ ДЕЙСТВИЕ:
    else if (action == "debug") {
        return handle_debug(request);
    }
    ...
}

AgentResult CodeAgent::handle_debug(const AgentRequest& request) {
    std::string code = request.get_param<std::string>("code", "");
    
    // Анализ на типичные ошибки
    nlohmann::json issues = nlohmann::json::array();
    
    if (code.find("std::") != std::string::npos && 
        code.find("#include <iostream>") == std::string::npos) {
        issues.push_back({
            {"type", "missing_include"},
            {"message", "std:: used without iostream"}
        });
    }
    
    return AgentResult::success({{"issues", issues}});
}
```

### Добавление возможности

```cpp
// В agent_capabilities.h
enum class AgentCapability : uint32_t {
    // ...
    CODE_DEBUG = (1 << 24),  // НОВАЯ ВОЗМОЖНОСТЬ
};

// В code_agent.h
AgentCapability capabilities() const override {
    return AgentCapability::CODE_GENERATION | 
           AgentCapability::CODE_ANALYSIS |
           AgentCapability::CODE_DEBUG;  // ДОБАВЛЕНО
}
```

---

## Примеры использования

### Через UI

```
1. Открыть панель агентов: Menu → Tools → Agents Panel
2. Выбрать агента (например, file_agent)
3. Нажать "Execute"
4. Ввести параметры
5. Получить результат
```

### Через команды

```bash
# RAG поиск
/rag Как настроить сервер?

# Генерация кода
/code python функция для вычисления факториала

# Файловые операции
/file read /home/user/document.txt
/file list /home/user/projects

# Суммаризация
/summarize Длинный текст для сокращения...

# Веб-поиск
/search лучшие практики C++

# Управление агентами
/agents list
/agents status
```

### Через API

```cpp
#include <agents/agents.h>

using namespace agents;

// Инициализация
AgentRegistry registry;
AgentContext context;

// Регистрация агента
auto agent = std::make_unique<MyAgent>();
registry.register_agent(std::move(agent), true);

// Инициализация
registry.initialize_all(&context);

// Выполнение
AgentRequest request("my_agent", "action");
request.with_param("param", "value");

AgentResult result = registry.execute(request);
if (result.is_ok()) {
    auto data = result.data();
    // Обработка результата
}

// Остановка
registry.shutdown_all();
```

---

## Ссылки

- [API документация](AGENT_API.md)
- [Руководство по плагинам](AGENT_PLUGIN_GUIDE.md)
- [Руководство пользователя](AGENT_USER_GUIDE.md)
- [Справка по командам](AGENT_COMMANDS.md)

---

**Документация является частью системы агентов llama-gui.**
**Для создания новых агентов см. [AGENT_PLUGIN_GUIDE.md](AGENT_PLUGIN_GUIDE.md).**
