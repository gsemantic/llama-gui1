#pragma once

#include <string>
#include "embedding_generator.h"  // Для EmbeddingMode и CloudEmbeddingProvider

namespace llama_gui {
namespace core {

// === РЕЖИМЫ РАБОТЫ RAG ===
enum class RagMode {
    DocumentsOnly,    // Только поиск по загруженным документам (рекомендуется)
    CacheOnly,        // Только кэш чат-истории
    Both              // Оба режима (документы + кэш)
};

// Режимы глубокого анализа документа
enum class DeepAnalysisMode {
    Disabled,         // Отключен (обычный RAG поиск)
    MapReduce,        // Map-Reduce: разбиение на группы → резюме → синтез
    Iterative,        // Итеративный: последовательный обход всех чанков
    Hierarchical      // Иерархический: дерево резюме (для очень больших документов)
};

// Настройки глубокого анализа
struct DeepAnalysisSettings {
    DeepAnalysisMode mode = DeepAnalysisMode::Disabled;  // Режим глубокого анализа
    int chunks_per_batch = 3;  // Количество чанков в одном батче (для Map-Reduce)
    int max_iterations = 20;    // Максимальное количество итераций
    bool enable_progressive_summary = true;  // Включить прогрессивное суммирование
    int final_synthesis_chunks = 10;  // Количество резюме для финального синтеза
    bool auto_adjust_context_size = false;  // Автоматически увеличивать контекст сервера
    int target_context_size = 4096;  // Целевой размер контекста (если auto_adjust=true)
    
    // === ВЫБОР МОДЕЛЕЙ ДЛЯ ГЛУБОКОГО АНАЛИЗА ===
    
    // Автоматический выбор моделей (по умолчанию включён)
    bool auto_select_models = true;
    
    // Профиль производительности: fast, balanced, quality, economy
    // По умолчанию balanced - оптимальный баланс скорости и качества
    std::string performance_profile = "balanced";
    
    // Ручная настройка моделей (если пустые - используется автовыбор)
    std::string map_model = "";      // Модель для Map-этапа (суммаризация чанков)
    std::string reduce_model = "";   // Модель для Reduce-этапа (синтез ответа)
    
    // Использовать только бесплатные модели (ПО УМОЛЧАНИЮ true)
    bool free_only = true;
    
    // Максимальная стоимость за 1M токенов (если free_only = false)
    double max_cost_per_million = 0.00;  // $0.00 = только бесплатные
};

struct RagSettings {
    std::string embedding_model_path = "/home/Alex/projects/gguf/bge-m3-Q5_K_M.gguf";  // BGE-M3 по умолчанию
    
    // === ОПТИМИЗИРОВАННЫЕ НАСТРОЙКИ ДЛЯ ЛУЧШЕГО ПОИСКА ===
    int max_chunks_in_memory = 500;         // ↑ Увеличено с 100 для лучшего покрытия
    float similarity_threshold = 0.50f;     // ↓ Снижено с 0.70 для поиска релевантных документов
    int max_embedding_cache_size = 500;     // ↑ Увеличено для экономии запросов к API
    int embedding_dimension = 1024;         // BGE-M3 (динамически обновляется при загрузке модели)
    int max_sequence_length = 1024;         // ↑ Увеличено для лучшего контекста
    int max_tokens_per_chunk = 512;         // ↑ Увеличено для более полных чанков
    
    int search_k = 10;                      // ↑ Увеличено с 5 для поиска большего числа кандидатов
    float mmr_lambda = 0.3f;                // Оптимизировано для баланса релевантность/разнообразие
    bool enable_mmr = true;                 // ✅ Включить MMR для разнообразия результатов
    bool enable_rag = true;
    bool enable_caching = true;
    bool enable_kv_cache = true;            // Включить KV-cache для документов

    // Параметры гибридного поиска
    bool enable_hybrid_search = true;       // ✅ Включить гибридный поиск
    float keyword_boost_weight = 3.0f;      // ↑ Усилено с 2.0 для лучшего boost ключевых слов
    bool enable_query_expansion = true;     // ✅ Включить расширение запроса (транслитерация)

    // Ограничение количества чанков для предотвращения зависаний GUI
    int max_rag_chunks = 50;                // ↑ Увеличено с 20 для более полного контекста

    // === НАСТРОЙКИ ЭМБЕДДИНГА ===
    
    // Режим генерации эмбеддингов
    EmbeddingMode embedding_mode = EmbeddingMode::Hybrid;  // Гибридный: облако + локальный fallback
    
    // Облачный провайдер для эмбеддингов
    CloudEmbeddingProvider cloud_embedding_provider = CloudEmbeddingProvider::HuggingFace;
    std::string cloud_embedding_api_key = "";  // API ключ для облачного провайдера
    std::string cloud_embedding_model = "all-MiniLM-L6-v2";  // Модель для совместимости
    
    // Приоритет облака в гибридном режиме
    bool cloud_embedding_priority = true;
    
    // Кэширование эмбеддингов
    bool enable_embedding_cache = true;
    int embedding_cache_size = 500;

    // Настройки глубокого анализа документа
    DeepAnalysisSettings deep_analysis;

    RagMode rag_mode = RagMode::DocumentsOnly;  // Режим работы RAG
};

} // namespace core
} // namespace llama_gui