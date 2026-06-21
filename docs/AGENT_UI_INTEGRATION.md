# Интеграция системы агентов с UI

**Версия:** 1.0.0  
**Дата:** 20 февраля 2026

---

## 📖 Содержание

1. [Обзор интеграции](#обзор-интеграции)
2. [Компоненты интеграции](#компоненты-интеграции)
3. [Использование в MainWindow](#использование-в-mainwindow)
4. [Обработка команд](#обработка-команд)
5. [Примеры кода](#примеры-кода)
6. [Отладка](#отладка)

---

## Обзор интеграции

Система агентов интегрируется с UI через следующие компоненты:

```
MainWindow
    │
    ├──→ CommandManager
    │       └──→ AgentChatIntegration
    │               ├──→ AgentCommands
    │               ├──→ AgentPanel
    │               └──→ AgentStatusWidget
    │
    └──→ ChatInterface
            └──→ Отображение результатов агентов
```

---

## Компоненты интеграции

### 1. AgentChatIntegration

**Файл:** `include/ui/agent_chat_integration.h`

Основной класс для интеграции агентов с чат-интерфейсом.

**Функции:**
- Обработка команд из чата
- Управление AgentPanel
- Обновление AgentStatusWidget
- Форматирование результатов для чата

**Инициализация:**
```cpp
#include "ui/agent_chat_integration.h"

AgentChatIntegration agent_integration;
agent_integration.initialize(&agent_registry, &agent_context);
```

---

### 2. CommandManager (обновлённый)

**Файл:** `include/ui/command_manager.h`

CommandManager теперь поддерживает команды агентов:

**Изменения:**
- Добавлен `agent_chat_integration_`
- Обновлён `executeCommand()` для обработки команд агентов
- Добавлены методы `setAgentChatIntegration()` и `getAgentChatIntegration()`

**Использование:**
```cpp
CommandManager command_manager;
command_manager.setAgentChatIntegration(&agent_integration);

// Теперь команды вида /rag, /search и т.д. обрабатываются
command_manager.executeCommand("/rag тестовый запрос");
```

---

### 3. AgentPanel

**Файл:** `include/ui/agent_panel.h`

Панель управления агентами в главном окне.

**Функции:**
- Отображение списка агентов
- Включение/отключение
- Просмотр информации
- Запуск действий

---

### 4. AgentStatusWidget

**Файл:** `include/ui/agent_status_widget.h`

Виджет статуса в статусной строке.

**Функции:**
- Индикаторы агентов
- Счётчик активных/всего
- Отображение активного агента

---

## Использование в MainWindow

### Шаг 1: Объявление компонентов

```cpp
// main_window.h
#include "ui/agent_chat_integration.h"
#include "ui/command_manager.h"

class MainWindow {
    // ...
    
private:
    agents::AgentRegistry agent_registry_;
    agents::AgentContext agent_context_;
    ui::AgentChatIntegration agent_integration_;
    ui::CommandManager command_manager_;
    
    bool show_agent_panel_ = false;
};
```

### Шаг 2: Инициализация

```cpp
// main_window_init.cpp
bool MainWindow::initializeAgents() {
    // Инициализация реестра
    // Регистрация агентов...
    
    // Инициализация интеграции
    if (!agent_integration_.initialize(&agent_registry_, &agent_context_)) {
        return false;
    }
    
    // Подключение к CommandManager
    command_manager_.setAgentChatIntegration(&agent_integration_);
    
    return true;
}
```

### Шаг 3: Отрисовка

```cpp
// main_window_render.cpp
void MainWindow::render() {
    // Главное меню
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Tools")) {
            ImGui::MenuItem("Agents Panel", nullptr, &show_agent_panel_);
            ImGui::EndMenu();
        }
        
        // Статусная строка с агентами
        agent_integration_.get_status_widget()->render();
        
        ImGui::EndMainMenuBar();
    }
    
    // Панель агентов
    if (show_agent_panel_) {
        agent_integration_.get_agent_panel()->render(&show_agent_panel_);
    }
    
    // ... остальной UI
}
```

### Шаг 4: Обработка команд

```cpp
// main_window_commands.cpp
void MainWindow::handleChatInput(const std::string& input) {
    ChatAgentResult result;
    
    // Обработка через интеграцию
    bool handled = agent_integration_.handle_chat_command(
        input,
        [&result](const ChatAgentResult& r) {
            result = r;
        }
    );
    
    if (handled) {
        // Отображение результата в чате
        if (result.success) {
            chat_interface_.addMessage("Agent", result.message);
        } else {
            chat_interface_.addMessage("Error", result.message);
        }
    } else {
        // Стандартная обработка
        handleStandardInput(input);
    }
}
```

---

## Обработка команд

### Формат команд

```
/<команда> [аргументы] [параметры]
```

### Типы команд

| Префикс | Тип | Пример |
|---------|-----|--------|
| `/agent` | Вызов агента | `/agent rag_agent search query='test'` |
| `/rag` | RAG поиск | `/rag Как настроить сервер?` |
| `/search` | Веб-поиск | `/search C++ tutorials` |
| `/summarize` | Суммаризация | `/summarize текст` |
| `/file` | Файловые операции | `/file read test.txt` |
| `/code` | Генерация кода | `/code python hello` |
| `/agents` | Управление | `/agents list` |

### Обработка результатов

```cpp
void MainWindow::onAgentResult(const ChatAgentResult& result) {
    if (result.success) {
        // Успешный результат
        chat_interface_.addAgentMessage(result.agent_name, result.message);
        
        // Если есть данные, отображаем их
        if (!result.data.empty()) {
            chat_interface_.addCodeBlock(result.data.dump(2));
        }
    } else {
        // Ошибка
        chat_interface_.addErrorMessage(result.message);
    }
    
    // Обновление статуса
    agent_integration_.get_status_widget()->update();
}
```

---

## Примеры кода

### Пример 1: Быстрая интеграция

```cpp
// Минимальная интеграция в MainWindow
class MainWindow {
    agents::AgentRegistry registry_;
    agents::AgentContext context_;
    ui::AgentChatIntegration agents_;
    
public:
    bool init() {
        // Регистрация агентов
        registry_.register_agent(std::make_unique<RagAgent>(), true);
        
        // Инициализация интеграции
        return agents_.initialize(&registry_, &context_);
    }
    
    void render() {
        if (ImGui::BeginMainMenuBar()) {
            agents_.get_status_widget()->render();
            ImGui::EndMainMenuBar();
        }
        
        agents_.get_agent_panel()->render(&show_panel);
    }
    
    void on_chat(const std::string& cmd) {
        agents_.handle_chat_command(cmd, [this](const auto& r) {
            chat_.add(r.success ? "Agent" : "Error", r.message);
        });
    }
};
```

### Пример 2: Расширенная обработка

```cpp
// Обработка с логированием и статистикой
void MainWindow::handleAgentCommand(const std::string& cmd) {
    auto start = std::chrono::steady_clock::now();
    
    agent_integration_.handle_chat_command(cmd, 
        [this, start, cmd](const ChatAgentResult& result) {
            auto end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                end - start).count();
            
            // Логирование
            logger_.info("Agent command", 
                cmd + " -> " + (result.success ? "success" : "failure") +
                " (" + std::to_string(duration) + "ms)");
            
            // Статистика
            stats_.agent_commands++;
            stats_.total_time_ms += duration;
            
            // Отображение
            displayAgentResult(result);
        }
    );
}
```

### Пример 3: Контекстное меню

```cpp
// Контекстное меню для быстрого вызова агентов
void MainWindow::renderAgentContextMenu() {
    if (ImGui::BeginPopupContextItem("AgentContext")) {
        ImGui::Text("Agents");
        ImGui::Separator();
        
        for (const auto& info : agent_registry_.list_agents()) {
            if (ImGui::MenuItem(info.name.c_str())) {
                // Выбор агента
                agent_integration_.set_active_agent(info.name);
                
                // Открытие панели
                show_agent_panel_ = true;
            }
        }
        
        ImGui::Separator();
        
        if (ImGui::MenuItem("Show Panel")) {
            show_agent_panel_ = true;
        }
        
        ImGui::EndPopup();
    }
}
```

---

## Отладка

### Включение логирования

```cpp
// В agents_config.json
{
    "enable_logging": true,
    "log_level": "DEBUG",
    "log_file": "logs/agents.log"
}
```

### Просмотр логов

```bash
# В реальном времени
tail -f logs/agents.log

# Фильтрация по агенту
grep "rag_agent" logs/agents.log
```

### Отладка команд

```cpp
// В CommandManager.cpp
CommandManager::CommandResult CommandManager::executeCommand(const std::string& name) {
    std::cout << "[DEBUG] Executing command: " << name << std::endl;
    
    if (agent_chat_integration_ && name[0] == '/') {
        std::cout << "[DEBUG] Agent command detected" << std::endl;
        // ...
    }
    
    // ...
}
```

### Статистика

```cpp
// Получение статистики
auto stats = command_manager_.getStatistics();
std::cout << "Total commands: " << stats.total_commands << std::endl;
std::cout << "Agent commands: " << stats.agent_commands << std::endl;
```

---

## Решение проблем

### Проблема: Команды агентов не обрабатываются

**Причина:** AgentChatIntegration не подключён к CommandManager

**Решение:**
```cpp
command_manager_.setAgentChatIntegration(&agent_integration_);
```

---

### Проблема: Панель агентов не отображается

**Причина:** Не вызывается render()

**Решение:**
```cpp
void MainWindow::render() {
    // Добавить:
    if (show_agent_panel_) {
        agent_integration_.get_agent_panel()->render(&show_agent_panel_);
    }
}
```

---

### Проблема: Статус не обновляется

**Причина:** Не вызывается update()

**Решение:**
```cpp
// После выполнения команды
agent_integration_.get_status_widget()->update();
```

---

## Ссылки

- [Обзор системы агентов](AGENT_SYSTEM_OVERVIEW.md)
- [Руководство пользователя](AGENT_USER_GUIDE.md)
- [Справка по командам](AGENT_COMMANDS.md)

---

**Интеграция системы агентов с UI завершена.**
