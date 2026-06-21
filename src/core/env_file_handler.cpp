#include "../include/core/env_file_handler.h"
#include "../include/core/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace llama_gui {
namespace core {

EnvFileHandler::EnvFileHandler() {
    // Загрузить .env файл при инициализации
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
    // Удалить переменную из окружения (Unix: unsetenv, Windows: _unsetenv)
    #ifdef _WIN32
        _putenv_s(key.c_str(), "");
    #else
        unsetenv(key.c_str());
    #endif
}

bool EnvFileHandler::update_environment(const std::string& key, const std::string& value) {
    // Обновить переменную окружения в системе
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
