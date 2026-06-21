---
name: env_file_handler_pattern
description: Шаблон для создания класса EnvFileHandler для работы с .env файлом и хранения секретов
source: auto-skill
extracted_at: '2026-06-19T17:34:37.394Z'
---

# Шаблон EnvFileHandler для хранения секретов в .env

## Когда использовать
При необходимости хранения чувствительных данных (API-ключи, токены) в файле .env вместо настройки INI/JSON.

## Структура реализации

### 1. Заголовочный файл (env_file_handler.h)

```cpp
#pragma once

#include <string>
#include <unordered_map>

namespace llama_gui {
namespace core {

class EnvFileHandler {
public:
    EnvFileHandler();
    ~EnvFileHandler() = default;

    // Запрет копирования
    EnvFileHandler(const EnvFileHandler&) = delete;
    EnvFileHandler& operator=(const EnvFileHandler&) = delete;

    /**
     * @brief Загрузить все переменные из .env файла
     * @param file_path Путь к .env файлу (по умолчанию: "./.env")
     * @return true если успешно загружено
     */
    bool load_from_file(const std::string& file_path = ".env");

    /**
     * @brief Сохранить все переменные в .env файл
     * @param file_path Путь к .env файлу (по умолчанию: "./.env")
     * @return true если успешно сохранено
     */
    bool save_to_file(const std::string& file_path = ".env") const;

    /**
     * @brief Установить значение переменной окружения
     * @param key Имя переменной
     * @param value Значение
     */
    void set(const std::string& key, const std::string& value);

    /**
     * @brief Получить значение переменной окружения
     * @param key Имя переменной
     * @param default_value Значение по умолчанию
     * @return Значение переменной или default_value
     */
    std::string get(const std::string& key, const std::string& default_value = "") const;

    /**
     * @brief Проверить, существует ли переменная
     * @param key Имя переменной
     * @return true если существует
     */
    bool has(const std::string& key) const;

    /**
     * @brief Удалить переменную из системы
     * @param key Имя переменной
     */
    void remove(const std::string& key);

    /**
     * @brief Обновить переменную в системе (через setenv/unsetenv)
     * @param key Имя переменной
     * @param value Значение
     * @return true если успешно
     */
    bool update_environment(const std::string& key, const std::string& value);

    /**
     * @brief Получить все переменные окружения
     * @return Мап всех переменных
     */
    const std::unordered_map<std::string, std::string>& get_all() const;

private:
    std::unordered_map<std::string, std::string> env_vars_;

    bool load_from_file_internal(const std::string& file_path);
    bool save_to_file_internal(const std::string& file_path) const;
};

} // namespace core
} // namespace llama_gui
```

### 2. Реализация (env_file_handler.cpp)

```cpp
#include "../include/core/env_file_handler.h"
#include "../include/core/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace llama_gui {
namespace core {

EnvFileHandler::EnvFileHandler() {
    // Автозагрузка при инициализации
    load_from_file(".env");
}

bool EnvFileHandler::load_from_file(const std::string& file_path) {
    if (!load_from_file_internal(file_path)) {
        LOG_WARNING("[EnvFileHandler] Не удалось загрузить .env файл: " + file_path);
        return false;
    }
    return true;
}

bool EnvFileHandler::save_to_file(const std::string& file_path) const {
    if (!save_to_file_internal(file_path)) {
        LOG_ERROR("[EnvFileHandler] Не удалось сохранить .env файл: " + file_path);
        return false;
    }
    return true;
}

void EnvFileHandler::set(const std::string& key, const std::string& value) {
    env_vars_[key] = value;
    update_environment(key, value);
}

std::string EnvFileHandler::get(const std::string& key, const std::string& default_value) const {
    auto it = env_vars_.find(key);
    if (it != env_vars_.end()) {
        return it->second;
    }
    return default_value;
}

bool EnvFileHandler::has(const std::string& key) const {
    return env_vars_.find(key) != env_vars_.end();
}

void EnvFileHandler::remove(const std::string& key) {
    env_vars_.erase(key);
    #ifdef _WIN32
        _putenv_s(key.c_str(), "");
    #else
        unsetenv(key.c_str());
    #endif
}

bool EnvFileHandler::update_environment(const std::string& key, const std::string& value) {
    #ifdef _WIN32
        return _putenv_s(key.c_str(), value.c_str()) == 0;
    #else
        return setenv(key.c_str(), value.c_str(), 1) == 0;
    #endif
}

const std::unordered_map<std::string, std::string>& EnvFileHandler::get_all() const {
    return env_vars_;
}

bool EnvFileHandler::load_from_file_internal(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        LOG_DEBUG("[EnvFileHandler] .env файл не существует: " + file_path);
        return false;
    }

    std::string line;
    int line_number = 0;

    while (std::getline(file, line)) {
        line_number++;

        // Удалить комментарии и пустые строки
        size_t comment_pos = line.find('#');
        if (comment_pos != std::string::npos) {
            line = line.substr(0, comment_pos);
        }
        line.erase(line.find_last_not_of(" \t\n\r") + 1);

        if (line.empty()) {
            continue;
        }

        // Разбить на ключ и значение
        size_t equal_pos = line.find('=');
        if (equal_pos == std::string::npos) {
            LOG_WARNING("[EnvFileHandler] Неверный формат строки в " + file_path + ":" + std::to_string(line_number));
            continue;
        }

        std::string key = line.substr(0, equal_pos);
        std::string value = line.substr(equal_pos + 1);

        // Удалить пробелы вокруг ключа и значения
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        value.erase(0, value.find_first_not_of(" \t"));
        value.erase(value.find_last_not_of(" \t") + 1);

        // Установить переменную окружения
        env_vars_[key] = value;
        setenv(key.c_str(), value.c_str(), 1);
    }

    file.close();
    LOG_DEBUG("[EnvFileHandler] Загружено переменных: " + std::to_string(env_vars_.size()));
    return true;
}

bool EnvFileHandler::save_to_file_internal(const std::string& file_path) const {
    std::ofstream file(file_path);
    if (!file.is_open()) {
        LOG_ERROR("[EnvFileHandler] Не удалось открыть файл для записи: " + file_path);
        return false;
    }

    // Записать каждую переменную окружения
    for (const auto& [key, value] : env_vars_) {
        file << key << "=" << value << std::endl;
    }

    file.close();
    LOG_DEBUG("[EnvFileHandler] Сохранено переменных: " + std::to_string(env_vars_.size()));
    return true;
}

} // namespace core
} // namespace llama_gui
```

### 3. Интеграция в Settings

```cpp
// В settings.h
#include "env_file_handler.h"

// В приватных полях
std::unique_ptr<EnvFileHandler> env_handler_;

// В конструкторе
Settings::Settings() {
    // ... существующий код
    env_handler_ = std::make_unique<EnvFileHandler>();
}

// Методы доступа к ключам
std::string Settings::get_openrouter_api_key() const {
    return env_handler_->get("OPENROUTER_API_KEY", "");
}

bool Settings::update_openrouter_api_key(const std::string& api_key) {
    if (env_handler_) {
        env_handler_->set("OPENROUTER_API_KEY", api_key);
        return env_handler_->save_to_file(".env");
    }
    return false;
}
```

### 4. Обновление UI

```cpp
// При вводе ключа
if (ImGui::InputText("##api_key", api_key_buffer_, sizeof(api_key_buffer_), ImGuiInputTextFlags_Password)) {
    openrouter_client_->set_api_key(api_key_buffer_);
    // Сохраняем ключ в .env
    settings_.update_openrouter_api_key(api_key_buffer_);
}
```

### 5. Обновление CMakeLists.txt

```cmake
set(SOURCES
    # ... существующие файлы
    src/core/env_file_handler.cpp
    # ... остальные файлы
)
```

### 6. Обновление .gitignore

```gitignore
# Environment variables (API keys)
.env
```

## Важные замечания

1. **Ключи не должны быть сериализованы** в settings.json/INI - удалите поля `api_key` из структур настроек
2. **Автозагрузка** при создании EnvFileHandler
3. **Автосохранение** при изменении через метод `set()`
4. **Кроссплатформенность** - поддержка Windows (setenv/_putenv_s) и Unix (setenv/unsetenv)
5. **Формат файла** - `KEY=VALUE` с поддержкой комментариев `#`

## Пример использования

### Создание файла .env:
```bash
cat > .env << EOF
OPENROUTER_API_KEY=sk-or-v1-...
KILOCODE_API_KEY=kilo-...
GLM_API_KEY=...
EOF
```

### Получение ключа:
```cpp
std::string api_key = settings.get_openrouter_api_key();
```

### Обновление ключа:
```cpp
if (settings.update_openrouter_api_key(new_key)) {
    std::cout << "Ключ успешно сохранён в .env" << std::endl;
}
```

## Преимущества

- Единый источник секретов
- Ключи не коммитятся в git (через .gitignore)
- Простая интеграция через стандартные функции setenv/unsetenv
- Поддержка кроссплатформенности
- Логирование ошибок через существующую систему логирования
