#include "../include/core/llama_interface.h"
#include "../include/core/llama_interface_impl.h"
#include "../include/core/logger.h"
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <functional>
#include <nlohmann/json.hpp>

namespace llama_gui {
namespace core {

// Static callback functions
size_t LlamaInterface::Impl::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::string* response = static_cast<std::string*>(userp);
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

size_t LlamaInterface::Impl::StreamingWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    StreamingData* stream_data = static_cast<StreamingData*>(userp);
    stream_data->buffer.append(static_cast<char*>(contents), total_size);

    // Process complete lines from buffer
    size_t pos = 0;
    while ((pos = stream_data->buffer.find('\n')) != std::string::npos) {
        std::string line = stream_data->buffer.substr(0, pos);
        stream_data->buffer.erase(0, pos + 1);

        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Skip empty lines
        if (line.empty() || line == "\r") continue;

        // Check for data: prefix
        if (line.rfind("data: ", 0) == 0) {
            std::string data = line.substr(6); // Remove "data: " prefix

            // Check for [DONE] marker
            if (data == "[DONE]") {
                stream_data->callback("", true);
                break;
            }

            // Parse and send JSON chunk
            try {
                json chunk = json::parse(data);
                stream_data->callback(chunk.dump(), false);
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse streaming chunk: " << e.what() << std::endl;
            }
        }
    }

    return total_size;
}

size_t LlamaInterface::Impl::AsyncStreamingWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    StreamingRequestData* stream_data = static_cast<StreamingRequestData*>(userp);

    // Проверка на корректность указателя
    if (!stream_data) {
        return 0;  // Ошибка - нет данных для записи
    }

    // Если запрос завершён или неактивен, прерываем передачу
    // Возврат 0 заставляет curl прервать запрос
    if (stream_data->completed || !stream_data->is_active) {
        return 0;  // Это заставит curl прервать передачу!
    }

    try {
        stream_data->buffer.append(static_cast<char*>(contents), total_size);
    } catch (...) {
        // Ошибка при записи в буфер - отменяем поток
        stream_data->is_active = false;
        return 0;
    }

    // Process complete lines from buffer
    size_t pos = 0;
    while ((pos = stream_data->buffer.find('\n')) != std::string::npos) {
        // OPTIMIZATION: Use string_view to avoid substring allocation
        std::string line = stream_data->buffer.substr(0, pos);
        stream_data->buffer.erase(0, pos + 1);

        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Skip empty lines
        if (line.empty() || line == "\r") continue;

        // Check for data: prefix
        if (line.rfind("data: ", 0) == 0) {
            std::string data = line.substr(6); // Remove "data: " prefix

            // Check for [DONE] marker
            if (data == "[DONE]") {
                if (Logger::instance().is_debug_mode()) {
                    std::cout << "[DEBUG] Streaming received [DONE]" << std::endl;
                }
                if (stream_data->callback && !stream_data->final_sent) {
                    stream_data->callback("", true);
                    stream_data->final_sent = true;
                }
                stream_data->completed = true;
                break;
            }

            // Parse and send JSON chunk
            try {
                json chunk = json::parse(data);
                if (Logger::instance().is_debug_mode()) {
                    std::cout << "[DEBUG] Received chunk: " << data.substr(0, 100) << "..." << std::endl;
                }
                if (stream_data->callback) {
                    stream_data->callback(chunk.dump(), false);
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse streaming chunk: " << e.what() << " Line: '" << line << "'" << std::endl;
            }
        }
        
        // Re-check active flag after processing each line
        if (!stream_data->is_active || stream_data->completed) {
            return 0;  // Прерываем передачу
        }
    }

    return total_size;
}

// Methods of LlamaInterface::Impl
std::string LlamaInterface::Impl::make_http_request(const std::string& endpoint, const std::string& method, const json& data) {
    return make_http_request_async(endpoint, method, data, 30);
}

std::string LlamaInterface::Impl::make_http_request_async(const std::string& endpoint, const std::string& method, const json& data, int timeout_seconds) {
    // Создаём response в куче, чтобы он жил пока запрос выполняется
    auto response = std::make_unique<std::string>();
    CURL* curl = curl_easy_init();

    if (!curl) {
        std::cerr << "Failed to create curl easy handle" << std::endl;
        return *response;
    }

    std::string url = endpoint;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_seconds);

    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, "Accept: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set write callback - передаём указатель на response в куче
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response.get());

    // Set method and data
    std::string json_data;
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        json_data = data.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

        // Clean JSON data using validate_and_clean_utf8 which handles:
        // - UTF-8 validation
        // - Control character filtering (0x00-0x1F, 0x7F except whitespace)
        std::string final_json_data = validate_and_clean_utf8(json_data);

        // OPTIMIZATION: Limit post_data_storage_ size to prevent memory growth
        if (post_data_storage_.size() > 100) {
            post_data_storage_.erase(post_data_storage_.begin(), 
                                     post_data_storage_.begin() + 50);
        }

        // Store the data to ensure it lives until the request completes
        post_data_storage_.push_back(std::move(final_json_data));
        const std::string& stored_data = post_data_storage_.back();

        // Set POST data - optimized without verbose debug output
        const char* post_data = stored_data.c_str();
        size_t post_data_len = stored_data.length();

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data_len);
    }

    // Add to multi handle
    curl_multi_add_handle(multi_handle_, curl);
    easy_handles_.push_back(curl);

    // Сохраняем response в map - теперь он живёт пока запрос выполняется
    std::lock_guard<std::mutex> lock(responses_mutex_);
    pending_responses_[curl] = std::move(response);

    // Возвращаем пустую строку - результат будет позже через callback
    return "";
}

void LlamaInterface::Impl::process_async_requests() {
    if (!multi_handle_) {
        return;
    }

    // Очистка завершённых streaming запросов (безопасное удаление)
    {
        std::lock_guard<std::mutex> lock(responses_mutex_);
        for (auto* stream_data : completed_streaming_requests_) {
            delete stream_data;
        }
        completed_streaming_requests_.clear();
    }

    // OPTIMIZATION: Use curl_multi_wait for efficient blocking instead of busy polling
    int numfds = 0;
    CURLMcode mc = curl_multi_wait(multi_handle_, nullptr, 0, 10, &numfds);

    if (mc != CURLM_OK) {
        std::cerr << "curl_multi_wait error: " << mc << std::endl;
    }

    // Now perform the actual I/O
    int still_running = 0;
    mc = curl_multi_perform(multi_handle_, &still_running);

    if (mc != CURLM_OK && mc != CURLM_CALL_MULTI_PERFORM) {
        std::cerr << "curl_multi_perform error: " << mc << std::endl;
    }

    // Check for completed streaming requests (natural completion, not user-stopped)
    // Note: User-stopped requests are already removed by stop_streaming_requests()
    std::vector<CURL*> to_remove;
    {
        std::lock_guard<std::mutex> lock(responses_mutex_);

        // Find naturally completed streaming requests (not stopped by user)
        for (auto it = streaming_requests_.begin(); it != streaming_requests_.end(); ) {
            CURL* easy_handle = it->first;
            StreamingRequestData* stream_data = it->second;

            if (stream_data && stream_data->completed && stream_data->final_sent) {
                // Request completed naturally - mark for removal
                std::cout << "Removing completed streaming request" << std::endl;
                to_remove.push_back(easy_handle);
                it = streaming_requests_.erase(it);
                completed_streaming_requests_.push_back(stream_data);
            } else {
                ++it;
            }
        }
    }

    // Remove completed handles outside the lock
    for (CURL* easy_handle : to_remove) {
        curl_multi_remove_handle(multi_handle_, easy_handle);
        curl_easy_cleanup(easy_handle);
        easy_handles_.erase(std::remove(easy_handles_.begin(), easy_handles_.end(), easy_handle),
                            easy_handles_.end());
    }

    // Check for completed transfers from curl
    CURLMsg* msg;
    int msgs_left;
    while ((msg = curl_multi_info_read(multi_handle_, &msgs_left))) {
        if (msg->msg == CURLMSG_DONE) {
            CURL* easy_handle = msg->easy_handle;
            long response_code = 0;
            curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &response_code);

            // Подробное логирование результатов запроса
            std::cout << "Request completed - response_code: " << response_code
                      << ", result: " << msg->data.result;
            
            if (msg->data.result != CURLE_OK) {
                std::cout << " (" << curl_easy_strerror((CURLcode)msg->data.result) << ")";
                
                // Дополнительная информация для отладки
                char* effective_url = nullptr;
                curl_easy_getinfo(easy_handle, CURLINFO_EFFECTIVE_URL, &effective_url);
                if (effective_url) {
                    std::cout << " [URL: " << effective_url << "]";
                }
                
                double total_time = 0;
                curl_easy_getinfo(easy_handle, CURLINFO_TOTAL_TIME, &total_time);
                std::cout << " [Total time: " << total_time << "s]";
                
                double num_connects = 0;
                curl_easy_getinfo(easy_handle, CURLINFO_NUM_CONNECTS, &num_connects);
                std::cout << " [Connects: " << (int)num_connects << "]";
            }
            std::cout << std::endl;

            std::lock_guard<std::mutex> lock(responses_mutex_);

            // Check if this is a streaming request
            auto stream_it = streaming_requests_.find(easy_handle);
            if (stream_it != streaming_requests_.end()) {
                StreamingRequestData* stream_data = stream_it->second;
                
                if (msg->data.result != CURLE_OK) {
                    // Ошибка при streaming
                    std::cerr << "❌ ERROR: Streaming request failed with error " 
                              << msg->data.result << " (" 
                              << curl_easy_strerror((CURLcode)msg->data.result) << ")" << std::endl;
                    
                    // Отправляем ошибку в callback
                    if (stream_data && stream_data->callback) {
                        stream_data->callback("", true);
                    }
                } else {
                    std::cout << "Streaming request completed naturally" << std::endl;
                    // Finalize streaming request
                    // Отправляем финальный callback
                    if (stream_data && stream_data->callback && !stream_data->final_sent) {
                        stream_data->callback("", true);
                        stream_data->final_sent = true;
                    }
                }

                // Remove from map and schedule for cleanup
                streaming_requests_.erase(stream_it);
                completed_streaming_requests_.push_back(stream_data);
            } else {
                // Regular request - already removed if it was stopped by user
                auto it = pending_responses_.find(easy_handle);
                if (it != pending_responses_.end()) {
                    pending_responses_.erase(it);
                }
            }

            // Remove from multi handle (if not already removed by stop_streaming_requests)
            curl_multi_remove_handle(multi_handle_, easy_handle);
            curl_easy_cleanup(easy_handle);
            easy_handles_.erase(std::remove(easy_handles_.begin(), easy_handles_.end(), easy_handle),
                                easy_handles_.end());
        }
    }
}

void LlamaInterface::Impl::make_streaming_http_request(const std::string& endpoint, const std::string& method, const json& data, LlamaInterface::StreamCallback callback) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to create curl easy handle for streaming" << std::endl;
        callback("", true);
        return;
    }

    std::string url = endpoint;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    // Set timeouts to prevent hanging
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 1800000L);  // 1800 seconds (30 min) for reasoning models
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 10000L);  // 10 second connect timeout
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1L);  // Min 1 byte/sec
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 300L);   // 300 seconds (5 min) - increased for RAG (server needs time to process large context)

    if (Logger::instance().is_debug_mode()) {
        std::cout << "[DEBUG] Streaming request started: " << endpoint << std::endl;
        std::cout << "[DEBUG] Request data size: " << data.dump().size() << " bytes" << std::endl;
    }
    
    // Логирование для RAG запросов с большим промптом
    if (data.contains("messages") && data["messages"].is_array()) {
        size_t total_prompt_size = 0;
        for (const auto& msg : data["messages"]) {
            if (msg.contains("content")) {
                total_prompt_size += msg["content"].dump().size();
            }
        }
        if (total_prompt_size > 10000) {  // Логируем только большие запросы (>10KB)
            std::cout << "[RAG DEBUG] Large streaming request: " << total_prompt_size 
                      << " bytes, " << data["messages"].size() << " messages" << std::endl;
        }
    }

    // Force HTTP/1.1 and disable Nagle's algorithm for better streaming
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, (long)CURL_HTTP_VERSION_1_1);
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L);

    // Set headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json; charset=utf-8");
    headers = curl_slist_append(headers, "Accept: text/event-stream");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Set method and data
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        std::string json_data = data.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);

        // Clean JSON data
        std::string clean_json_data = validate_and_clean_utf8(json_data);

        // Store the data to ensure it lives until the request completes
        post_data_storage_.push_back(std::move(clean_json_data));
        const std::string& stored_data = post_data_storage_.back();

        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, stored_data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, stored_data.length());
    }

    // Create streaming request data with async callback
    StreamingRequestData* stream_data = new StreamingRequestData{callback, "", true};

    // Set async streaming write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, AsyncStreamingWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, stream_data);

    // Add to multi handle for async processing
    {
        std::lock_guard<std::mutex> lock(responses_mutex_);
        curl_multi_add_handle(multi_handle_, curl);
        easy_handles_.push_back(curl);
        streaming_requests_[curl] = stream_data;
    }

    // Start the request (non-blocking)
    int still_running = 0;
    curl_multi_perform(multi_handle_, &still_running);
}

void LlamaInterface::Impl::stop_streaming_requests() {
    std::lock_guard<std::mutex> lock(responses_mutex_);

    // Принудительно удаляем все streaming запросы из multi_handle_
    // Это гарантирует немедленную остановку без зависания
    std::vector<CURL*> to_remove;
    for (auto& pair : streaming_requests_) {
        CURL* easy_handle = pair.first;
        StreamingRequestData* stream_data = pair.second;

        if (stream_data) {
            // Отправляем финальный callback если ещё не отправлен
            if (!stream_data->final_sent && stream_data->callback) {
                stream_data->callback("", true);
                stream_data->final_sent = true;
            }

            // Помечаем для удаления
            to_remove.push_back(easy_handle);
        }
    }

    // Принудительно удаляем запросы из multi handle
    for (CURL* easy_handle : to_remove) {
        curl_multi_remove_handle(multi_handle_, easy_handle);
        curl_easy_cleanup(easy_handle);
        easy_handles_.erase(std::remove(easy_handles_.begin(), easy_handles_.end(), easy_handle),
                            easy_handles_.end());
    }

    // Очищаем map streaming запросов
    streaming_requests_.clear();
}

// Public wrapper methods for LlamaInterface
std::string LlamaInterface::make_http_request(const std::string& endpoint, const std::string& method, const json& data) const {
    return pImpl->make_http_request(endpoint, method, data);
}

void LlamaInterface::process_async_requests() {
    pImpl->process_async_requests();
}

void LlamaInterface::make_streaming_http_request(const std::string& endpoint, const std::string& method, const json& data, StreamCallback callback) {
    pImpl->make_streaming_http_request(endpoint, method, data, callback);
}

void LlamaInterface::stop_streaming_requests() {
    pImpl->stop_streaming_requests();
}

// ============================================================================
// KV-cache Slot Operations (операции с слотами KV-cache)
// ============================================================================

// Примечание: get_slots_status() уже определён в llama_interface_utils.cpp

bool LlamaInterface::save_slot_kv_cache(int slot_id, const std::string& filename) {
    auto result = save_slot_kv_cache_detailed(slot_id, filename);
    return result.success;
}

LlamaInterface::SlotOperationResult LlamaInterface::save_slot_kv_cache_detailed(
    int slot_id, const std::string& filename)
{
    SlotOperationResult result;
    result.slot_id = slot_id;
    result.filename = filename;
    result.success = false;

    try {
        json request_data = {{"filename", filename}};
        std::string endpoint = "/slots/" + std::to_string(slot_id) + "?action=save";

        std::string response_str = make_http_request(endpoint, "POST", request_data);
        json response = json::parse(response_str);

        if (response.contains("error")) {
            result.error_message = response["error"].dump();
            return result;
        }

        result.success = true;
        result.n_tokens = response.value("n_saved", 0);
        result.n_bytes = response.value("n_written", 0);

        if (response.contains("timings")) {
            result.processing_ms = response["timings"].value("prompt_ms", 0.0);
        }

    } catch (const std::exception& e) {
        result.error_message = std::string("Exception: ") + e.what();
    }

    return result;
}

bool LlamaInterface::restore_slot_kv_cache(int slot_id, const std::string& filename) {
    auto result = restore_slot_kv_cache_detailed(slot_id, filename);
    return result.success;
}

LlamaInterface::SlotOperationResult LlamaInterface::restore_slot_kv_cache_detailed(
    int slot_id, const std::string& filename)
{
    SlotOperationResult result;
    result.slot_id = slot_id;
    result.filename = filename;
    result.success = false;

    try {
        json request_data = {{"filename", filename}};
        std::string endpoint = "/slots/" + std::to_string(slot_id) + "?action=restore";

        std::string response_str = make_http_request(endpoint, "POST", request_data);
        json response = json::parse(response_str);

        if (response.contains("error")) {
            result.error_message = response["error"].dump();
            return result;
        }

        result.success = true;
        result.n_tokens = response.value("n_restored", 0);
        result.n_bytes = response.value("n_read", 0);

        if (response.contains("timings")) {
            result.processing_ms = response["timings"].value("prompt_ms", 0.0);
        }

    } catch (const std::exception& e) {
        result.error_message = std::string("Exception: ") + e.what();
    }

    return result;
}

bool LlamaInterface::reset_slot(int slot_id) {
    try {
        std::string endpoint = "/slots/" + std::to_string(slot_id) + "?action=reset";
        make_http_request(endpoint, "POST", json{});
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Failed to reset slot: ") + e.what();
        return false;
    }
}

bool LlamaInterface::erase_slot(int slot_id) {
    try {
        std::string endpoint = "/slots/" + std::to_string(slot_id) + "?action=erase";
        make_http_request(endpoint, "POST", json{});
        return true;
    } catch (const std::exception& e) {
        last_error_ = std::string("Failed to erase slot: ") + e.what();
        return false;
    }
}

LlamaInterface::SlotOperationResult LlamaInterface::tokenize_text_in_slot(int slot_id, const std::string& text) {
    SlotOperationResult result;
    result.slot_id = slot_id;
    result.success = false;

    try {
        // Используем endpoint /slots/{id}/text для загрузки текста в слот
        json request_data = {{"text", text}};
        std::string endpoint = "/slots/" + std::to_string(slot_id) + "/text";

        std::string response_str = make_http_request(endpoint, "POST", request_data);
        json response = json::parse(response_str);

        if (response.contains("error")) {
            result.error_message = response["error"].dump();
            return result;
        }

        result.success = true;
        result.n_tokens = response.value("n_tokens", 0);
        result.n_bytes = static_cast<int>(text.size());

        if (response.contains("timings")) {
            result.processing_ms = response["timings"].value("prompt_ms", 0.0);
        }

    } catch (const std::exception& e) {
        result.error_message = std::string("Exception: ") + e.what();
    }

    return result;
}

} // namespace core
} // namespace llama_gui