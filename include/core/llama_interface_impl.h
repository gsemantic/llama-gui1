#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <iostream>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

namespace llama_gui {
namespace core {

class LlamaInterface;

class LlamaInterface::Impl {
public:
    Impl(const std::string& server_url) : server_url_(server_url) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        // Initialize curl multi handle
        multi_handle_ = curl_multi_init();
        if (!multi_handle_) {
            std::cerr << "Failed to initialize curl multi handle" << std::endl;
        }
    }

    ~Impl() {
        // Cleanup all easy handles
        for (auto& handle : easy_handles_) {
            curl_easy_cleanup(handle);
        }
        if (multi_handle_) {
            curl_multi_cleanup(multi_handle_);
        }
        curl_global_cleanup();
    }

    // Methods
    std::string make_http_request(const std::string& endpoint, const std::string& method, const json& data);
    std::string make_http_request_async(const std::string& endpoint, const std::string& method, const json& data, int timeout_seconds);
    void process_async_requests();
    void make_streaming_http_request(const std::string& endpoint, const std::string& method, const json& data, LlamaInterface::StreamCallback callback);
    void stop_streaming_requests();

    // Static callback functions
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    struct StreamingData {
        LlamaInterface::StreamCallback callback;
        std::string buffer;
    };

    struct StreamingRequestData {
        LlamaInterface::StreamCallback callback;
        std::string buffer;
        bool is_active = true;
        bool completed = false;  // Запрос завершён, но данные ещё могут приходить
        bool final_sent = false;
    };

    static size_t StreamingWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t AsyncStreamingWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);

    // Member variables
    std::string server_url_;
    CURLM* multi_handle_ = nullptr;
    std::vector<CURL*> easy_handles_;
    std::unordered_map<CURL*, std::unique_ptr<std::string>> pending_responses_;  // Используем unique_ptr для безопасности
    std::unordered_map<CURL*, StreamingRequestData*> streaming_requests_;
    std::vector<StreamingRequestData*> completed_streaming_requests_;  // Завершённые запросы на удаление
    std::mutex responses_mutex_;
    std::vector<std::string> post_data_storage_;
};

} // namespace core
} // namespace llama_gui
