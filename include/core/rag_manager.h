#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <ctime>
#include "llama_interface.h"
#include "rag_settings.h"
#include "kv_cache_types.h"
#include "rag_index_profile.h"
#include "openrouter_client.h"

// Предварительное объявление класса EmbeddingGenerator
namespace llama_gui {
namespace core {
    class EmbeddingGenerator;
}
}

#ifdef USE_FAISS
namespace faiss {
    class Index;
}
#endif

namespace llama_gui {
namespace core {

struct RagChunk {
    std::string content;
    std::string document_id;
    int chunk_index;
    std::vector<float> embedding;
};

// Этапы глубокого анализа для выбора модели
enum class RagAnalysisStage {
    Map,    // Суммаризация чанков
    Reduce  // Синтез финального ответа
};

// Информация о бесплатной модели OpenRouter
struct FreeModelInfo {
    std::string id;              // ID модели (например, "google/gemma-2-2b-it:free")
    std::string name;            // Человекочитаемое название
    int context_length;          // Максимальный размер контекста
    int speed_rating;            // 1-5 (5 = самая быстрая)
    int quality_rating;          // 1-5 (5 = лучшее качество)
    std::string best_for;        // "map", "reduce", или "both"
};

class RagManager {
public:
    RagManager(const std::string& embedding_model_path = "models/embedding_model.gguf");
    ~RagManager();

    // Инициализация индексов
    bool initialize_indexes();

    // Обработка внешних документов
    bool process_document(const std::string& file_path);
    bool process_text_chunk(const std::string& text, const std::string& doc_id, int chunk_index);

    // Кэширование чат-истории
    std::string find_cached_response(const std::string& query);
    void cache_query_response(const std::string& query, const std::string& response);

    // Поиск по внешним документам
    std::vector<RagChunk> search_external_documents(const std::string& query, int k = 5);
    
    // === ГИБРИДНЫЙ ПОИСК (векторный + полнотекстовый) ===
    std::vector<RagChunk> search_hybrid(const std::string& query, int k = 5);
    std::vector<RagChunk> rerank_results(const std::string& query, 
                                         const std::vector<RagChunk>& results,
                                         const std::vector<std::string>& keywords);
    std::vector<std::string> expand_query(const std::string& query);
    float keyword_boost_score(const RagChunk& chunk, const std::vector<std::string>& keywords);

    // Поиск по чат-истории
    std::vector<RagChunk> search_chat_history(const std::string& query, int k = 3);

    // Генерация эмбеддинга
    std::vector<float> generate_embedding(const std::string& text);

    // Формирование промпта с RAG-контекстом
    std::string build_rag_prompt(const std::string& user_query,
                                const std::vector<RagChunk>& context_chunks);

    // === ГЛУБОКИЙ АНАЛИЗ ДОКУМЕНТА (Map-Reduce) ===
    
    /**
     * @brief Основной метод глубокого анализа с выбором режима
     * @param query Исходный запрос пользователя
     * @param all_chunks Все найденные чанки документов
     * @param settings Настройки глубокого анализа
     * @return Финальный синтезированный ответ
     */
    std::string process_deep_analysis(const std::string& query,
                                      std::vector<RagChunk>& all_chunks,
                                      const DeepAnalysisSettings& settings);

    /**
     * @brief Map-Reduce режим: разбиение на батчи → резюме → синтез
     * @param query Исходный запрос пользователя
     * @param all_chunks Все найденные чанки документов
     * @param settings Настройки глубокого анализа
     * @return Финальный синтезированный ответ
     */
    std::string process_deep_analysis_mapreduce(const std::string& query,
                                                 std::vector<RagChunk>& all_chunks,
                                                 const DeepAnalysisSettings& settings);

    /**
     * @brief Итеративный режим: последовательный обход всех чанков
     * @param query Исходный запрос пользователя
     * @param all_chunks Все найденные чанки документов
     * @param settings Настройки глубокого анализа
     * @return Финальный синтезированный ответ
     */
    std::string process_deep_analysis_iterative(const std::string& query,
                                                 std::vector<RagChunk>& all_chunks,
                                                 const DeepAnalysisSettings& settings);

    /**
     * @brief Иерархический режим: дерево резюме (для очень больших документов)
     * @param query Исходный запрос пользователя
     * @param all_chunks Все найденные чанки документов
     * @param settings Настройки глубокого анализа
     * @return Финальный синтезированный ответ
     */
    std::string process_deep_analysis_hierarchical(const std::string& query,
                                                    std::vector<RagChunk>& all_chunks,
                                                    const DeepAnalysisSettings& settings);

    /**
     * @brief Генерация резюме для одного чанка (Map-этап)
     * @param chunk Чанк для суммаризации
     * @param query Исходный запрос (для фокусировки резюме)
     * @return Текст резюме
     */
    std::string generate_chunk_summary(const RagChunk& chunk, const std::string& query);
    
    /**
     * @brief Генерация резюме через OpenRouter (Map-этап)
     * @param chunk Чанк для суммаризации
     * @param query Исходный запрос (для фокусировки резюме)
     * @return Текст резюме
     */
    std::string generate_chunk_summary_openrouter(const RagChunk& chunk, const std::string& query);

    /**
     * @brief Синтез финального ответа из промежуточных резюме (Reduce-этап)
     * @param query Исходный запрос пользователя
     * @param summaries Список промежуточных резюме
     * @param target_context_size Целевой размер контекста
     * @return Финальный синтезированный ответ
     */
    std::string synthesize_final_answer(const std::string& query,
                                        const std::vector<std::string>& summaries,
                                        int target_context_size);
    
    /**
     * @brief Синтез финального ответа через OpenRouter (Reduce-этап)
     * @param query Исходный запрос пользователя
     * @param summaries Список промежуточных резюме
     * @param target_context_size Целевой размер контекста
     * @return Финальный синтезированный ответ
     */
    std::string synthesize_final_answer_openrouter(const std::string& query,
                                                    const std::vector<std::string>& summaries,
                                                    int target_context_size);

    /**
     * @brief Автоматическое увеличение размера контекста сервера
     * @param target_context_size Целевой размер контекста
     * @return true если успешно
     */
    bool auto_adjust_server_context_size(int target_context_size);

    // Управление параметрами
    void set_max_chunks(int max_chunks);
    void set_similarity_threshold(float threshold);

    // Обновление из настроек
    void update_from_settings(const core::RagSettings& settings);
    
    // === ПОДДЕРЖКА OPENROUTER ДЛЯ ГЛУБОКОГО АНАЛИЗА ===
    
    /**
     * @brief Установить OpenRouter клиент для глубокого анализа
     * @param client Клиент OpenRouter
     */
    void set_openrouter_client(std::shared_ptr<OpenRouterClient> client);
    
    /**
     * @brief Установить модель OpenRouter для глубокого анализа
     * @param model_id ID модели (например, "openrouter/free")
     */
    void set_openrouter_model(const std::string& model_id);
    
    /**
     * @brief Выбрать модель для этапа глубокого анализа
     * @param stage Этап (Map или Reduce)
     * @param settings Настройки глубокого анализа
     * @return ID выбранной модели
     */
    std::string select_model_for_stage(RagAnalysisStage stage, const DeepAnalysisSettings& settings);

    /**
     * @brief Получить бесплатную модель для профиля
     * @param stage Этап (Map или Reduce)
     * @param profile Профиль производительности
     * @return ID бесплатной модели
     */
    std::string get_free_model_for_profile(RagAnalysisStage stage, const std::string& profile);
    
    // === ПРОВЕРКИ И FALLBACK ДЛЯ OPENROUTER ===
    
    /**
     * @brief Проверка доступности OpenRouter API (НЕ тратит лимиты!)
     * @return true если API доступен
     */
    bool check_openrouter_availability();

    /**
     * @brief Проверка лимитов OpenRouter перед MapReduce (НЕ тратит лимиты!)
     * @param required_requests Требуемое количество запросов
     * @return true если лимиты позволяют
     */
    bool check_openrouter_rate_limit(int required_requests);

    /**
     * @brief Генерация резюме с fallback на локальный сервер
     * @param chunk Чанк для суммаризации
     * @param query Исходный запрос
     * @return Текст резюме
     */
    std::string generate_chunk_summary_with_fallback(const RagChunk& chunk, const std::string& query);

    /**
     * @brief Генерация резюме через локальный сервер
     * @param chunk Чанк для суммаризации
     * @param query Исходный запрос
     * @return Текст резюме
     */
    std::string generate_chunk_summary_local(const RagChunk& chunk, const std::string& query);

    // === Персистентность (сохранение/загрузка индекса) ===
    bool save_index(const std::string& index_path = "");
    bool load_index(const std::string& index_path = "");
    bool has_persistent_index() const;
    std::string get_default_index_path() const;
    void clear_all_indexes();

    // === Управление профилями индексов ===
    bool initialize_profile_manager(const std::string& profiles_directory = "");
    bool create_index_profile(const std::string& profile_name, const std::string& source_directory = "");
    bool switch_index_profile(const std::string& profile_name);
    bool delete_index_profile(const std::string& profile_name, bool delete_index_file = false);
    std::vector<std::string> get_index_profile_names() const;
    std::string get_current_index_profile() const;
    std::string get_current_index_path() const;  // Путь к файлу индекса текущего профиля
    bool load_index_for_current_profile();

    // === KV-cache persistence для документов ===

    /**
     * @brief Сохранить KV-cache для обработанного документа
     * @param doc_id Уникальный идентификатор документа
     * @param slot_id ID слота, в котором загружен документ
     * @return true если успешно
     */
    bool save_document_kv_cache(const std::string& doc_id, int slot_id);

    /**
     * @brief Загрузить KV-cache для документа в слот
     * @param doc_id Уникальный идентификатор документа
     * @param slot_id ID слота для загрузки
     * @return true если успешно
     */
    bool load_document_kv_cache(const std::string& doc_id, int slot_id);

    /**
     * @brief Проверить наличие сохранённого KV-cache для документа
     * @param doc_id Уникальный идентификатор документа
     * @return true если файл существует
     */
    bool has_document_kv_cache(const std::string& doc_id) const;

    /**
     * @brief Удалить сохранённый KV-cache документа
     * @param doc_id Уникальный идентификатор документа
     * @return true если успешно
     */
    bool delete_document_kv_cache(const std::string& doc_id);

    /**
     * @brief Получить путь к файлу KV-cache для документа
     * @param doc_id Уникальный идентификатор документа
     * @return Полный путь к файлу
     */
    std::string get_document_kv_cache_path(const std::string& doc_id) const;

    /**
     * @brief Очистить все сохранённые KV-cache
     * @param older_than_seconds Удалить файлы старше указанного времени (0 = все)
     * @return Количество удалённых файлов
     */
    int cleanup_old_kv_caches(int older_than_seconds = 0);

    /**
     * @brief Структура информации о документе в RAG
     */
    struct RagDocumentInfo {
        std::string doc_id;
        std::string file_path;
        std::string file_hash;
        size_t n_tokens;
        time_t last_processed;
        bool kv_cache_available;
        std::string kv_cache_path;
    };

    /**
     * @brief Получить информацию о документе
     * @param doc_id Уникальный идентификатор документа
     * @return Информация о документе
     */
    RagDocumentInfo get_document_info(const std::string& doc_id) const;

    /**
     * @brief Попытаться загрузить KV-cache для документов из найденных чанков
     * @param chunks Найденные RAG-чанки
     * @param slot_id ID слота для загрузки KV-cache
     * @return true если хотя бы один KV-cache загружен успешно
     */
    bool try_load_document_kv_cache(const std::vector<RagChunk>& chunks, int slot_id);

    // === Информация о состоянии ===
    size_t get_external_chunks_count() const { return external_chunks_.size(); }
    size_t get_chat_history_chunks_count() const { return chat_history_chunks_.size(); }
    
    // Проверка наличия документа в индексе
    bool is_document_indexed(const std::string& file_path) const;
    std::vector<std::string> get_indexed_documents() const;

private:
#ifdef USE_FAISS
    std::unique_ptr<faiss::Index> external_docs_index_;  // Для внешних документов
    std::unique_ptr<faiss::Index> chat_history_index_;   // Для кэширования чатов
#else
    void* external_docs_index_;  // Для внешних документов
    void* chat_history_index_;   // Для кэширования чатов
#endif
    std::vector<RagChunk> external_chunks_;
    std::vector<RagChunk> chat_history_chunks_;

    // GGUF модель для эмбеддингов
    std::unique_ptr<EmbeddingGenerator> embedding_generator_;

    // Параметры
    int max_chunks_in_memory_ = 1000;  // Ограничение для экономии памяти
    float similarity_threshold_ = 0.7f; // Порог схожести
    int max_tokens_per_chunk_ = 512;   // Размер чанка в токенах (увеличено с 256)
    static constexpr int EMBEDDING_DIMENSION = 1024; // Размер эмбеддинга BGE-M3 (обновляется динамически)
    
    // MMR параметры
    bool enable_mmr_ = false;  // Включить MMR
    float mmr_lambda_ = 0.5f;  // MMR lambda (0=релевантность, 1=разнообразие)

    // KV-cache параметры
    bool enable_kv_cache_ = true;  // Включить KV-cache для документов
    
    // Параметры гибридного поиска
    bool enable_hybrid_search_ = true;  // Включить гибридный поиск
    float keyword_boost_weight_ = 2.0f;  // Вес ключевого слова при reranking
    bool enable_query_expansion_ = true;  // Включить расширение запроса

    // KV-cache состояние: маппинг document_id → slot_id
    std::unordered_map<std::string, int> doc_id_to_slot_map_;  // document_id → slot_id

    // Кэш эмбеддингов запросов - OPTIMIZATION: unordered_map для O(1) поиска
    struct QueryEmbeddingCache {
        std::vector<float> embedding;
        uint64_t last_access;
    };
    std::unordered_map<std::string, QueryEmbeddingCache> query_embedding_cache_; // OPTIMIZATION: hash map вместо vector
    int max_embedding_cache_size_ = 100; // Размер кэша эмбеддингов (можно изменять)

    // Методы для работы с кэшем
    std::vector<float> get_cached_embedding(const std::string& text);
    void cache_embedding(const std::string& text, const std::vector<float>& embedding);
    void cleanup_embedding_cache();

    // Методы для оптимизации
    void cleanup_old_chunks();
#ifdef USE_FAISS
    std::unique_ptr<faiss::Index> create_optimized_index(int dim);
#else
    void* create_optimized_index(int dim);
#endif
    void normalize_vector(std::vector<float>& vec);
    std::vector<std::string> split_into_chunks(const std::string& text, int max_tokens);
    std::vector<std::string> split_into_sentences(const std::string& text);  // Новый метод
    int count_tokens_approx(const std::string& text);
    std::string get_file_extension(const std::string& file_path);
    std::string read_txt_file(const std::string& file_path);
    void rebuild_index();

    // === Методы KV-cache ===
    std::string generate_doc_id(const std::string& file_path) const;
    std::string compute_file_hash(const std::string& file_path) const;

    // === Члены класса для KV-cache ===
    std::string kv_cache_directory_;  // Директория для хранения KV-cache
    std::shared_ptr<LlamaInterface> llama_interface_;  // Интерфейс к серверу

    // === Менеджер профилей индексов ===
    RagIndexProfileManager profile_manager_;  // Менеджер профилей
    
    // === OpenRouter для глубокого анализа ===
    std::shared_ptr<OpenRouterClient> openrouter_client_;  // Клиент OpenRouter
    std::string openrouter_model_id_;  // Текущая модель OpenRouter
    bool use_openrouter_for_rag_ = false;  // Флаг использования OpenRouter
};

} // namespace core
} // namespace llama_gui