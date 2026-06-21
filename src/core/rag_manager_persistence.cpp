#include "../include/core/rag_manager.h"
#include "../include/core/embedding_generator.h"
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <nlohmann/json.hpp>

#ifdef USE_FAISS
#include <faiss/IndexFlat.h>
#include <faiss/index_io.h>
#endif

namespace llama_gui {
namespace core {

std::string RagManager::get_default_index_path() const {
    // Сохраняем индекс в домашней директории пользователя
    const char* home = getenv("HOME");
    if (!home) {
        home = ".";
    }

    std::string index_dir = std::string(home) + "/.llama-gui/rag_indexes/";

    // Создаём директорию если не существует (создаём всё дерево директорий)
    // Используем system с mkdir -p для надёжности
    std::string mkdir_cmd = "mkdir -p \"" + index_dir + "\"";
    int result = system(mkdir_cmd.c_str());
    (void)result; // Игнорируем результат - директория может уже существовать

    return index_dir + "rag_index.faiss";
}

bool RagManager::has_persistent_index() const {
    // Сначала пробуем получить путь из текущего профиля
    std::string index_path = profile_manager_.get_current_index_path();
    
    // Если профиль не установлен или путь пустой, используем путь по умолчанию
    if (index_path.empty()) {
        index_path = get_default_index_path();
    }
    
    std::string metadata_path = index_path + ".metadata.json";

    struct stat buffer;
    bool index_exists = (stat(index_path.c_str(), &buffer) == 0);
    bool metadata_exists = (stat(metadata_path.c_str(), &buffer) == 0);

    return index_exists && metadata_exists;
}

bool RagManager::save_index(const std::string& index_path) {
    // Если путь не указан, используем путь из текущего профиля
    std::string path = index_path.empty() ? profile_manager_.get_current_index_path() : index_path;
    
    // Если профиль не установлен или путь пустой, используем путь по умолчанию
    if (path.empty()) {
        path = get_default_index_path();
    }
    
    std::string metadata_path = path + ".metadata.json";

    std::cout << "[RAG PERSISTENCE] Saving index to: " << path << std::endl;
    std::cout << "[RAG PERSISTENCE] Metadata to: " << metadata_path << std::endl;

#ifdef USE_FAISS
    if (!external_docs_index_ || external_chunks_.empty()) {
        std::cerr << "[RAG PERSISTENCE] Error: No index or chunks to save" << std::endl;
        return false;
    }

    try {
        // Сохраняем FAISS индекс
        faiss::write_index(external_docs_index_.get(), path.c_str());
        std::cout << "[RAG PERSISTENCE] FAISS index saved successfully" << std::endl;

        // Сохраняем метаданные чанков в JSON
        nlohmann::json metadata;
        metadata["version"] = 1;
        metadata["chunk_count"] = external_chunks_.size();
        metadata["embedding_dimension"] = EMBEDDING_DIMENSION;

        // Сохраняем информацию о каждом чанке (без эмбеддингов - они в индексе)
        nlohmann::json chunks_json = nlohmann::json::array();
        for (const auto& chunk : external_chunks_) {
            nlohmann::json chunk_meta;
            chunk_meta["content"] = chunk.content;
            chunk_meta["document_id"] = chunk.document_id;
            chunk_meta["chunk_index"] = chunk.chunk_index;
            chunks_json.push_back(chunk_meta);
        }

        metadata["chunks"] = chunks_json;

        // Записываем JSON
        std::ofstream meta_file(metadata_path);
        if (!meta_file.is_open()) {
            std::cerr << "[RAG PERSISTENCE] Error: Cannot open metadata file" << std::endl;
            return false;
        }

        meta_file << metadata.dump(2);  // Красивый JSON с отступами
        meta_file.close();

        std::cout << "[RAG PERSISTENCE] Metadata saved successfully ("
                  << external_chunks_.size() << " chunks)" << std::endl;

        // Обновляем информацию в профиле
        profile_manager_.update_current_profile_chunk_count(static_cast<int>(external_chunks_.size()));

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[RAG PERSISTENCE] Error: Failed to save index: " << e.what() << std::endl;
        return false;
    }
#else
    std::cerr << "[RAG PERSISTENCE] Error: FAISS not available" << std::endl;
    return false;
#endif
}

bool RagManager::load_index(const std::string& index_path) {
    std::string path = index_path.empty() ? get_default_index_path() : index_path;
    std::string metadata_path = path + ".metadata.json";

    std::cout << "[RAG PERSISTENCE] Loading index from: " << path << std::endl;
    std::cout << "[RAG PERSISTENCE] Metadata from: " << metadata_path << std::endl;

    // Проверяем существование файлов
    struct stat buffer;
    if (stat(path.c_str(), &buffer) != 0) {
        std::cout << "[RAG PERSISTENCE] Index file does not exist" << std::endl;
        return false;
    }

    if (stat(metadata_path.c_str(), &buffer) != 0) {
        std::cout << "[RAG PERSISTENCE] Metadata file does not exist" << std::endl;
        return false;
    }

#ifdef USE_FAISS
    try {
        // Загружаем метаданные
        std::ifstream meta_file(metadata_path);
        if (!meta_file.is_open()) {
            std::cerr << "[RAG PERSISTENCE] Error: Cannot open metadata file" << std::endl;
            return false;
        }

        nlohmann::json metadata = nlohmann::json::parse(meta_file);
        meta_file.close();

        int chunk_count = metadata.value("chunk_count", 0);
        int embedding_dim = metadata.value("embedding_dimension", EMBEDDING_DIMENSION);

        std::cout << "[RAG PERSISTENCE] Metadata loaded: " << chunk_count << " chunks, "
                  << embedding_dim << " dimensions" << std::endl;

        // Загружаем FAISS индекс
        external_docs_index_ = std::unique_ptr<faiss::Index>(faiss::read_index(path.c_str(), 0));

        if (!external_docs_index_) {
            std::cerr << "[RAG PERSISTENCE] Error: Failed to load FAISS index" << std::endl;
            return false;
        }

        // Проверяем размерность
        if (external_docs_index_->d != embedding_dim) {
            std::cerr << "[RAG PERSISTENCE] Error: Embedding dimension mismatch. Expected: "
                      << embedding_dim << ", got: " << external_docs_index_->d << std::endl;
            return false;
        }

        std::cout << "[RAG PERSISTENCE] FAISS index loaded successfully" << std::endl;

        // Загружаем чанки
        external_chunks_.clear();
        external_chunks_.reserve(chunk_count);

        const auto& chunks_json = metadata["chunks"];
        for (const auto& chunk_meta : chunks_json) {
            RagChunk chunk;
            chunk.content = chunk_meta["content"].get<std::string>();
            chunk.document_id = chunk_meta["document_id"].get<std::string>();
            chunk.chunk_index = chunk_meta["chunk_index"].get<int>();
            // Эмбеддинги не загружаем - они уже в FAISS индексе
            // При необходимости можно восстановить из индекса, но это дорого

            external_chunks_.push_back(std::move(chunk));
        }

        std::cout << "[RAG PERSISTENCE] Chunks loaded successfully: " << external_chunks_.size() << std::endl;

        // === ИНИЦИАЛИЗАЦИЯ CHAT_HISTORY_INDEX_ ===
        // После загрузки профиля нужно убедиться, что chat_history_index_ корректно инициализирован
        int actual_dim = external_docs_index_->d;
        if (!chat_history_index_ || chat_history_chunks_.empty()) {
            chat_history_index_ = create_optimized_index(actual_dim);
            std::cout << "[RAG PERSISTENCE] chat_history_index_ initialized with dimension " << actual_dim << std::endl;
        }
        // =========================================

        std::cout << "[RAG PERSISTENCE] RAG index ready for use!" << std::endl;

        return true;

    } catch (const std::exception& e) {
        std::cerr << "[RAG PERSISTENCE] Error: Failed to load index: " << e.what() << std::endl;
        return false;
    }
#else
    std::cerr << "[RAG PERSISTENCE] Error: FAISS not available" << std::endl;
    return false;
#endif
}

void RagManager::clear_all_indexes() {
    std::cout << "[RAG PERSISTENCE] Clearing all indexes..." << std::endl;
    
#ifdef USE_FAISS
    external_docs_index_.reset();
    chat_history_index_.reset();
#endif
    
    external_chunks_.clear();
    chat_history_chunks_.clear();

    // Очищаем кэш эмбеддингов
    query_embedding_cache_.clear();

    std::cout << "[RAG PERSISTENCE] All indexes cleared" << std::endl;
}

// ============================================================================
// Управление профилями индексов
// ============================================================================

bool RagManager::initialize_profile_manager(const std::string& profiles_directory) {
    std::cout << "[RAG PROFILE] Initializing profile manager..." << std::endl;
    return profile_manager_.initialize(profiles_directory);
}

bool RagManager::create_index_profile(const std::string& profile_name, const std::string& source_directory) {
    std::cout << "[RAG PROFILE] Creating profile: " << profile_name << std::endl;
    return profile_manager_.create_profile(profile_name, source_directory);
}

bool RagManager::switch_index_profile(const std::string& profile_name) {
    std::cout << "[RAG PROFILE] Switching to profile: " << profile_name << std::endl;
    
    // Переключаем профиль
    if (!profile_manager_.set_current_profile(profile_name)) {
        std::cerr << "[RAG PROFILE] Failed to switch profile" << std::endl;
        return false;
    }

    // Очищаем текущий индекс
    clear_all_indexes();

    // Загружаем индекс для нового профиля
    return load_index_for_current_profile();
}

bool RagManager::delete_index_profile(const std::string& profile_name, bool delete_index_file) {
    std::cout << "[RAG PROFILE] Deleting profile: " << profile_name << std::endl;
    return profile_manager_.delete_profile(profile_name, delete_index_file);
}

std::vector<std::string> RagManager::get_index_profile_names() const {
    return profile_manager_.get_profile_names();
}

std::string RagManager::get_current_index_profile() const {
    return profile_manager_.get_current_profile();
}

std::string RagManager::get_current_index_path() const {
    return profile_manager_.get_current_index_path();
}

bool RagManager::load_index_for_current_profile() {
    std::string index_path = profile_manager_.get_current_index_path();
    std::string metadata_path = profile_manager_.get_current_metadata_path();

    if (index_path.empty() || metadata_path.empty()) {
        std::cerr << "[RAG PROFILE] Error: Current profile has no index path" << std::endl;
        return false;
    }

    std::cout << "[RAG PROFILE] Loading index for profile from: " << index_path << std::endl;
    return load_index(index_path);
}

} // namespace core
} // namespace llama_gui
