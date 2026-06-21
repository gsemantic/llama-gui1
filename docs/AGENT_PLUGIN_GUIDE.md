# Руководство по разработке плагинов для системы агентов

**Версия:** 1.0.0  
**Дата:** 20 февраля 2026

---

## 📖 Содержание

1. [Введение](#введение)
2. [Структура плагина](#структура-плагина)
3. [Создание простого агента](#создание-простого-агента)
4. [Расширение существующего агента](#расширение-существующего-агента)
5. [Шаблон плагина](#шаблон-плагина)
6. [Отладка и тестирование](#отладка-и-тестирование)
7. [Публикация плагинов](#публикация-плагинов)

---

## Введение

Это руководство описывает процесс создания пользовательских плагинов для системы агентов llama-gui. Вы научитесь:

- Создавать новых агентов с нуля
- Расширять существующих агентов
- Использовать шаблон для быстрого старта
- Отлаживать и тестировать плагины

---

## Структура плагина

### Минимальный набор файлов

```
my_agent/
├── my_agent.h              # Заголовок (опционально)
├── my_agent.cpp            # Реализация
├── CMakeLists.txt          # Сборка
├── plugin.json             # Метаданные
└── README.md               # Документация
```

### plugin.json

```json
{
    "name": "my_agent",
    "version": "1.0.0",
    "description": "Описание моего агента",
    "author": "Ваше имя",
    "api_version": "1.0.0",
    "library": "libmy_agent_plugin.so",
    "entry_point": "plugin_create_agent",
    "permissions": ["read_files"],
    "capabilities": ["custom_action"],
    "config": {
        "setting1": "value1"
    }
}
```

---

## Создание простого агента

### Шаг 1: Базовый класс

```cpp
// my_agent.cpp
#include <agents/agents.h>
#include <agents/plugin_c_api.h>
#include <string>

class MyAgent : public agents::IAgent {
public:
    // Обязательные методы
    const char* name() const override { return "my_agent"; }
    const char* description() const override { 
        return "Мой первый агент для демонстрации"; 
    }
    const char* version() const override { return "1.0.0"; }
    
    // Инициализация
    bool initialize(agents::AgentContext* context) override {
        context_ = context;
        if (context_) {
            context_->info(name(), "Agent initialized");
        }
        return true;
    }
    
    // Выполнение задач
    agents::AgentResult execute(const agents::AgentRequest& request) override {
        std::string action = request.action();
        
        if (action == "hello") {
            return handle_hello(request);
        } else if (action == "add") {
            return handle_add(request);
        }
        
        return agents::AgentResult::error("Unknown action: " + action);
    }
    
    // Остановка
    void shutdown() override {
        if (context_) {
            context_->info(name(), "Agent shutdown");
        }
        context_ = nullptr;
    }
    
    // Возможности
    agents::AgentCapability capabilities() const override {
        return agents::AgentCapability::NONE;
    }

private:
    agents::AgentContext* context_ = nullptr;
    
    // Обработчики действий
    agents::AgentResult handle_hello(const agents::AgentRequest& request) {
        std::string name = request.get_param<std::string>("name", "World");
        return agents::AgentResult::success({
            {"message", "Hello, " + name + "!"},
            {"agent", name()}
        });
    }
    
    agents::AgentResult handle_add(const agents::AgentRequest& request) {
        int a = request.get_param<int>("a", 0);
        int b = request.get_param<int>("b", 0);
        return agents::AgentResult::success({
            {"result", a + b},
            {"expression", std::to_string(a) + " + " + std::to_string(b)}
        });
    }
};

// ============================================================================
// C-API экспорт (обязательно для всех плагинов)
// ============================================================================

extern "C" {

AGENT_PLUGIN_EXPORT const char* plugin_get_name() {
    return "my_agent";
}

AGENT_PLUGIN_EXPORT const char* plugin_get_version() {
    return "1.0.0";
}

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
        plugin_get_name,
        plugin_get_version,
        plugin_get_api_version,
        reinterpret_cast<plugin_create_agent_fn>(plugin_create_agent),
        reinterpret_cast<plugin_destroy_agent_fn>(plugin_destroy_agent),
        nullptr,  // initialize (вызывается через IAgent)
        nullptr   // shutdown (вызывается через IAgent)
    };
    return &exports;
}

} // extern "C"
```

### Шаг 2: CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.14)
project(my_agent_plugin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Путь к заголовкам проекта
get_filename_component(PROJECT_ROOT 
    "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

# Создаём разделяемую библиотеку
add_library(my_agent_plugin SHARED my_agent.cpp)

# Включаем заголовки
target_include_directories(my_agent_plugin PRIVATE 
    ${PROJECT_ROOT}/include
    ${PROJECT_ROOT}/include/agents)

# Скрываем символы
set_target_properties(my_agent_plugin PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON)

message(STATUS "Building my_agent plugin:")
message(STATUS "  Output: ${CMAKE_SHARED_LIBRARY_PREFIX}my_agent_plugin${CMAKE_SHARED_LIBRARY_SUFFIX}")
```

### Шаг 3: Сборка

```bash
cd my_agent
mkdir build && cd build
cmake ..
make

# Копирование в директорию плагинов
cp libmy_agent_plugin.so ../../plugins/user_plugins/
```

### Шаг 4: Использование

```bash
# В приложении
/agent my_agent hello name='Alex'
# Результат: {"message": "Hello, Alex!", "agent": "my_agent"}

/agent my_agent add a=5 b=3
# Результат: {"result": 8, "expression": "5 + 3"}
```

---

## Расширение существующего агента

### Добавление действия в Code Agent

**Файл:** `plugins/official/code_agent/code_agent.cpp`

```cpp
AgentResult CodeAgent::execute(const AgentRequest& request) {
    std::string action = request.action();
    
    // Существующие действия
    if (action == "generate") { return handle_generate(request); }
    if (action == "analyze") { return handle_analyze(request); }
    
    // НОВОЕ ДЕЙСТВИЕ: Проверка на уязвимости
    if (action == "check_security") {
        return handle_security_check(request);
    }
    
    return AgentResult::error("Unknown action: " + action);
}

// НОВЫЙ МЕТОД
AgentResult CodeAgent::handle_security_check(const AgentRequest& request) {
    std::string code = request.get_param<std::string>("code", "");
    
    nlohmann::json vulnerabilities = nlohmann::json::array();
    
    // Проверка на типичные уязвимости
    if (code.find("strcpy") != std::string::npos) {
        vulnerabilities.push_back({
            {"type", "buffer_overflow"},
            {"function", "strcpy"},
            {"severity", "high"},
            {"suggestion", "Use strncpy or std::string"}
        });
    }
    
    if (code.find("gets") != std::string::npos) {
        vulnerabilities.push_back({
            {"type", "buffer_overflow"},
            {"function", "gets"},
            {"severity", "critical"},
            {"suggestion", "Use fgets"}
        });
    }
    
    if (code.find("system(") != std::string::npos) {
        vulnerabilities.push_back({
            {"type", "command_injection"},
            {"function", "system"},
            {"severity", "high"},
            {"suggestion", "Avoid system calls"}
        });
    }
    
    return AgentResult::success({
        {"vulnerabilities", vulnerabilities},
        {"count", static_cast<int>(vulnerabilities.size())},
        {"safe", vulnerabilities.empty()}
    });
}
```

### Обновление заголовка

**Файл:** `plugins/official/code_agent/code_agent.h`

```cpp
class CodeAgent : public IAgent {
    // ...
private:
    // Существующие обработчики
    AgentResult handle_generate(const AgentRequest& request);
    AgentResult handle_analyze(const AgentRequest& request);
    
    // НОВЫЙ ОБРАБОТЧИК
    AgentResult handle_security_check(const AgentRequest& request);
    // ...
};
```

### Обновление возможностей

**Файл:** `include/agents/agent_capabilities.h`

```cpp
enum class AgentCapability : uint32_t {
    // ...
    CODE_SECURITY = (1 << 25),  // НОВАЯ ВОЗМОЖНОСТЬ
};

// В CodeAgent::capabilities()
AgentCapability capabilities() const override {
    return AgentCapability::CODE_GENERATION | 
           AgentCapability::CODE_ANALYSIS |
           AgentCapability::CODE_SECURITY;  // ДОБАВЛЕНО
}
```

---

## Шаблон плагина

Создайте директорию `plugins/examples/template_agent/` со следующими файлами:

### template_agent.h

```cpp
#pragma once

#include <agents/agents.h>
#include <string>

namespace agents {

/**
 * @brief Шаблон для создания нового агента
 * 
 * Скопируйте этот файл и измените:
 * 1. Имя класса
 * 2. name(), description(), version()
 * 3. Метод execute()
 * 4. capabilities()
 */
class TemplateAgent : public IAgent {
public:
    TemplateAgent();
    ~TemplateAgent() override;

    const char* name() const override;
    const char* description() const override;
    const char* version() const override;

    bool initialize(AgentContext* context) override;
    AgentResult execute(const AgentRequest& request) override;
    void shutdown() override;
    AgentCapability capabilities() const override;
    bool is_ready() const override;

private:
    // Обработчики действий
    AgentResult handle_default(const AgentRequest& request);
    
    AgentContext* context_ = nullptr;
    bool initialized_ = false;
};

} // namespace agents
```

### template_agent.cpp

```cpp
#include "template_agent.h"

namespace agents {

TemplateAgent::TemplateAgent() = default;
TemplateAgent::~TemplateAgent() = default;

const char* TemplateAgent::name() const { return "template_agent"; }
const char* TemplateAgent::description() const { return "Шаблон для нового агента"; }
const char* TemplateAgent::version() const { return "1.0.0"; }

bool TemplateAgent::initialize(AgentContext* context) {
    context_ = context;
    initialized_ = true;
    if (context_) {
        context_->info(name(), "Initialized");
    }
    return true;
}

AgentResult TemplateAgent::execute(const AgentRequest& request) {
    if (!initialized_) {
        return AgentResult::error("Not initialized");
    }
    
    std::string action = request.action();
    
    if (action == "default") {
        return handle_default(request);
    }
    
    return AgentResult::error("Unknown action: " + action);
}

void TemplateAgent::shutdown() {
    if (context_) {
        context_->info(name(), "Shutdown");
    }
    initialized_ = false;
    context_ = nullptr;
}

AgentCapability TemplateAgent::capabilities() const {
    return AgentCapability::NONE;
}

bool TemplateAgent::is_ready() const {
    return initialized_;
}

AgentResult TemplateAgent::handle_default(const AgentRequest& request) {
    return AgentResult::success({{"message", "Default action"}});
}

} // namespace agents

// C-API экспорт
extern "C" {

AGENT_PLUGIN_EXPORT const char* plugin_get_name() { return "template_agent"; }
AGENT_PLUGIN_EXPORT const char* plugin_get_version() { return "1.0.0"; }
AGENT_PLUGIN_EXPORT const char* plugin_get_api_version() { return AGENT_PLUGIN_API_VERSION; }
AGENT_PLUGIN_EXPORT agents::IAgent* plugin_create_agent() { return new agents::TemplateAgent(); }
AGENT_PLUGIN_EXPORT void plugin_destroy_agent(agents::IAgent* agent) { delete agent; }
AGENT_PLUGIN_EXPORT PluginExports* plugin_get_exports() {
    static PluginExports exports = {
        AGENT_PLUGIN_API_VERSION,
        plugin_get_name, plugin_get_version, plugin_get_api_version,
        reinterpret_cast<plugin_create_agent_fn>(plugin_create_agent),
        reinterpret_cast<plugin_destroy_agent_fn>(plugin_destroy_agent),
        nullptr, nullptr
    };
    return &exports;
}

} // extern "C"
```

---

## Отладка и тестирование

### Логирование

```cpp
bool MyAgent::initialize(AgentContext* context) {
    context_ = context;
    
    // Разные уровни логирования
    context_->debug(name(), "Debug info");
    context_->info(name(), "Initialized successfully");
    context_->warning(name(), "Low memory warning");
    context_->error(name(), "Connection failed");
    
    return true;
}
```

### Проверка прав доступа

```cpp
AgentResult MyAgent::execute(const AgentRequest& request) {
    std::string file_path = request.file_path();
    
    // Проверка через SecurityManager
    if (context_) {
        auto security = context_->security();
        auto result = security->check_file_access(name(), file_path, false);
        
        if (!result.allowed) {
            return AgentResult::error("Access denied: " + result.reason);
        }
    }
    
    // ... выполнение действия
}
```

### Использование Sandbox

```cpp
AgentResult MyAgent::execute_command(const std::string& command) {
    SandboxConfig config;
    config.timeout_ms = 5000;
    config.max_memory_mb = 128;
    
    Sandbox sandbox(config);
    
    auto result = sandbox.run_with_timeout([&command]() {
        return std::system(command.c_str());
    }, config.timeout_ms);
    
    if (result.status == SandboxStatus::TIMEOUT) {
        return AgentResult::error("Command timeout");
    }
    
    return AgentResult::success({
        {"exit_code", result.exit_code},
        {"output", result.output}
    });
}
```

---

## Публикация плагинов

### Структура репозитория

```
my-agent-plugin/
├── src/
│   ├── my_agent.h
│   └── my_agent.cpp
├── CMakeLists.txt
├── plugin.json
├── README.md
├── LICENSE
└── tests/
    └── test_my_agent.cpp
```

### plugin.json для публикации

```json
{
    "name": "my_agent",
    "version": "1.0.0",
    "description": "Подробное описание агента",
    "author": "Ваше имя <email@example.com>",
    "api_version": "1.0.0",
    "library": "libmy_agent_plugin.so",
    "entry_point": "plugin_create_agent",
    "dependencies": ["core:1.0"],
    "permissions": ["read_files", "http_get"],
    "capabilities": ["custom_action"],
    "config": {
        "default_timeout_ms": 30000
    },
    "homepage": "https://github.com/username/my-agent-plugin",
    "license": "MIT",
    "tags": ["utility", "custom"],
    "platforms": ["linux", "windows", "macos"]
}
```

### README.md

```markdown
# My Agent Plugin

Плагин для системы агентов llama-gui.

## Возможности

- Действие 1
- Действие 2

## Установка

```bash
git clone https://github.com/username/my-agent-plugin
cd my-agent-plugin
mkdir build && cd build
cmake ..
make
cp libmy_agent_plugin.so ~/.llama-gui/plugins/
```

## Использование

```
/agent my_agent action1 param=value
```

## Лицензия

MIT
```

---

## Ссылки

- [Обзор системы агентов](AGENT_SYSTEM_OVERVIEW.md)
- [API документация](AGENT_API.md)
- [Руководство пользователя](AGENT_USER_GUIDE.md)

---

**Это руководство является частью документации системы агентов llama-gui.**
