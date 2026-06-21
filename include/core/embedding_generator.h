#pragma once

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <mutex>

// llama.cpp headers
#include "llama.h"
#include "llama-cpp.h"

namespace llama_gui {
namespace core {

// === РЕЖИМЫ ЭМБЕДДИНГА ===
enum class EmbeddingMode {
    LocalOnly,      // Только локальная модель (gguf)
    CloudOnly,      // Только облачное API
    Hybrid,         // Гибридный: облако + локальный fallback
    Auto            // Авто: облако если доступно, иначе локально
};

// === ПРОВАЙДЕРЫ ОБЛАЧНЫХ ЭМБЕДДИНГОВ ===
enum class CloudEmbeddingProvider {
    None,
    HuggingFace,    // HuggingFace Inference API (бесплатно)
    OpenRouter,     // OpenRouter API (платно/бесплатно)
    Custom          // Кастомный API endpoint
};

// Конфигурация облачного провайдера
struct CloudEmbeddingConfig {
    CloudEmbeddingProvider provider = CloudEmbeddingProvider::None;
    std::string api_key = "";
    std::string endpoint_url = "";
    std::string model_name = "all-MiniLM-L6-v2";  // Модель для совместимости
    int timeout_ms = 30000;
    int max_retries = 2;
};

// Кэш для эмбеддингов
struct EmbeddingCacheEntry {
    std::vector<float> embedding;
    std::chrono::steady_clock::time_point last_access;
    size_t access_count = 0;
};

/**
 * @brief Универсальный генератор эмбеддингов с поддержкой локальных и облачных моделей
 * 
 * Поддерживает режимы:
 * - LocalOnly: только локальная gguf модель
 * - CloudOnly: только облачное API
 * - Hybrid: облако + локальный fallback
 * - Auto: облако если доступно, иначе локально
 * 
 * КРИТИЧЕСКИ ВАЖНО: Для совместимости облачных и локальных эмбеддингов
 * должна использоваться ОДНА И ТА ЖЕ модель (например, all-MiniLM-L6-v2)
 */
class EmbeddingGenerator {
public:
    explicit EmbeddingGenerator(const std::string& model_path = "");
    ~EmbeddingGenerator();

    // === Инициализация ===
    
    /**
     * @brief Загрузить локальную модель
     * @param model_path Путь к .gguf файлу
     * @return true если успешно
     */
    bool load_local_model(const std::string& model_path);
    
    /**
     * @brief Загрузить модель из пути, указанного в конструкторе
     * @return true если успешно
     */
    bool load_model();  // Для обратной совместимости

    /**
     * @brief Настроить облачный API
     * @param config Конфигурация облачного провайдера
     * @return true если успешно
     */
    bool configure_cloud_provider(const CloudEmbeddingConfig& config);

    /**
     * @brief Проверить доступность облачного API
     * @return true если API доступен
     */
    bool check_cloud_availability();

    // === Генерация эмбеддингов ===
    
    /**
     * @brief Сгенерировать эмбеддинг с использованием текущего режима
     * @param text Текст для эмбеддинга
     * @return Вектор эмбеддинга (размерность зависит от модели)
     */
    std::vector<float> generate_embedding(const std::string& text);

    /**
     * @brief Сгенерировать эмбеддинг через облачный API
     * @param text Текст для эмбеддинга
     * @return Вектор эмбеддинга
     */
    std::vector<float> generate_embedding_cloud(const std::string& text);

    /**
     * @brief Сгенерировать эмбеддинг через локальную модель
     * @param text Текст для эмбеддинга
     * @return Вектор эмбеддинга
     */
    std::vector<float> generate_embedding_local(const std::string& text);

    /**
     * @brief Выгрузить модель из памяти (освободить RAM)
     */
    void unload_model();

    /**
     * @brief Проверить, загружена ли модель
     */
    bool is_model_loaded() const { return local_model_loaded_; }

    /**
     * @brief Включить/выключить ленивую загрузку
     */
    void set_lazy_load(bool enable) { lazy_load_enabled_ = enable; }
    bool is_lazy_load_enabled() const { return lazy_load_enabled_; }

    // === Статус ===
    
    bool is_loaded() const { return local_model_loaded_ || cloud_configured_; }
    bool is_local_loaded() const { return local_model_loaded_; }
    bool is_cloud_configured() const { return cloud_configured_; }
    bool is_cloud_available() const { return cloud_available_; }
    
    int get_embedding_dimension() const;
    std::string get_model_name() const { return current_model_name_; }
    
    // === Настройки ===
    
    void set_mode(EmbeddingMode mode) { embedding_mode_ = mode; }
    EmbeddingMode get_mode() const { return embedding_mode_; }
    
    void set_cloud_priority(bool priority) { cloud_priority_ = priority; }
    bool get_cloud_priority() const { return cloud_priority_; }
    
    // === Кэширование ===
    
    void enable_cache(bool enable) { cache_enabled_ = enable; }
    bool is_cache_enabled() const { return cache_enabled_; }
    
    void set_cache_size(size_t max_entries) { max_cache_size_ = max_entries; }
    size_t get_cache_size() const { return cache_.size(); }
    
    void clear_cache();
    
    // === Статистика ===
    
    struct EmbeddingStats {
        size_t total_requests = 0;
        size_t cloud_requests = 0;
        size_t local_requests = 0;
        size_t cache_hits = 0;
        size_t failures = 0;
        double avg_latency_ms = 0.0;
    };
    
    EmbeddingStats get_stats() const { return stats_; }
    void reset_stats();

private:
    // === Конфигурация ===
    std::string local_model_path_;
    CloudEmbeddingConfig cloud_config_;
    EmbeddingMode embedding_mode_ = EmbeddingMode::Hybrid;
    bool cloud_priority_ = true;  // Приоритет облака в гибридном режиме
    
    // === Состояние ===
    bool local_model_loaded_ = false;
    bool cloud_configured_ = false;
    bool cloud_available_ = false;
    std::string current_model_name_;
    int embedding_dimension_ = 1024;  // BGE-M3 (динамически обновляется при загрузке)
    
    // === Локальная модель (llama.cpp) ===
    llama_model_ptr model_;           // Умный указатель на модель
    llama_context_ptr context_;       // Умный указатель на контекст
    int max_sequence_length_ = 512;
    bool has_encoder_ = false;        // Флаг наличия энкодера (для embedding моделей)
    enum llama_pooling_type pooling_type_ = LLAMA_POOLING_TYPE_NONE;

    // Ленивая загрузка
    bool lazy_load_enabled_ = true;   // Загружать модель только при необходимости
    mutable bool model_loading_ = false;  // Флаг процесса загрузки (для потокобезопасности)
    
    // === Кэш ===
    bool cache_enabled_ = true;
    size_t max_cache_size_ = 500;
    std::unordered_map<std::string, EmbeddingCacheEntry> cache_;

    // === Mutex для защиты модели (НЕ потокобезопасна!) ===
    mutable std::mutex model_mutex_;

    // === Статистика ===
    EmbeddingStats stats_;
    
    // === Внутренние методы ===
    std::vector<int> tokenize_text(const std::string& text);
    void normalize_embedding(std::vector<float>& embedding);
    
    // Кэш-методы
    std::vector<float> get_cached_embedding(const std::string& text);
    void cache_embedding(const std::string& text, const std::vector<float>& embedding);
    void cleanup_cache();
    std::string compute_text_hash(const std::string& text);
    
    // Облачные методы
    std::vector<float> call_huggingface_api(const std::string& text);
    std::vector<float> call_openrouter_api(const std::string& text);
    std::vector<float> call_custom_api(const std::string& text);
};

} // namespace core
} // namespace llama_gui
