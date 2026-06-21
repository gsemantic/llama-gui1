#pragma once

#include <string>
#include <unordered_map>

namespace llama_gui {
namespace core {

/**
 * @brief Обработчик файла .env для хранения секретных ключей
 *
 * Использование:
 * - Загрузка переменных окружения из .env файла при старте
 * - Запись и обновление переменных окружения
 * - Чтение ключей через std::getenv() / getenv()
 */
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
     * @brief Удалить переменную
     * @param key Имя переменной
     */
    void remove(const std::string& key);

    /**
     * @brief Обновить переменную окружения в системе (через setenv)
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

    /**
     * @brief Загрузить переменные окружения из файла в память
     * @param file_path Путь к .env файлу
     * @return true если успешно
     */
    bool load_from_file_internal(const std::string& file_path);

    /**
     * @brief Сохранить переменные окружения из памяти в файл
     * @param file_path Путь к .env файлу
     * @return true если успешно
     */
    bool save_to_file_internal(const std::string& file_path) const;
};

} // namespace core
} // namespace llama_gui
