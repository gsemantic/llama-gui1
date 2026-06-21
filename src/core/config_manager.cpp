#include "../include/core/config_manager.h"
#include <iostream>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace llama_gui {
namespace core {

ConfigManager::ConfigManager() 
    : settings_(nullptr) {
}

ConfigManager::~ConfigManager() {
    // Не сохраняем при разрушении, так как это ответственность владельца Settings
}

void ConfigManager::setSettings(Settings& settings) {
    settings_ = &settings;
}

bool ConfigManager::initialize() {
    std::cout << "======================================================" << std::endl;
    std::cout << "  ConfigManager: Инициализация" << std::endl;
    std::cout << "======================================================" << std::endl;

    // Проверка на установленный Settings
    if (!settings_) {
        std::cerr << "ConfigManager: Ошибка - Settings не установлен! Вызовите setSettings() перед initialize()" << std::endl;
        return false;
    }

    // Шаг 1: Создаём директории
    if (!createConfigDirectories()) {
        std::cerr << "ConfigManager: Ошибка создания директорий" << std::endl;
        return false;
    }

    // Шаг 2: Валидируем пути
    if (!validateConfigPaths()) {
        std::cerr << "ConfigManager: Ошибка валидации путей" << std::endl;
        return false;
    }

    // Шаг 3: Синхронизируем настройки
    std::cout << "\nConfigManager: Синхронизация настроек..." << std::endl;
    if (!settings_->synchronize_at_startup()) {
        std::cerr << "ConfigManager: Предупреждение: не удалось загрузить настройки, используются значения по умолчанию" << std::endl;
    }

    // Шаг 4: Синхронизируем current_profile_ с Settings
    current_profile_ = settings_->get_current_profile_name();
    if (!current_profile_.empty()) {
        std::cout << "ConfigManager: Текущий профиль установлен: " << current_profile_ << std::endl;
    }

    // Шаг 5: Валидируем настройки
    if (!validateSettings()) {
        std::cerr << "ConfigManager: Предупреждение: настройки не прошли валидацию" << std::endl;
        std::cerr << getSettingsValidationReport() << std::endl;
    }

    initialized_ = true;
    std::cout << "\n✓ ConfigManager успешно инициализирован" << std::endl;
    std::cout << "======================================================" << std::endl;
    return true;
}

Settings& ConfigManager::getSettings() {
    if (!settings_) {
        static Settings dummy;
        return dummy;
    }
    return *settings_;
}

const Settings& ConfigManager::getSettings() const {
    if (!settings_) {
        static Settings dummy;
        return dummy;
    }
    return *settings_;
}

bool ConfigManager::saveSettings(bool sync_profiles) {
    if (!initialized_) {
        std::cerr << "ConfigManager: Не инициализирован" << std::endl;
        return false;
    }

    bool success = true;

    // Сохраняем в settings.ini
    std::cout << "ConfigManager: Сохранение settings.ini..." << std::endl;
    if (!settings_->save_to_ini(settings_->get_ini_file_path())) {
        std::cerr << "ConfigManager: Ошибка сохранения settings.ini" << std::endl;
        success = false;
    }

    // Синхронизируем с профилем
    if (sync_profiles) {
        std::string profile_name = current_profile_;
        if (profile_name.empty()) {
            profile_name = "default";
        }
        std::cout << "ConfigManager: Сохранение профиля: " << profile_name << std::endl;
        if (!saveProfile(profile_name)) {
            std::cerr << "ConfigManager: Ошибка сохранения профиля" << std::endl;
            success = false;
        }
    }

    if (success) {
        std::cout << "✓ ConfigManager: Настройки сохранены" << std::endl;
    }

    return success;
}

bool ConfigManager::loadProfile(const std::string& profile_name) {
    if (!initialized_) {
        std::cerr << "ConfigManager: Не инициализирован" << std::endl;
        return false;
    }

    std::cout << "ConfigManager: Загрузка профиля: " << profile_name << std::endl;
    if (settings_->load_profile(profile_name)) {
        current_profile_ = profile_name;
        std::cout << "✓ ConfigManager: Профиль загружен" << std::endl;
        return true;
    }

    std::cerr << "ConfigManager: Ошибка загрузки профиля" << std::endl;
    return false;
}

bool ConfigManager::saveProfile(const std::string& profile_name) {
    if (!initialized_) {
        std::cerr << "ConfigManager: Не инициализирован" << std::endl;
        return false;
    }

    std::string name = profile_name;
    if (name.empty()) {
        name = current_profile_;
        if (name.empty()) {
            name = "default";
        }
    }

    std::cout << "ConfigManager: Сохранение профиля: " << name << std::endl;
    if (settings_->save_profile(name)) {
        current_profile_ = name;
        std::cout << "✓ ConfigManager: Профиль сохранён" << std::endl;
        return true;
    }

    std::cerr << "ConfigManager: Ошибка сохранения профиля" << std::endl;
    return false;
}

std::vector<std::string> ConfigManager::listProfiles() const {
    return settings_->list_profiles();
}

std::string ConfigManager::getCurrentProfileName() const {
    return current_profile_;
}

bool ConfigManager::resetToDefaults() {
    std::cout << "ConfigManager: Сброс к настройкам по умолчанию..." << std::endl;
    current_profile_.clear();
    return settings_->reset_to_defaults();
}

std::string ConfigManager::getSettingsIniPath() const {
    return settings_->get_ini_file_path();
}

std::string ConfigManager::getProfilesDirectory() const {
    return settings_->get_profiles_directory();
}

void ConfigManager::setProfilesDirectory(const std::string& path) {
    settings_->set_profiles_directory(path);
}

bool ConfigManager::validateSettings() const {
    return settings_->validate();
}

std::string ConfigManager::getSettingsValidationReport() const {
    return settings_->get_validation_errors();
}

std::string ConfigManager::createBackup(const std::string& backup_path) {
    namespace fs = std::filesystem;

    std::string backup_dir = "backups/configs";
    if (!backup_path.empty()) {
        backup_dir = backup_path;
    }

    // Создаём директорию резервных копий
    if (!fs::exists(backup_dir)) {
        fs::create_directories(backup_dir);
    }

    // Генерируем имя файла с временной меткой
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y%m%d_%H%M%S");
    std::string timestamp = ss.str();

    std::string backup_file = backup_dir + "/settings_backup_" + timestamp + ".ini";

    // Копируем settings.ini
    std::string ini_path = settings_->get_ini_file_path();
    if (fs::exists(ini_path)) {
        try {
            fs::copy_file(ini_path, backup_file, fs::copy_options::overwrite_existing);
            std::cout << "ConfigManager: Резервная копия создана: " << backup_file << std::endl;
            return backup_file;
        } catch (const std::exception& e) {
            std::cerr << "ConfigManager: Ошибка создания резервной копии: " << e.what() << std::endl;
        }
    }

    return "";
}

bool ConfigManager::restoreFromBackup(const std::string& backup_path) {
    namespace fs = std::filesystem;

    if (!fs::exists(backup_path)) {
        std::cerr << "ConfigManager: Файл резервной копии не найден: " << backup_path << std::endl;
        return false;
    }

    std::string ini_path = settings_->get_ini_file_path();
    try {
        fs::copy_file(backup_path, ini_path, fs::copy_options::overwrite_existing);
        std::cout << "ConfigManager: Восстановление из резервной копии: " << backup_path << std::endl;
        return settings_->load_from_ini(ini_path);
    } catch (const std::exception& e) {
        std::cerr << "ConfigManager: Ошибка восстановления: " << e.what() << std::endl;
        return false;
    }
}

std::string ConfigManager::getDebugInfo() const {
    std::stringstream ss;
    
    ss << "======================================================\n";
    ss << "  ConfigManager Debug Info\n";
    ss << "======================================================\n";
    ss << "Initialized: " << (initialized_ ? "yes" : "no") << "\n";
    ss << "Current profile: " << (current_profile_.empty() ? "(none)" : current_profile_) << "\n";
    ss << "Settings INI path: " << settings_->get_ini_file_path() << "\n";
    ss << "Profiles directory: " << settings_->get_profiles_directory() << "\n";
    
    auto profiles = settings_->list_profiles();
    ss << "Available profiles: " << profiles.size() << "\n";
    for (const auto& p : profiles) {
        ss << "  - " << p << "\n";
    }
    
    ss << "\n";
    ss << settings_->get_debug_info();
    
    return ss.str();
}

bool ConfigManager::createConfigDirectories() {
    namespace fs = std::filesystem;

    std::cout << "ConfigManager: Создание директорий..." << std::endl;

    // Директория профилей
    std::string profiles_dir = settings_->get_profiles_directory();
    if (!fs::exists(profiles_dir)) {
        std::cout << "  Создаём директорию профилей: " << profiles_dir << std::endl;
        fs::create_directories(profiles_dir);
    }

    // Директория для INI файла
    std::string ini_path = settings_->get_ini_file_path();
    std::string ini_dir = ini_path.substr(0, ini_path.find_last_of('/'));
    if (!ini_dir.empty() && !fs::exists(ini_dir)) {
        std::cout << "  Создаём директорию настроек: " << ini_dir << std::endl;
        fs::create_directories(ini_dir);
    }

    // Директория резервных копий
    std::string backup_dir = "backups/configs";
    if (!fs::exists(backup_dir)) {
        std::cout << "  Создаём директорию резервных копий: " << backup_dir << std::endl;
        fs::create_directories(backup_dir);
    }

    return true;
}

bool ConfigManager::validateConfigPaths() const {
    // Проверяем, что пути не пустые
    if (settings_->get_ini_file_path().empty()) {
        std::cerr << "ConfigManager: Путь к settings.ini пустой" << std::endl;
        return false;
    }

    if (settings_->get_profiles_directory().empty()) {
        std::cerr << "ConfigManager: Путь к директории профилей пустой" << std::endl;
        return false;
    }

    return true;
}

} // namespace core
} // namespace llama_gui
