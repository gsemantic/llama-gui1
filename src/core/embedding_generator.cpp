#include "../include/core/embedding_generator.h"
#include "../include/core/logger.h"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <iostream>
#include <functional>
#include <numeric>
#include <mutex>

// llama.cpp
#include "llama.h"
#include "llama-cpp.h"
#include "ggml-backend.h"

// CURL для HTTP запросов
#include <curl/curl.h>

// JSON для парсинга ответов
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace llama_gui {
namespace core {

// ============================================================================
// Утилиты
// ============================================================================

namespace {

// Callback для CURL
size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// Хеш текста для кэша
std::string compute_hash(const std::string& text) {
    std::hash<std::string> hasher;
    return std::to_string(hasher(text));
}

// Нормализация вектора
void normalize_vector(std::vector<float>& vec) {
    float norm = 0.0f;
    for (float val : vec) {
        norm += val * val;
    }
    if (norm > 0.0f) {
        norm = std::sqrt(norm);
        for (float& val : vec) {
            val /= norm;
        }
    }
}

} // anonymous namespace

// ============================================================================
// Конструктор/деструктор
// ============================================================================

EmbeddingGenerator::EmbeddingGenerator(const std::string& model_path)
    : local_model_path_(model_path) {
    // Инициализация CURL (если нужно)
    curl_global_init(CURL_GLOBAL_DEFAULT);

    std::cerr << "[EmbeddingGenerator] CONSTRUCTOR CALLED! mode="
              << (embedding_mode_ == EmbeddingMode::Hybrid ? "Hybrid" :
                  embedding_mode_ == EmbeddingMode::LocalOnly ? "Local" :
                  embedding_mode_ == EmbeddingMode::CloudOnly ? "Cloud" : "Auto")
              << " path='" << model_path << "'" << std::endl;
    std::cerr << "[EmbeddingGenerator] local_model_path_ = '" << local_model_path_ << "'" << std::endl;
}

EmbeddingGenerator::~EmbeddingGenerator() {
    // Умные указатели освободят ресурсы автоматически
    curl_global_cleanup();
}

// ============================================================================
// Загрузка локальной модели
// ============================================================================

bool EmbeddingGenerator::load_local_model(const std::string& model_path) {
    if (model_path.empty()) {
        std::cerr << "[EmbeddingGenerator] Error: Empty model path" << std::endl;
        return false;
    }

    // Если модель уже загружена, выгрузим сначала
    if (local_model_loaded_) {
        std::cerr << "[EmbeddingGenerator] Unloading previous model..." << std::endl;
        unload_model();
    }

    std::cerr << "[EmbeddingGenerator] Loading local model: " << model_path << std::endl;

    // Инициализация llama.cpp
    llama_backend_init();

    // Загрузка GGML бэкендов ВРУЧНУЮ из пути llama-b7472
    std::cerr << "[EmbeddingGenerator] Loading GGML backends from llama-b7472..." << std::endl;
    ggml_backend_load("/home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-b7472/libggml-base.so");
    ggml_backend_load("/home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-b7472/libggml-cpu-x64.so");
    std::cerr << "[EmbeddingGenerator] GGML backends loaded" << std::endl;

    // Параметры модели
    llama_model_params model_params = llama_model_default_params();

    // Загрузка модели
    std::cerr << "[EmbeddingGenerator] Loading GGUF model..." << std::endl;
    llama_model* model_raw = llama_model_load_from_file(model_path.c_str(), model_params);
    
    if (!model_raw) {
        std::cerr << "[EmbeddingGenerator] Error: Failed to load model from " << model_path << std::endl;
        return false;
    }

    model_.reset(model_raw);  // Передаём в умный указатель

    // Проверка наличия энкодера (для embedding моделей)
    has_encoder_ = llama_model_has_encoder(model_.get());
    
    // Принудительная проверка для BGE-M3 (иногда llama_model_has_encoder возвращает false)
    std::string model_path_lower = model_path;
    std::transform(model_path_lower.begin(), model_path_lower.end(), model_path_lower.begin(), ::tolower);
    bool is_bge_model = model_path_lower.find("bge") != std::string::npos;
    
    if (is_bge_model && !has_encoder_) {
        std::cerr << "[EmbeddingGenerator] Forcing has_encoder=true for BGE model" << std::endl;
        has_encoder_ = true;
    }

    std::cerr << "[EmbeddingGenerator] Model has encoder: " << (has_encoder_ ? "YES" : "NO") << std::endl;

    // Получение pooling type (из контекста, не из модели)
    // Пока ставим UNSPECIFIED, определим после создания контекста
    pooling_type_ = LLAMA_POOLING_TYPE_UNSPECIFIED;

    // Размер эмбеддинга
    int n_embd = llama_model_n_embd(model_.get());
    embedding_dimension_ = n_embd;
    std::cout << "[EmbeddingGenerator] Embedding dimension: " << embedding_dimension_ << std::endl;

    // Максимальная длина последовательности
    max_sequence_length_ = llama_model_n_ctx_train(model_.get());
    std::cout << "[EmbeddingGenerator] Max sequence length: " << max_sequence_length_ << std::endl;

    // Параметры контекста
    llama_context_params ctx_params = llama_context_default_params();
    ctx_params.n_ctx = std::min(max_sequence_length_, 8192);  // Ограничиваем контекст
    ctx_params.n_batch = std::min(max_sequence_length_, 8192);
    ctx_params.no_perf = true;  // Отключаем performance counters
    // embeddings = true для получения эмбеддингов через llama_encode/llama_get_embeddings_seq
    ctx_params.embeddings = true;

    // Создание контекста
    std::cout << "[EmbeddingGenerator] Creating context..." << std::endl;
    llama_context* ctx_raw = llama_init_from_model(model_.get(), ctx_params);
    
    if (!ctx_raw) {
        std::cerr << "[EmbeddingGenerator] Error: Failed to create context" << std::endl;
        model_.reset();
        return false;
    }

    context_.reset(ctx_raw);  // Передаём в умный указатель

    // Теперь можно получить pooling type из контекста
    pooling_type_ = llama_pooling_type(context_.get());
    std::cout << "[EmbeddingGenerator] Pooling type: " << 
        (pooling_type_ == LLAMA_POOLING_TYPE_NONE ? "NONE" :
         pooling_type_ == LLAMA_POOLING_TYPE_MEAN ? "MEAN" :
         pooling_type_ == LLAMA_POOLING_TYPE_CLS ? "CLS" :
         pooling_type_ == LLAMA_POOLING_TYPE_LAST ? "LAST" :
         pooling_type_ == LLAMA_POOLING_TYPE_RANK ? "RANK" : "UNSPECIFIED") << std::endl;

    // Успех
    local_model_path_ = model_path;
    local_model_loaded_ = true;
    
    // Извлекаем имя модели из пути
    size_t pos = model_path.find_last_of('/');
    if (pos != std::string::npos) {
        current_model_name_ = model_path.substr(pos + 1);
    } else {
        current_model_name_ = model_path;
    }

    std::cout << "[EmbeddingGenerator] Local model loaded successfully: " << current_model_name_ << std::endl;
    std::cout << "[EmbeddingGenerator] Dimension: " << embedding_dimension_ << std::endl;

    return true;
}

bool EmbeddingGenerator::load_model() {
    return load_local_model(local_model_path_);
}

void EmbeddingGenerator::unload_model() {
    if (!local_model_loaded_) {
        return;
    }

    std::cout << "[EmbeddingGenerator] Unloading model..." << std::endl;

    // Освобождаем контекст и модель
    context_.reset();
    model_.reset();

    local_model_loaded_ = false;
    has_encoder_ = false;
    pooling_type_ = LLAMA_POOLING_TYPE_NONE;
    embedding_dimension_ = 1024;  // BGE-M3 fallback (динамически обновляется при загрузке модели)
    current_model_name_ = "";

    std::cout << "[EmbeddingGenerator] Model unloaded successfully" << std::endl;
}

// ============================================================================
// Настройка облачного провайдера
// ============================================================================

bool EmbeddingGenerator::configure_cloud_provider(const CloudEmbeddingConfig& config) {
    cloud_config_ = config;
    cloud_configured_ = true;
    
    std::cout << "[EmbeddingGenerator] Cloud provider configured: ";
    
    switch (config.provider) {
        case CloudEmbeddingProvider::HuggingFace:
            std::cout << "HuggingFace";
            break;
        case CloudEmbeddingProvider::OpenRouter:
            std::cout << "OpenRouter";
            break;
        case CloudEmbeddingProvider::Custom:
            std::cout << "Custom (" << config.endpoint_url << ")";
            break;
        default:
            std::cout << "None";
    }
    
    std::cout << ", Model: " << config.model_name << std::endl;
    
    // Проверяем доступность
    cloud_available_ = check_cloud_availability();
    
    return true;
}

bool EmbeddingGenerator::check_cloud_availability() {
    if (!cloud_configured_) {
        return false;
    }
    
    // Простая проверка: пинг эндпоинта
    // TODO: Реальная проверка API
    
    cloud_available_ = true;  // Пока эмулируем
    return cloud_available_;
}

// ============================================================================
// Генерация эмбеддингов (основной метод)
// ============================================================================

std::vector<float> EmbeddingGenerator::generate_embedding(const std::string& text) {
    // === ЗАЩИТА СТАТИСТИКИ И КЭША ===
    std::lock_guard<std::mutex> lock(model_mutex_);
    
    stats_.total_requests++;
    auto start_time = std::chrono::steady_clock::now();

    Logger::instance().info("EMBEDDING: generate_embedding called, text_len=" + std::to_string(text.length()));
    Logger::instance().flush_file_log();

    // Проверяем кэш
    if (cache_enabled_) {
        auto cached = get_cached_embedding(text);
        if (!cached.empty()) {
            stats_.cache_hits++;
            return cached;
        }
    }
    
    std::vector<float> embedding;
    
    // Выбираем источник в зависимости от режима
    bool use_cloud = false;
    
    switch (embedding_mode_) {
        case EmbeddingMode::CloudOnly:
            use_cloud = true;
            break;
            
        case EmbeddingMode::LocalOnly:
            use_cloud = false;
            break;
            
        case EmbeddingMode::Hybrid:
        case EmbeddingMode::Auto:
            use_cloud = cloud_priority_ && cloud_configured_ && cloud_available_;
            break;
    }
    
    // Генерируем эмбеддинг
    if (use_cloud) {
        embedding = generate_embedding_cloud(text);
        
        // Fallback на локальный если облако не сработало
        if (embedding.empty() && local_model_loaded_) {
            std::cerr << "[EmbeddingGenerator] Cloud failed, falling back to local" << std::endl;
            embedding = generate_embedding_local(text);
        }
    } else {
        embedding = generate_embedding_local(text);
        
        // Fallback на облако если локальный не сработал
        if (embedding.empty() && cloud_configured_ && cloud_available_) {
            std::cerr << "[EmbeddingGenerator] Local failed, falling back to cloud" << std::endl;
            embedding = generate_embedding_cloud(text);
        }
    }
    
    // Обновляем статистику
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    stats_.avg_latency_ms = (stats_.avg_latency_ms * (stats_.total_requests - 1) + 
                            static_cast<double>(duration)) / stats_.total_requests;
    
    // Кэшируем результат
    if (cache_enabled_ && !embedding.empty()) {
        cache_embedding(text, embedding);
    } else if (embedding.empty()) {
        stats_.failures++;
    }
    
    return embedding;
}

// ============================================================================
// Облачный эмбеддинг
// ============================================================================

std::vector<float> EmbeddingGenerator::generate_embedding_cloud(const std::string& text) {
    // ВНИМАНИЕ: Вызывается ТОЛЬКО из generate_embedding где mutex уже захвачен!
    // НЕ нужно захватывать mutex здесь
    
    if (!cloud_configured_) {
        std::cerr << "[EmbeddingGenerator] Cloud not configured" << std::endl;
        return std::vector<float>(embedding_dimension_, 0.0f);
    }
    
    if (text.empty()) {
        std::cerr << "[EmbeddingGenerator] Empty text for embedding" << std::endl;
        return std::vector<float>(embedding_dimension_, 0.0f);
    }
    
    stats_.cloud_requests++;
    
    std::cout << "[EmbeddingGenerator] Using cloud provider: " 
              << cloud_config_.model_name << std::endl;
    
    // Вызываем соответствующий API
    switch (cloud_config_.provider) {
        case CloudEmbeddingProvider::HuggingFace:
            return call_huggingface_api(text);
            
        case CloudEmbeddingProvider::OpenRouter:
            return call_openrouter_api(text);
            
        case CloudEmbeddingProvider::Custom:
            return call_custom_api(text);
            
        default:
            std::cerr << "[EmbeddingGenerator] Unknown cloud provider" << std::endl;
            return std::vector<float>(embedding_dimension_, 0.0f);
    }
}

std::vector<float> EmbeddingGenerator::call_huggingface_api(const std::string& text) {
    // HuggingFace Inference API
    // POST https://api-inference.huggingface.co/models/sentence-transformers/all-MiniLM-L6-v2
    // {"inputs": "text"}
    
    std::string url = "https://api-inference.huggingface.co/models/sentence-transformers/all-MiniLM-L6-v2";
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[EmbeddingGenerator] Failed to initialize CURL" << std::endl;
        return std::vector<float>(embedding_dimension_, 0.0f);
    }
    
    // Формируем JSON запрос
    json request_body;
    request_body["inputs"] = text;
    request_body["options"] = {
        {"wait_for_model", true}
    };
    
    std::string body = request_body.dump();
    std::string response;
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    if (!cloud_config_.api_key.empty()) {
        std::string auth = "Authorization: Bearer " + cloud_config_.api_key;
        headers = curl_slist_append(headers, auth.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, cloud_config_.timeout_ms);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "[EmbeddingGenerator] HuggingFace API error: " 
                  << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return std::vector<float>(embedding_dimension_, 0.0f);
    }
    
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (response_code != 200) {
        std::cerr << "[EmbeddingGenerator] HuggingFace HTTP error: " 
                  << response_code << ", Response: " << response.substr(0, 200) << std::endl;
        return std::vector<float>(embedding_dimension_, 0.0f);
    }
    
    // Парсим ответ
    try {
        auto embedding_json = json::parse(response);
        
        if (embedding_json.is_array() && !embedding_json.empty()) {
            std::vector<float> embedding;
            embedding.reserve(embedding_json.size());
            
            for (const auto& val : embedding_json) {
                embedding.push_back(val.get<float>());
            }
            
            // Нормализуем
            normalize_vector(embedding);
            
            std::cout << "[EmbeddingGenerator] HuggingFace embedding: " 
                      << embedding.size() << " dimensions" << std::endl;
            
            return embedding;
        }
    } catch (const json::exception& e) {
        std::cerr << "[EmbeddingGenerator] JSON parse error: " << e.what() << std::endl;
    }
    
    return std::vector<float>(embedding_dimension_, 0.0f);
}

std::vector<float> EmbeddingGenerator::call_openrouter_api(const std::string& text) {
    // OpenRouter API (если поддерживает эмбеддинги)
    // TODO: Реализовать когда OpenRouter добавит embeddings API
    
    std::cerr << "[EmbeddingGenerator] OpenRouter embeddings not yet supported" << std::endl;
    return std::vector<float>(embedding_dimension_, 0.0f);
}

std::vector<float> EmbeddingGenerator::call_custom_api(const std::string& text) {
    // Кастомный API endpoint
    // Ожидается формат: {"embedding": [float, ...]}
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "[EmbeddingGenerator] Failed to initialize CURL" << std::endl;
        return std::vector<float>(embedding_dimension_, 0.0f);
    }
    
    json request_body;
    request_body["input"] = text;
    request_body["model"] = cloud_config_.model_name;
    
    std::string body = request_body.dump();
    std::string response;
    
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    
    if (!cloud_config_.api_key.empty()) {
        std::string auth = "Authorization: Bearer " + cloud_config_.api_key;
        headers = curl_slist_append(headers, auth.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, cloud_config_.endpoint_url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, cloud_config_.timeout_ms);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    
    CURLcode res = curl_easy_perform(curl);
    
    if (res != CURLE_OK) {
        std::cerr << "[EmbeddingGenerator] Custom API error: " 
                  << curl_easy_strerror(res) << std::endl;
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return std::vector<float>(embedding_dimension_, 0.0f);
    }
    
    long response_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    if (response_code != 200) {
        std::cerr << "[EmbeddingGenerator] Custom API HTTP error: " 
                  << response_code << std::endl;
        return std::vector<float>(embedding_dimension_, 0.0f);
    }
    
    // Парсим ответ
    try {
        auto response_json = json::parse(response);
        
        if (response_json.contains("embedding") && response_json["embedding"].is_array()) {
            std::vector<float> embedding;
            for (const auto& val : response_json["embedding"]) {
                embedding.push_back(val.get<float>());
            }
            
            normalize_vector(embedding);
            return embedding;
        }
    } catch (const json::exception& e) {
        std::cerr << "[EmbeddingGenerator] JSON parse error: " << e.what() << std::endl;
    }
    
    return std::vector<float>(embedding_dimension_, 0.0f);
}

// ============================================================================
// Локальный эмбеддинг
// ============================================================================

std::vector<float> EmbeddingGenerator::generate_embedding_local(const std::string& text) {
    // ВНИМАНИЕ: Вызывается ТОЛЬКО из generate_embedding где mutex уже захвачен!
    // НЕ нужно захватывать mutex здесь
    
    try {
        std::cout << "[EmbeddingGenerator] === LOCAL EMBEDDING START ===" << std::endl;
        Logger::instance().info("EMBEDDING: LOCAL START");
        Logger::instance().flush_file_log();
        
        // Ленивая загрузка: если модель не загружена, загрузим сейчас
        if (!local_model_loaded_ && lazy_load_enabled_ && !local_model_path_.empty()) {
            std::cout << "[EmbeddingGenerator] Lazy loading model for embedding..." << std::endl;
            if (!load_local_model(local_model_path_)) {
                std::cerr << "[EmbeddingGenerator] Failed to load model for lazy loading" << std::endl;
                return std::vector<float>(embedding_dimension_, 0.0f);
            }
        }

        if (!local_model_loaded_) {
            std::cerr << "[EmbeddingGenerator] Local model not loaded" << std::endl;
            return std::vector<float>(embedding_dimension_, 0.0f);
        }

        if (text.empty()) {
            std::cerr << "[EmbeddingGenerator] Empty text for embedding" << std::endl;
            return std::vector<float>(embedding_dimension_, 0.0f);
        }

        stats_.local_requests++;

        // Отладка: проверяем текст
        std::cout << "[EmbeddingGenerator] Input text length: " << text.length() << " chars" << std::endl;
        std::cout << "[EmbeddingGenerator] Input text preview: " << text.substr(0, std::min<size_t>(100, text.length())) << std::endl;

        // Токенизация
        std::cout << "[EmbeddingGenerator] About to tokenize..." << std::endl;
        const llama_vocab* vocab = llama_model_get_vocab(model_.get());
        int vocab_size = llama_vocab_n_tokens(vocab);
        std::vector<llama_token> tokens(vocab_size);

        // Токенизируем текст
        int n_tokens = llama_tokenize(
            vocab,
            text.c_str(),
            static_cast<int>(text.length()),
            tokens.data(),
            static_cast<int>(tokens.size()),
            true,   // add_special = true для BGE-M3 (добавляет BOS)
            false   // parse_special = false
        );

        std::cout << "[EmbeddingGenerator] Tokenization result: " << n_tokens << " tokens" << std::endl;

        if (n_tokens <= 0) {
            std::cerr << "[EmbeddingGenerator] Tokenization failed: " << n_tokens << std::endl;
            return std::vector<float>(embedding_dimension_, 0.0f);
        }

        tokens.resize(n_tokens);

        // Создаём batch для эмбеддинга
        std::cout << "[EmbeddingGenerator] About to create batch..." << std::endl;
        llama_batch batch = llama_batch_init(n_tokens, 0, 1);
        batch.n_tokens = n_tokens;  // ВАЖНО: устанавливаем количество токенов

        for (int i = 0; i < n_tokens; ++i) {
            batch.token[i] = tokens[i];
            batch.pos[i] = i;
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            batch.logits[i] = 0;  // logits не нужны для эмбеддингов
        }

        // Для моделей с pooling (как BGE-M3), logits нужен только у последнего токена
        if (pooling_type_ != LLAMA_POOLING_TYPE_NONE) {
            batch.logits[n_tokens - 1] = 1;
            std::cout << "[EmbeddingGenerator] Set logits for last token (index " << (n_tokens - 1) << ")" << std::endl;
        } else {
            // Для моделей без pooling - logits у всех токенов
            for (int i = 0; i < n_tokens; ++i) {
                batch.logits[i] = 1;
            }
            std::cout << "[EmbeddingGenerator] Set logits for all " << n_tokens << " tokens" << std::endl;
        }

        std::cout << "[EmbeddingGenerator] Batch info: " << n_tokens << " tokens, pooling="
                  << (pooling_type_ == LLAMA_POOLING_TYPE_NONE ? "NONE" :
                      pooling_type_ == LLAMA_POOLING_TYPE_MEAN ? "MEAN" :
                      pooling_type_ == LLAMA_POOLING_TYPE_CLS ? "CLS" :
                      pooling_type_ == LLAMA_POOLING_TYPE_LAST ? "LAST" : "UNKNOWN")
                  << std::endl;

        // Encode/Decode
        // Для encoder моделей (BERT-like) используем llama_encode
        // Для decoder моделей (GPT-like) используем llama_decode
        int32_t ret;
        std::cout << "[EmbeddingGenerator] About to call llama_" << (has_encoder_ ? "encode" : "decode") << "..." << std::endl;
        if (has_encoder_) {
            ret = llama_encode(context_.get(), batch);
        } else {
            ret = llama_decode(context_.get(), batch);
        }
        std::cout << "[EmbeddingGenerator] llama_" << (has_encoder_ ? "encode" : "decode") << " returned: " << ret << std::endl;

        if (ret != 0) {
            std::cerr << "[EmbeddingGenerator] llama_" << (has_encoder_ ? "encode" : "decode")
                      << " failed: " << ret << std::endl;
            llama_batch_free(batch);
            return std::vector<float>(embedding_dimension_, 0.0f);
        }

        // Получаем эмбеддинг
        std::vector<float> embedding;

        if (pooling_type_ != LLAMA_POOLING_TYPE_NONE) {
            // Для моделей с pooling (BGE-M3, etc)
            std::cout << "[EmbeddingGenerator] Getting sequence embeddings..." << std::endl;
            float* embd = llama_get_embeddings_seq(context_.get(), 0);
            if (embd) {
                embedding.assign(embd, embd + embedding_dimension_);
                std::cout << "[EmbeddingGenerator] Local embedding generated: " << embedding_dimension_ << " dimensions" << std::endl;
            } else {
                std::cerr << "[EmbeddingGenerator] Failed to get sequence embeddings" << std::endl;
            }
        } else {
            // Для моделей без pooling - берём эмбеддинг последнего токена
            std::cout << "[EmbeddingGenerator] Getting token embeddings..." << std::endl;
            float* embd = llama_get_embeddings_ith(context_.get(), n_tokens - 1);
            if (embd) {
                embedding.assign(embd, embd + embedding_dimension_);
            } else {
                std::cerr << "[EmbeddingGenerator] Failed to get token embeddings" << std::endl;
            }
        }

        llama_batch_free(batch);
        std::cout << "[EmbeddingGenerator] === LOCAL EMBEDDING END ===" << std::endl;
        Logger::instance().info("EMBEDDING: LOCAL END");
        Logger::instance().flush_file_log();
        return embedding;

    } catch (const std::exception& e) {
        std::cerr << "[EmbeddingGenerator CRASH] Exception in generate_embedding_local: " << e.what() << std::endl;
        return std::vector<float>(embedding_dimension_, 0.0f);
    } catch (...) {
        std::cerr << "[EmbeddingGenerator CRASH] Unknown exception in generate_embedding_local" << std::endl;
        return std::vector<float>(embedding_dimension_, 0.0f);
    }
}

// ============================================================================
// Кэширование
// ============================================================================

std::vector<float> EmbeddingGenerator::get_cached_embedding(const std::string& text) {
    auto hash = compute_hash(text);
    auto it = cache_.find(hash);
    
    if (it != cache_.end()) {
        it->second.last_access = std::chrono::steady_clock::now();
        it->second.access_count++;
        return it->second.embedding;
    }
    
    return std::vector<float>();
}

void EmbeddingGenerator::cache_embedding(const std::string& text, const std::vector<float>& embedding) {
    if (cache_.size() >= max_cache_size_) {
        cleanup_cache();
    }
    
    auto hash = compute_hash(text);
    cache_[hash] = {
        embedding,
        std::chrono::steady_clock::now(),
        1
    };
}

void EmbeddingGenerator::cleanup_cache() {
    // LRU eviction: удаляем наименее используемые записи
    if (cache_.empty()) return;
    
    // Находим запись с наименьшим access_count и oldest last_access
    auto min_it = cache_.begin();
    for (auto it = cache_.begin(); it != cache_.end(); ++it) {
        if (it->second.access_count < min_it->second.access_count ||
            (it->second.access_count == min_it->second.access_count &&
             it->second.last_access < min_it->second.last_access)) {
            min_it = it;
        }
    }
    
    cache_.erase(min_it);
}

void EmbeddingGenerator::clear_cache() {
    cache_.clear();
}

std::string EmbeddingGenerator::compute_text_hash(const std::string& text) {
    return compute_hash(text);
}

// ============================================================================
// Статистика
// ============================================================================

int EmbeddingGenerator::get_embedding_dimension() const {
    return embedding_dimension_;
}

void EmbeddingGenerator::reset_stats() {
    stats_ = EmbeddingStats();
}

} // namespace core
} // namespace llama_gui
