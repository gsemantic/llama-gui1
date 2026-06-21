#include "core/rag_index_profile.h"
#include <nlohmann/json.hpp>
#include <sys/stat.h>
#include <chrono>
#include <algorithm>

namespace fs = std::filesystem;

namespace llama_gui {
namespace core {

RagIndexProfileManager::RagIndexProfileManager() = default;

RagIndexProfileManager::~RagIndexProfileManager() = default;

bool RagIndexProfileManager::initialize(const std::string& profiles_directory) {
    // Устанавливаем директорию профилей
    if (profiles_directory.empty()) {
        const char* home = getenv("HOME");
        if (!home) {
            home = ".";
        }
        profiles_directory_ = std::string(home) + "/.llama-gui/rag_profiles/";
    } else {
        profiles_directory_ = profiles_directory;
    }

    std::cout << "[RAG PROFILE] Initializing with directory: " << profiles_directory_ << std::endl;

    // Создаём директорию если не существует
    try {
        if (!fs::exists(profiles_directory_)) {
            fs::create_directories(profiles_directory_);
            std::cout << "[RAG PROFILE] Created profiles directory" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[RAG PROFILE] Error creating directory: " << e.what() << std::endl;
        return false;
    }

    // Загружаем существующие профили
    load_all_profiles();

    // Загружаем конфигурацию менеджера (текущий профиль)
    load_manager_config();

    std::cout << "[RAG PROFILE] Initialized with " << profiles_.size() << " profile(s)" << std::endl;
    if (!current_profile_.empty()) {
        std::cout << "[RAG PROFILE] Current profile: " << current_profile_ << std::endl;
    }

    return true;
}

std::vector<std::string> RagIndexProfileManager::get_profile_names() const {
    std::vector<std::string> names;
    names.reserve(profiles_.size());

    for (const auto& [name, profile] : profiles_) {
        names.push_back(name);
    }

    // Сортируем по имени
    std::sort(names.begin(), names.end());
    return names;
}

const RagIndexProfile* RagIndexProfileManager::get_profile(const std::string& profile_name) const {
    auto it = profiles_.find(profile_name);
    if (it != profiles_.end()) {
        return &(it->second);
    }
    return nullptr;
}

std::string RagIndexProfileManager::get_current_profile() const {
    return current_profile_;
}

bool RagIndexProfileManager::set_current_profile(const std::string& profile_name) {
    if (profile_name.empty()) {
        std::cerr << "[RAG PROFILE] Error: Empty profile name" << std::endl;
        return false;
    }

    // Проверяем существование профиля
    if (!has_profile(profile_name)) {
        std::cerr << "[RAG PROFILE] Error: Profile does not exist: " << profile_name << std::endl;
        return false;
    }

    current_profile_ = profile_name;
    save_manager_config();

    std::cout << "[RAG PROFILE] Switched to profile: " << profile_name << std::endl;
    return true;
}

bool RagIndexProfileManager::create_profile(const std::string& profile_name,
                                           const std::string& source_directory) {
    if (profile_name.empty()) {
        std::cerr << "[RAG PROFILE] Error: Empty profile name" << std::endl;
        return false;
    }

    // Проверяем, не существует ли уже профиль
    if (has_profile(profile_name)) {
        std::cerr << "[RAG PROFILE] Error: Profile already exists: " << profile_name << std::endl;
        return false;
    }

    // Создаём новый профиль
    RagIndexProfile profile(profile_name);
    profile.source_directory = source_directory;
    profile.created_at = std::time(nullptr);
    profile.modified_at = profile.created_at;

    // Генерируем путь к индексу
    std::string safe_name = profile_name;
    // Заменяем пробелы и специальные символы на подчёркивание
    for (char& c : safe_name) {
        if (c == ' ' || c == '/' || c == '\\' || c == ':') {
            c = '_';
        }
    }

    profile.index_path = profiles_directory_ + safe_name + "_index.faiss";
    profile.metadata_path = profile.index_path + ".metadata.json";

    // Сохраняем профиль
    if (!save_profile(profile)) {
        std::cerr << "[RAG PROFILE] Error: Failed to save profile" << std::endl;
        return false;
    }

    // Добавляем в список
    profiles_[profile_name] = profile;

    // Устанавливаем как текущий
    set_current_profile(profile_name);

    std::cout << "[RAG PROFILE] Created profile: " << profile_name << std::endl;
    return true;
}

bool RagIndexProfileManager::delete_profile(const std::string& profile_name, bool delete_index_file) {
    if (profile_name.empty()) {
        std::cerr << "[RAG PROFILE] Error: Empty profile name" << std::endl;
        return false;
    }

    // Проверяем существование
    if (!has_profile(profile_name)) {
        std::cerr << "[RAG PROFILE] Error: Profile does not exist: " << profile_name << std::endl;
        return false;
    }

    // Если удаляем текущий профиль, переключаемся на первый доступный
    bool is_current = (current_profile_ == profile_name);
    if (is_current) {
        // Находим первый профиль, который не является удаляемым
        std::string new_current;
        for (const auto& [name, profile] : profiles_) {
            if (name != profile_name) {
                new_current = name;
                break;
            }
        }
        // Переключаемся на новый профиль (или очищаем current_profile если других нет)
        if (!new_current.empty()) {
            std::cout << "[RAG PROFILE] Switching from deleted profile to: " << new_current << std::endl;
            current_profile_ = new_current;
        } else {
            std::cout << "[RAG PROFILE] No other profiles available, clearing current profile" << std::endl;
            current_profile_ = "";
        }
    }

    // Удаляем файлы индекса если запрошено
    if (delete_index_file) {
        auto it = profiles_.find(profile_name);
        if (it != profiles_.end()) {
            try {
                if (fs::exists(it->second.index_path)) {
                    fs::remove(it->second.index_path);
                    std::cout << "[RAG PROFILE] Deleted index file: " << it->second.index_path << std::endl;
                }
                if (fs::exists(it->second.metadata_path)) {
                    fs::remove(it->second.metadata_path);
                    std::cout << "[RAG PROFILE] Deleted metadata file: " << it->second.metadata_path << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[RAG PROFILE] Error deleting files: " << e.what() << std::endl;
            }
        }
    }

    // ВАЖНО: Всегда удаляем файл профиля _profile.json
    {
        auto it = profiles_.find(profile_name);
        if (it != profiles_.end()) {
            try {
                std::string profile_file = it->second.index_path;
                if (profile_file.empty()) {
                    // Если index_path пустой, строим имя файла из имени профиля
                    std::string safe_name = profile_name;
                    for (char& c : safe_name) {
                        if (c == ' ' || c == '/' || c == '\\' || c == ':') c = '_';
                    }
                    profile_file = profiles_directory_ + "/" + safe_name + "_index.faiss";
                }
                
                // Заменяем _index.faiss на _profile.json
                std::string profile_json_path = profile_file;
                size_t pos = profile_json_path.find("_index.faiss");
                if (pos != std::string::npos) {
                    profile_json_path.replace(pos, std::string("_index.faiss").length(), "_profile.json");
                } else {
                    // Если не нашли _index.faiss, просто добавляем расширение
                    profile_json_path += ".metadata.json";
                    profile_json_path = profile_json_path.substr(0, profile_json_path.find(".metadata.json"));
                    profile_json_path += "_profile.json";
                }
                
                if (fs::exists(profile_json_path)) {
                    fs::remove(profile_json_path);
                    std::cout << "[RAG PROFILE] Deleted profile JSON file: " << profile_json_path << std::endl;
                } else {
                    std::cout << "[RAG PROFILE] Profile JSON file does not exist: " << profile_json_path << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[RAG PROFILE] Error deleting profile file: " << e.what() << std::endl;
            }
        }
    }

    // Удаляем профиль из списка
    profiles_.erase(profile_name);

    // Сохраняем конфигурацию
    save_manager_config();

    std::cout << "[RAG PROFILE] Deleted profile: " << profile_name << std::endl;
    return true;
}

bool RagIndexProfileManager::save_profile(const RagIndexProfile& profile) {
    std::string config_path = get_profile_config_path(profile.name);

    try {
        nlohmann::json json;
        json["name"] = profile.name;
        json["index_path"] = profile.index_path;
        json["metadata_path"] = profile.metadata_path;
        json["source_directory"] = profile.source_directory;
        json["documents"] = profile.documents;
        json["chunk_count"] = profile.chunk_count;
        json["created_at"] = profile.created_at;
        json["modified_at"] = profile.modified_at;

        std::ofstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "[RAG PROFILE] Error: Cannot open config file for writing" << std::endl;
            return false;
        }

        file << json.dump(2);
        file.close();

        std::cout << "[RAG PROFILE] Saved profile: " << profile.name << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[RAG PROFILE] Error saving profile: " << e.what() << std::endl;
        return false;
    }
}

RagIndexProfile* RagIndexProfileManager::load_profile(const std::string& profile_name) {
    if (!has_profile(profile_name)) {
        return nullptr;
    }

    std::string config_path = get_profile_config_path(profile_name);

    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "[RAG PROFILE] Error: Cannot open config file for reading" << std::endl;
            return nullptr;
        }

        nlohmann::json json = nlohmann::json::parse(file);
        file.close();

        // Обновляем профиль в памяти
        auto it = profiles_.find(profile_name);
        if (it != profiles_.end()) {
            it->second.name = json.value("name", profile_name);
            it->second.index_path = json.value("index_path", "");
            it->second.metadata_path = json.value("metadata_path", "");
            it->second.source_directory = json.value("source_directory", "");
            it->second.documents = json.value("documents", std::vector<std::string>{});
            it->second.chunk_count = json.value("chunk_count", 0);
            it->second.created_at = json.value("created_at", int64_t(0));
            it->second.modified_at = json.value("modified_at", int64_t(0));

            return &(it->second);
        }

        return nullptr;
    } catch (const std::exception& e) {
        std::cerr << "[RAG PROFILE] Error loading profile: " << e.what() << std::endl;
        return nullptr;
    }
}

std::string RagIndexProfileManager::get_current_index_path() const {
    if (current_profile_.empty()) {
        return "";
    }

    auto it = profiles_.find(current_profile_);
    if (it != profiles_.end()) {
        return it->second.index_path;
    }

    return "";
}

std::string RagIndexProfileManager::get_current_metadata_path() const {
    if (current_profile_.empty()) {
        return "";
    }

    auto it = profiles_.find(current_profile_);
    if (it != profiles_.end()) {
        return it->second.metadata_path;
    }

    return "";
}

bool RagIndexProfileManager::has_profile(const std::string& profile_name) const {
    return profiles_.find(profile_name) != profiles_.end();
}

void RagIndexProfileManager::add_document_to_current_profile(const std::string& document_path) {
    if (current_profile_.empty()) {
        std::cerr << "[RAG PROFILE] Error: No current profile selected" << std::endl;
        return;
    }

    auto it = profiles_.find(current_profile_);
    if (it == profiles_.end()) {
        std::cerr << "[RAG PROFILE] Error: Current profile not found" << std::endl;
        return;
    }

    // Проверяем, не добавлен ли уже документ
    for (const auto& doc : it->second.documents) {
        if (doc == document_path) {
            return; // Уже добавлен
        }
    }

    // Добавляем документ
    it->second.documents.push_back(document_path);
    it->second.modified_at = std::time(nullptr);

    // Сохраняем профиль
    save_profile(it->second);
}

void RagIndexProfileManager::update_current_profile_chunk_count(int chunk_count) {
    if (current_profile_.empty()) {
        return;
    }

    auto it = profiles_.find(current_profile_);
    if (it == profiles_.end()) {
        return;
    }

    it->second.chunk_count = chunk_count;
    it->second.modified_at = std::time(nullptr);

    // Сохраняем профиль
    save_profile(it->second);
}

void RagIndexProfileManager::load_all_profiles() {
    profiles_.clear();

    try {
        if (!fs::exists(profiles_directory_)) {
            return;
        }

        for (const auto& entry : fs::directory_iterator(profiles_directory_)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                std::string filename = entry.path().filename().string();
                
                // Пропускаем конфигурационный файл менеджера
                if (filename == "profile_manager_config.json") {
                    continue;
                }

                try {
                    std::ifstream file(entry.path());
                    if (!file.is_open()) {
                        continue;
                    }

                    nlohmann::json json = nlohmann::json::parse(file);
                    file.close();

                    std::string name = json.value("name", "");
                    if (!name.empty()) {
                        RagIndexProfile profile;
                        profile.name = name;
                        profile.index_path = json.value("index_path", "");
                        profile.metadata_path = json.value("metadata_path", "");
                        profile.source_directory = json.value("source_directory", "");
                        profile.documents = json.value("documents", std::vector<std::string>{});
                        profile.chunk_count = json.value("chunk_count", 0);
                        profile.created_at = json.value("created_at", int64_t(0));
                        profile.modified_at = json.value("modified_at", int64_t(0));

                        profiles_[name] = profile;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "[RAG PROFILE] Error loading profile " << filename << ": " << e.what() << std::endl;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[RAG PROFILE] Error loading profiles: " << e.what() << std::endl;
    }
}

void RagIndexProfileManager::save_manager_config() const {
    std::string config_path = get_config_path();

    try {
        nlohmann::json json;
        json["current_profile"] = current_profile_;

        std::ofstream file(config_path);
        if (!file.is_open()) {
            std::cerr << "[RAG PROFILE] Error: Cannot open manager config for writing" << std::endl;
            return;
        }

        file << json.dump(2);
        file.close();
    } catch (const std::exception& e) {
        std::cerr << "[RAG PROFILE] Error saving manager config: " << e.what() << std::endl;
    }
}

void RagIndexProfileManager::load_manager_config() {
    std::string config_path = get_config_path();

    try {
        std::ifstream file(config_path);
        if (!file.is_open()) {
            return; // Файл не существует, используем профиль по умолчанию
        }

        nlohmann::json json = nlohmann::json::parse(file);
        file.close();

        current_profile_ = json.value("current_profile", "");
    } catch (const std::exception& e) {
        std::cerr << "[RAG PROFILE] Error loading manager config: " << e.what() << std::endl;
    }
}

std::string RagIndexProfileManager::get_config_path() const {
    return profiles_directory_ + "profile_manager_config.json";
}

std::string RagIndexProfileManager::get_profile_config_path(const std::string& profile_name) const {
    std::string safe_name = profile_name;
    for (char& c : safe_name) {
        if (c == ' ' || c == '/' || c == '\\' || c == ':') {
            c = '_';
        }
    }
    return profiles_directory_ + safe_name + "_profile.json";
}

} // namespace core
} // namespace llama_gui
