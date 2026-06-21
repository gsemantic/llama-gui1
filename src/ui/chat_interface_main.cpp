#include "../include/ui/chat_interface.h"
#include "../include/ui/file_dialog_helper.h"
#include "../include/core/rag_manager.h"
#include "../include/core/rag_settings.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <thread>
#include <queue>
#include <atomic>

namespace llama_gui {
namespace ui {

ChatInterface::ChatInterface(StateManager& state_manager, Settings& settings, LlamaInterface& llama_interface)
    : state_manager_(state_manager)
    , settings_(settings)
    , llama_interface_(llama_interface) {

    memset(input_buffer_, 0, sizeof(input_buffer_));
    input_focused_ = false;
    streaming_active_ = false;
    current_stream_content_ = "";
    selected_text_ = ""; // Initialize selected text

    // Initialize performance metrics
    current_metrics_.response_time_seconds = 0.0;
    current_metrics_.tokens_generated = 0;
    current_metrics_.tokens_per_second = 0.0;
    current_metrics_.is_measuring = false;
    
    // Initialize cache statistics
    cache_stats_.total_requests = 0;
    cache_stats_.prompt_cache_hits = 0;
    cache_stats_.rag_cache_hits = 0;
    cache_stats_.total_tokens_generated = 0;
    cache_stats_.total_tokens_saved = 0;
    cache_stats_.session_start = std::chrono::steady_clock::now();
    
    // Запуск RAG worker потока
    start_rag_worker();
}

ChatInterface::~ChatInterface() {
    stop_rag_worker();
}

void ChatInterface::set_model_load_request_callback(ModelLoadRequestCallback callback) {
    model_load_request_callback_ = callback;
    std::cout << "ChatInterface: Model load request callback set" << std::endl;
}

std::string ChatInterface::generate_conversation_title(const std::string& message_content, size_t max_length) {
    if (message_content.empty()) {
        return "New Chat";
    }

    // Берем первые слова из сообщения
    std::string title = message_content;

    // Удаляем переносы строк и лишние пробелы
    for (char& c : title) {
        if (c == '\n' || c == '\r') {
            c = ' ';
        }
    }

    // Trim начала
    size_t start = title.find_first_not_of(" \t");
    if (start == std::string::npos) {
        return "New Chat";
    }
    title = title.substr(start);

    // Trim конца
    size_t end = title.find_last_not_of(" \t");
    if (end != std::string::npos) {
        title = title.substr(0, end + 1);
    }

    // Ограничиваем длину
    if (title.length() > max_length) {
        title = title.substr(0, max_length - 3) + "...";
    }

    return title;
}

void ChatInterface::add_file_attachment(const std::string& file_path) {
    // Check if file already exists in attachments
    auto it = std::find(attachments_.begin(), attachments_.end(), file_path);
    if (it == attachments_.end()) {
        // === ЧТЕНИЕ СОДЕРЖИМОГО ФАЙЛА (всегда, независимо от RAG) ===
        std::string file_content;
        bool file_read_success = false;
        
        try {
            if (std::filesystem::is_regular_file(file_path)) {
                std::ifstream file(file_path);
                if (file.is_open()) {
                    file_content = std::string((std::istreambuf_iterator<char>(file)),
                                               std::istreambuf_iterator<char>());
                    file.close();
                    file_read_success = true;
                    std::cout << "[ATTACHMENT] File content read: " << file_content.size() 
                              << " bytes from " << file_path << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[ATTACHMENT] Error reading file: " << e.what() << std::endl;
        }
        // ================================================================

        attachments_.push_back(file_path);
        
        // Сохраняем содержимое файла для последующей передачи модели
        if (file_read_success) {
            attachment_contents_[file_path] = file_content;
            std::cout << "[ATTACHMENT] Content stored for: " << file_path << std::endl;
        }
        
        std::cout << "ChatInterface: Added attachment: " << file_path << std::endl;

        // === ОТЛАДКА: Проверка состояния RAG ===
        std::cout << "[ATTACHMENT RAG DEBUG] rag_manager_ = " << (rag_manager_ ? "yes" : "NO") << std::endl;
        std::cout << "[ATTACHMENT RAG DEBUG] rag_enabled_ = " << (rag_enabled_ ? "true" : "FALSE") << std::endl;
        std::cout << "[ATTACHMENT RAG DEBUG] settings_.rag().enable_rag = " << (settings_.rag().enable_rag ? "true" : "FALSE") << std::endl;
        // =========================================

        // === ОБРАБОТКА ФАЙЛА ЧЕРЕЗ RAG (если включен) ===
        // Это позволяет модели "видеть" содержимое прикрепленного файла через RAG-поиск
        if (rag_manager_ && rag_enabled_ && settings_.rag().enable_rag) {
            std::cout << "[ATTACHMENT RAG] Processing attached file through RAG: " << file_path << std::endl;

            try {
                bool success = rag_manager_->process_document(file_path);
                if (success) {
                    std::cout << "[ATTACHMENT RAG] File successfully processed and indexed: " << file_path << std::endl;
                } else {
                    std::cerr << "[ATTACHMENT RAG] Failed to process file: " << file_path << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "[ATTACHMENT RAG] Error processing file: " << e.what() << std::endl;
            }
        } else {
            std::cout << "[ATTACHMENT RAG] RAG is disabled, file content will be sent directly to model" << std::endl;
        }
        // ==================================================
    }
}

void ChatInterface::remove_attachment(size_t index) {
    if (index < attachments_.size()) {
        std::string removed_path = attachments_[index];
        std::cout << "ChatInterface: Removed attachment: " << removed_path << std::endl;
        attachments_.erase(attachments_.begin() + index);
        
        // Также удаляем содержимое файла
        attachment_contents_.erase(removed_path);
        std::cout << "[ATTACHMENT] Content removed for: " << removed_path << std::endl;
    }
}

void ChatInterface::set_attachments(const std::vector<std::string>& attachments) {
    attachments_ = attachments;
    std::cout << "ChatInterface: Set " << attachments_.size() << " attachments" << std::endl;
}

std::string ChatInterface::build_attachments_context() const {
    if (attachment_contents_.empty()) {
        return "";
    }
    
    std::string context;
    context.reserve(5000);  // Преаллокация для производительности
    
    context += "\n\n=== ПРИКРЕПЛЁННЫЕ ФАЙЛЫ ===\n";
    context += "Ниже приведено содержимое прикреплённых файлов. Используй эту информацию для ответа на вопрос.\n";
    
    for (const auto& [file_path, content] : attachment_contents_) {
        std::string filename = file_path;
        size_t last_slash = file_path.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            filename = file_path.substr(last_slash + 1);
        }
        
        context += "\n--- ФАЙЛ: " + filename + " ---\n";
        context += content;
        context += "\n--- КОНЕЦ ФАЙЛА: " + filename + " ---\n";
    }
    
    context += "=== КОНЕЦ ПРИКРЕПЛЁННЫХ ФАЙЛОВ ===\n\n";
    
    return context;
}

// Адаптивный выбор k в зависимости от типа вопроса
static int get_adaptive_k(const std::string& query, int base_k) {
    std::string lower_query = query;
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), 
                   [](unsigned char c) { return std::tolower(c); });
    
    // === 1. ОБЩИЕ ВОПРОСЫ О ДОКУМЕНТЕ (максимальный охват) ===
    std::vector<std::string> broad_patterns = {
        "о чем", "о чём",  // о чем документ
        "расскажи", "расскажите",  // расскажи о документе
        "краткое содержание", "саммари", "резюме",  // summary
        "основная идея", "главная мысль",  // main idea
        "что это", "что такое",  // what is this
        "опиши", "опишите",  // describe
        "what is", "tell me about", "summary", "overview",  // English patterns
        "worum geht es", "zusammenfassung"  // German patterns
    };
    
    for (const auto& pattern : broad_patterns) {
        if (lower_query.find(pattern) != std::string::npos) {
            // Для общих вопросов увеличиваем k в 3-5 раз
            int adaptive_k = std::max(base_k * 3, 10);
            std::cout << "[RAG ADAPTIVE] Broad query detected ('" << pattern << "'), using k=" 
                      << adaptive_k << " instead of " << base_k << std::endl;
            return adaptive_k;
        }
    }
    
    // === 2. ВОПРОСЫ "ПОЧЕМУ/КАК" (средний охват) ===
    std::vector<std::string> why_patterns = {
        "почему", "как", "зачем",  // why/how
        "why", "how", "wieso", "warum"
    };
    
    for (const auto& pattern : why_patterns) {
        if (lower_query.find(pattern) != std::string::npos) {
            int adaptive_k = std::max(base_k * 2, 5);
            std::cout << "[RAG ADAPTIVE] Why/how query detected ('" << pattern << "'), using k=" 
                      << adaptive_k << " instead of " << base_k << std::endl;
            return adaptive_k;
        }
    }
    
    // === 3. ВОПРОСЫ О КОНКРЕТНЫХ ОБЪЕКТАХ/ТЕМАХ (увеличенный k) ===
    // Распознаём вопросы типа "что говорится про X", "информация о X", "расскажи про X"
    std::vector<std::string> topic_patterns = {
        "что говорится", "что сказано", "что пишут",  // what does it say about
        "информация о", "информация про",  // information about
        "расскажи про", "расскажите про",  // tell me about
        "упоминается",  // mentioned
        "what about", "tell me about", "information about",  // English
        "was steht über", "informationen über"  // German
    };
    
    // Проверяем, есть ли в запросе паттерн вопроса о теме/объекте
    bool is_topic_query = false;
    for (const auto& pattern : topic_patterns) {
        if (lower_query.find(pattern) != std::string::npos) {
            is_topic_query = true;
            break;
        }
    }
    
    // Если вопрос о конкретной теме (есть паттерн + вопросительное слово или предлог)
    if (is_topic_query) {
        // Для вопросов о конкретных темах увеличиваем k, чтобы найти все упоминания
        int adaptive_k = std::max(base_k * 2, 8);
        std::cout << "[RAG ADAPTIVE] Topic-specific query detected, using k=" 
                  << adaptive_k << " instead of " << base_k << " (to find all mentions)" << std::endl;
        return adaptive_k;
    }
    
    // === 4. КОНКРЕТНЫЕ ВОПРОСЫ (базовый k) ===
    std::cout << "[RAG ADAPTIVE] Specific query detected, using base k=" << base_k << std::endl;
    return base_k;
}

std::string ChatInterface::process_with_rag(const std::string& user_input) {
    std::cout << "[RAG DEBUG] process_with_rag called (enabled=" << rag_enabled_
              << ", manager=" << (rag_manager_ ? "yes" : "no")
              << ", settings.enable_rag=" << settings_.rag().enable_rag << ")" << std::endl;

    try {
        if (!rag_manager_ || !rag_enabled_ || !settings_.rag().enable_rag) {
            std::cout << "[RAG DEBUG] RAG disabled or not configured, returning original input" << std::endl;
            return user_input;
        }

        // Флаг устанавливается ПЕРЕД вызовом этой функции в send_message()

        // Сначала проверяем кэш чат-истории
        std::cout << "[RAG DEBUG] Checking cached response..." << std::endl;
        std::string cached_response = rag_manager_->find_cached_response(user_input);
        if (!cached_response.empty()) {
            std::cout << "[RAG DEBUG] Found cached response for query: " << user_input << std::endl;

            // Статистика: RAG cache hit (будет учтено в send_message)
            cache_stats_.rag_cache_hits++;

            return cached_response;
        }
        std::cout << "[RAG DEBUG] No cached response found" << std::endl;

        // Затем ищем во внешних документах
        // ИСПОЛЬЗУЕМ АДАПТИВНЫЙ k В ЗАВИСИМОСТИ ОТ ТИПА ВОПРОСА
        std::cout << "[RAG DEBUG] Starting document search..." << std::endl;
        int base_k = settings_.rag().search_k;
        int num_results = get_adaptive_k(user_input, base_k);
    
    // ОГРАНИЧИВАЕМ максимальное количество чанков настройкой max_rag_chunks
    // Если max_rag_chunks = 0 - ограничений нет (для мощных серверов/облака)
    int max_chunks = settings_.rag().max_rag_chunks;
    if (max_chunks > 0 && num_results > max_chunks) {
        std::cout << "[RAG] Limiting search results to " << max_chunks 
                  << " chunks (was " << num_results << ", from max_rag_chunks setting)" << std::endl;
        num_results = max_chunks;
    } else if (max_chunks == 0) {
        std::cout << "[RAG] No limit on chunks (max_rag_chunks=0), using " << num_results << " chunks" << std::endl;
    }

    // === ГИБРИДНЫЙ ПОИСК (векторный + полнотекстовый + query expansion + reranking) ===
    std::vector<RagChunk> search_results;

    // Проверяем настройки RAG
    bool hybrid_enabled = settings_.rag().enable_hybrid_search;

    if (hybrid_enabled) {
        std::cout << "[RAG] Using HYBRID SEARCH (vector + full-text + query expansion + reranking)" << std::endl;
        search_results = rag_manager_->search_hybrid(user_input, num_results);
    } else {
        std::cout << "[RAG] Using standard VECTOR SEARCH" << std::endl;
        search_results = rag_manager_->search_external_documents(user_input, num_results);
    }
    // ================================================================================

    std::cout << "[RAG DEBUG] Found " << search_results.size() << " external results" << std::endl;

    for (size_t i = 0; i < search_results.size(); ++i) {
        std::cout << "[RAG DEBUG]   Result " << (i+1) << ": doc=" << search_results[i].document_id
                  << ", chunk=" << search_results[i].chunk_index
                  << ", content_preview=" << search_results[i].content.substr(0, 50) << "..." << std::endl;
    }

    // Формируем контекст из найденных фрагментов
    if (!search_results.empty()) {
        // === ПРОВЕРКА РЕЖИМА ГЛУБОКОГО АНАЛИЗА ===
        const auto& deep_analysis_settings = settings_.rag().deep_analysis;

        // ОТКЛЮЧАЕМ глубокий анализ для небольших документов
        // Порог: max_rag_chunks чанков (по умолчанию 20)
        // Если max_rag_chunks = 0 (без ограничений), используем порог 50 чанков
        int deep_analysis_threshold = settings_.rag().max_rag_chunks > 0 
                                      ? settings_.rag().max_rag_chunks 
                                      : 50;
        
        bool should_use_deep_analysis = 
            deep_analysis_settings.mode != llama_gui::core::DeepAnalysisMode::Disabled &&
            search_results.size() >= deep_analysis_threshold;
        
        if (!should_use_deep_analysis && search_results.size() >= 10) {
            std::cout << "[RAG] Deep analysis DISABLED for small document (" 
                      << search_results.size() << " chunks < " << deep_analysis_threshold
                      << "), using standard RAG" << std::endl;
        }

        if (should_use_deep_analysis) {
            std::cout << "[RAG] Deep analysis mode enabled: "
                      << (deep_analysis_settings.mode == llama_gui::core::DeepAnalysisMode::MapReduce ? "MapReduce" :
                          deep_analysis_settings.mode == llama_gui::core::DeepAnalysisMode::Iterative ? "Iterative" :
                          "Hierarchical") << std::endl;

            // Запускаем глубокий анализ вместо обычного RAG-промпта
            std::string deep_analysis_result = rag_manager_->process_deep_analysis(
                user_input,
                search_results,
                deep_analysis_settings);

            if (!deep_analysis_result.empty()) {
                std::cout << "[RAG] Deep analysis completed, returning synthesized answer" << std::endl;

                // Кэшируем результат глубокого анализа
                cache_current_interaction(user_input, deep_analysis_result);

                return deep_analysis_result;
            } else {
                std::cerr << "[RAG] Warning: Deep analysis returned empty result, falling back to standard RAG" << std::endl;
            }
        }
        // =========================================
        
        // Стандартный режим RAG (без глубокого анализа)
        std::string rag_prompt = rag_manager_->build_rag_prompt(user_input, search_results);
        std::cout << "[RAG DEBUG] Built RAG prompt with " << search_results.size() << " context chunks" << std::endl;

        // === КЭШИРОВАНИЕ НА ОСНОВЕ ПОЛНОГО RAG-ПРОМПТА ===
        // Проверяем, есть ли уже в кэше ответ на этот полный промпт (вопрос + контекст)
        std::string cached_rag_response;
        if (check_rag_prompt_cache(rag_prompt, cached_rag_response)) {
            std::cout << "[RAG PROMPT CACHE] Found cached response for full RAG prompt!" << std::endl;
            return cached_rag_response;
        }
        // ==================================================

        // === DEBUG: Вывод полного промпта для отладки ===
        std::cout << "===== RAG PROMPT START =====" << std::endl;
        std::cout << rag_prompt << std::endl;
        std::cout << "===== RAG PROMPT END =====" << std::endl;
        // =========================================
        std::cout << "[RAG DEBUG] Returning RAG prompt successfully" << std::endl;
        return rag_prompt;
    }

    // Если ничего не найдено, возвращаем оригинальный запрос
    std::cout << "[RAG DEBUG] No relevant documents found, returning original query" << std::endl;
    return user_input;

    } catch (const std::exception& e) {
        std::cerr << "[RAG ERROR] Exception in process_with_rag: " << e.what() << std::endl;
        std::cerr << "[RAG ERROR] Returning original query as fallback" << std::endl;
        return user_input;  // Fallback: возвращаем оригинальный запрос
    } catch (...) {
        std::cerr << "[RAG ERROR] Unknown exception in process_with_rag" << std::endl;
        std::cerr << "[RAG ERROR] Returning original query as fallback" << std::endl;
        return user_input;  // Fallback: возвращаем оригинальный запрос
    }
}

void ChatInterface::cache_current_interaction(const std::string& query, const std::string& response) {
    if (rag_manager_ && rag_enabled_ && settings_.rag().enable_caching) {
        rag_manager_->cache_query_response(query, response);
    }
}

// ============================================================================
// Prompt Caching - кэширование идентичных запросов
// ============================================================================

size_t ChatInterface::compute_prompt_hash(const std::string& prompt) {
    // Простой хэш FNV-1a для скорости
    const uint64_t FNV_PRIME = 1099511628211ULL;
    const uint64_t FNV_OFFSET = 14695981039346656037ULL;
    
    uint64_t hash = FNV_OFFSET;
    for (char c : prompt) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(c));
        hash *= FNV_PRIME;
    }
    return static_cast<size_t>(hash);
}

bool ChatInterface::check_prompt_cache(const std::string& prompt, std::string& cached_response) {
    std::shared_lock<std::shared_mutex> lock(prompt_cache_mutex_);
    
    auto hash = compute_prompt_hash(prompt);
    auto it = prompt_cache_.find(hash);
    
    if (it != prompt_cache_.end()) {
        cached_response = it->second.response;
        cache_stats_.prompt_cache_hits++;
        it->second.hit_count++;
        
        std::cout << "[PROMPT CACHE] HIT! Hash: " << hash 
                  << ", Total hits: " << it->second.hit_count
                  << ", Tokens saved: " << it->second.tokens_saved << std::endl;
        
        return true;
    }
    
    return false;
}

void ChatInterface::update_prompt_cache(const std::string& prompt, const std::string& response, int tokens) {
    std::unique_lock<std::shared_mutex> lock(prompt_cache_mutex_);

    auto hash = compute_prompt_hash(prompt);

    PromptCacheEntry entry;
    entry.response = response;
    entry.tokens_saved = tokens;
    entry.created_at = std::chrono::steady_clock::now();
    entry.hit_count = 0;

    prompt_cache_[hash] = entry;

    std::cout << "[PROMPT CACHE] Stored! Hash: " << hash
              << ", Tokens: " << tokens
              << ", Cache size: " << prompt_cache_.size() << std::endl;
}

// ============================================================================
// RAG Prompt Caching - кэширование на основе полного промпта (вопрос + контекст)
// ============================================================================

bool ChatInterface::check_rag_prompt_cache(const std::string& rag_prompt, std::string& cached_response) {
    std::shared_lock<std::shared_mutex> lock(prompt_cache_mutex_);

    auto hash = compute_prompt_hash(rag_prompt);
    auto it = prompt_cache_.find(hash);

    if (it != prompt_cache_.end()) {
        cached_response = it->second.response;
        cache_stats_.prompt_cache_hits++;
        it->second.hit_count++;

        std::cout << "[RAG PROMPT CACHE] HIT! Hash: " << hash
                  << ", Total hits: " << it->second.hit_count
                  << ", Tokens saved: " << it->second.tokens_saved << std::endl;

        return true;
    }

    return false;
}

void ChatInterface::update_rag_prompt_cache(const std::string& rag_prompt, const std::string& response, int tokens) {
    std::unique_lock<std::shared_mutex> lock(prompt_cache_mutex_);

    auto hash = compute_prompt_hash(rag_prompt);

    PromptCacheEntry entry;
    entry.response = response;
    entry.tokens_saved = tokens;
    entry.created_at = std::chrono::steady_clock::now();
    entry.hit_count = 0;

    prompt_cache_[hash] = entry;

    std::cout << "[RAG PROMPT CACHE] Stored! Hash: " << hash
              << ", Tokens: " << tokens
              << ", Cache size: " << prompt_cache_.size() << std::endl;
}

void ChatInterface::reset_cache_stats() {
    std::unique_lock<std::shared_mutex> lock(prompt_cache_mutex_);
    
    cache_stats_.total_requests = 0;
    cache_stats_.prompt_cache_hits = 0;
    cache_stats_.rag_cache_hits = 0;
    cache_stats_.total_tokens_generated = 0;
    cache_stats_.total_tokens_saved = 0;
    cache_stats_.session_start = std::chrono::steady_clock::now();
    
    std::cout << "[CACHE STATS] Reset all statistics" << std::endl;
}

// ============================================================================
// АСИНХРОННАЯ RAG-ОБРАБОТКА
// ============================================================================

void ChatInterface::start_rag_worker() {
    rag_worker_running_ = true;
    rag_stop_requested_ = false;
    
    rag_worker_thread_ = std::thread([this]() {
        std::cout << "[RAG WORKER] Thread started" << std::endl;
        
        while (rag_worker_running_ && !rag_stop_requested_) {
            RagRequest request;
            bool has_request = false;
            
            // Извлекаем запрос из очереди
            {
                std::unique_lock<std::mutex> lock(rag_queue_mutex_);
                if (!rag_request_queue_.empty()) {
                    request = rag_request_queue_.front();
                    rag_request_queue_.pop();
                    has_request = true;
                }
            }
            
            if (has_request && rag_manager_ && rag_enabled_) {
                rag_processing_active_ = true;
                std::cout << "[RAG WORKER] Processing query: " << request.query.substr(0, 50) << "..." << std::endl;
                
                // Выполняем RAG-обработку
                std::string result = process_with_rag(request.query);
                
                // Сохраняем результат для главного потока
                {
                    std::unique_lock<std::mutex> result_lock(rag_result_mutex_);
                    rag_pending_result_ = result;
                    rag_pending_query_ = request.query;
                    rag_result_ready_ = true;
                }
                
                rag_processing_active_ = false;
                std::cout << "[RAG WORKER] Completed, result size: " << result.size() << " bytes" << std::endl;
                
                // Вызываем callback если есть
                if (request.callback) {
                    request.callback(result);
                }
            } else if (!has_request) {
                // Нет запросов - спим 100ms
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        std::cout << "[RAG WORKER] Thread stopped" << std::endl;
    });
}

void ChatInterface::stop_rag_worker() {
    rag_stop_requested_ = true;
    rag_worker_running_ = false;
    
    if (rag_worker_thread_.joinable()) {
        rag_worker_thread_.join();
    }
    
    std::cout << "[RAG WORKER] Stopped and joined" << std::endl;
}

void ChatInterface::submit_rag_request(const std::string& query) {
    RagRequest request;
    request.query = query;
    request.submitted_at = std::chrono::steady_clock::now();
    
    {
        std::unique_lock<std::mutex> lock(rag_queue_mutex_);
        rag_request_queue_.push(request);
    }
    
    std::cout << "[RAG] Submitted async request: " << query.substr(0, 50) << "..." << std::endl;
}

void ChatInterface::process_rag_queue() {
    // Проверяем, готов ли результат от RAG worker
    if (rag_result_ready_.load()) {
        std::string result;
        std::string query;
        
        {
            std::unique_lock<std::mutex> lock(rag_result_mutex_);
            result = rag_pending_result_;
            query = rag_pending_query_;
            rag_result_ready_ = false;
        }
        
        // Результат готов - можно отправлять сообщение
        // Это будет обработано в main_window или send_message
        std::cout << "[RAG] Async result ready: " << result.size() << " bytes" << std::endl;
        
        // Здесь можно вызвать callback или установить флаг
        // Фактическая отправка сообщения будет в main_window
    }
}

} // namespace ui
} // namespace llama_gui