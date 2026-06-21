#include "../include/core/llama_interface.h"
#include <curl/curl.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <algorithm>

namespace llama_gui {
namespace core {

// Stream callback data structure
struct StreamData {
    LlamaInterface::StreamCallback callback;
    std::string buffer;
    bool is_final;

    StreamData(LlamaInterface::StreamCallback cb) : callback(cb), is_final(false) {}
};

// Helper function to get current timestamp
static std::string get_current_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    return std::to_string(time_t);
}

// HTTP request callback for libcurl
static size_t http_write_callback(void* contents, size_t size, size_t nmemb, std::string* s) {
    size_t total_size = size * nmemb;
    s->append((char*)contents, total_size);
    return total_size;
}

// Stream response callback for libcurl
static size_t http_stream_callback(void* contents, size_t size, size_t nmemb, StreamData* data) {
    size_t total_size = size * nmemb;
    std::string chunk((char*)contents, total_size);
    
    data->buffer += chunk;
    
    // Process complete SSE messages
    size_t pos;
    while ((pos = data->buffer.find("\n\n")) != std::string::npos) {
        std::string message = data->buffer.substr(0, pos);
        data->buffer.erase(0, pos + 2);
        
        // Parse SSE message
        std::istringstream iss(message);
        std::string line;
        std::string content;
        bool is_final = false;
        
        while (std::getline(iss, line)) {
            if (line.rfind("data: ", 0) == 0) {
                std::string data_content = line.substr(6);
                if (data_content == "[DONE]") {
                    is_final = true;
                    break;
                }
                
                // Try to parse JSON content
                try {
                    json j = json::parse(data_content);
                    if (j.contains("choices") && !j["choices"].empty()) {
                        auto& choice = j["choices"][0];
                        if (choice.contains("delta") && choice["delta"].contains("content")) {
                            content += choice["delta"]["content"].get<std::string>();
                        } else if (choice.contains("message") && choice["message"].contains("content")) {
                            content += choice["message"]["content"].get<std::string>();
                        }
                    }
                } catch (const json::exception& e) {
                    // If JSON parsing fails, treat as plain text
                    content += data_content;
                }
            }
        }
        
        // Send the content to callback
        if (!content.empty() || is_final) {
            data->callback(content, is_final);
        }
    }
    
    return total_size;
}

class LlamaInterface::Impl {
public:
    Impl() : curl_(nullptr), initialized_(false) {}
    
    ~Impl() {
        if (curl_) {
            curl_easy_cleanup(curl_);
        }
        if (initialized_) {
            curl_global_cleanup();
        }
    }
    
    bool initialize() {
        if (initialized_) return true;
        
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curl_ = curl_easy_init();
        if (!curl_) {
            last_error_ = "Failed to initialize curl";
            return false;
        }
        
        initialized_ = true;
        return true;
    }
    
    bool make_http_request(const std::string& endpoint, const std::string& method, 
                          const json& data, std::string& response) {
        if (!curl_) return false;
        
        std::string url = server_url_ + endpoint;
        std::string json_data = data.dump();
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        
        if (!api_key_.empty()) {
            std::string auth_header = "Authorization: Bearer " + api_key_;
            headers = curl_slist_append(headers, auth_header.c_str());
        }
        
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, http_write_callback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);
        
        if (method == "POST") {
            curl_easy_setopt(curl_, CURLOPT_POST, 1L);
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, json_data.c_str());
        } else if (method == "GET") {
            curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
        }
        
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_seconds_);
        curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 10L);
        curl_easy_setopt(curl_, CURLOPT_FOLLOWLOCATION, 1L);
        
        CURLcode res = curl_easy_perform(curl_);
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) {
            last_error_ = std::string("HTTP request failed: ") + curl_easy_strerror(res);
            return false;
        }
        
        long response_code;
        curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &response_code);
        
        if (response_code >= 400) {
            last_error_ = "HTTP error: " + std::to_string(response_code);
            return false;
        }
        
        return true;
    }
    
    bool make_streaming_request(const std::string& endpoint, const std::string& method,
                               const json& data, LlamaInterface::StreamCallback callback) {
        if (!curl_) return false;
        
        std::string url = server_url_ + endpoint;
        StreamData stream_data(callback);
        
        curl_easy_setopt(curl_, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, nullptr);
        
        // Add headers
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        headers = curl_slist_append(headers, "Accept: text/event-stream");
        headers = curl_slist_append(headers, "Cache-Control: no-cache");
        
        if (!api_key_.empty()) {
            std::string auth_header = "Authorization: Bearer " + api_key_;
            headers = curl_slist_append(headers, auth_header.c_str());
        }
        
        curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);
        
        if (method == "POST") {
            std::string json_data = data.dump();
            curl_easy_setopt(curl_, CURLOPT_POST, 1L);
            curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, json_data.c_str());
        } else if (method == "GET") {
            curl_easy_setopt(curl_, CURLOPT_HTTPGET, 1L);
        }
        
        curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, http_stream_callback);
        curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &stream_data);
        curl_easy_setopt(curl_, CURLOPT_TIMEOUT, timeout_seconds_);
        curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 10L);
        
        CURLcode res = curl_easy_perform(curl_);
        curl_slist_free_all(headers);
        
        if (res != CURLE_OK) {
            last_error_ = std::string("Streaming request failed: ") + curl_easy_strerror(res);
            return false;
        }
        
        return true;
    }
    
    // Getters and setters
    void set_server_url(const std::string& url) { server_url_ = url; }
    void set_api_key(const std::string& key) { api_key_ = key; }
    void set_timeout(int seconds) { timeout_seconds_ = seconds; }
    const std::string& get_last_error() const { return last_error_; }
    
private:
    CURL* curl_;
    bool initialized_;
    std::string server_url_;
    std::string api_key_;
    int timeout_seconds_;
    std::string last_error_;
};

LlamaInterface::LlamaInterface() 
    : server_url_("http://localhost:8081")
    , api_key_("")
    , timeout_seconds_(60)
    , last_error_("")
{
    pImpl = std::make_unique<Impl>();
}

LlamaInterface::~LlamaInterface() = default;

bool LlamaInterface::initialize(const std::string& server_url) {
    server_url_ = server_url;
    if (server_url_.back() == '/') {
        server_url_.pop_back();
    }
    
    return pImpl->initialize();
}

bool LlamaInterface::is_server_healthy() const {
    std::string response;
    if (pImpl->make_http_request("/health", "GET", json{}, response)) {
        try {
            json j = json::parse(response);
            return j.contains("status") && j["status"] == "ok";
        } catch (const json::exception&) {
            return false;
        }
    }
    return false;
}

json LlamaInterface::get_server_info() const {
    std::string response;
    if (pImpl->make_http_request("/props", "GET", json{}, response)) {
        try {
            return json::parse(response);
        } catch (const json::exception&) {
            return json{};
        }
    }
    return json{};
}

json LlamaInterface::get_models() const {
    std::string response;
    if (pImpl->make_http_request("/v1/models", "GET", json{}, response)) {
        try {
            return json::parse(response);
        } catch (const json::exception&) {
            return json{};
        }
    }
    return json{};
}

json LlamaInterface::get_slots_status() const {
    std::string response;
    if (pImpl->make_http_request("/slots", "GET", json{}, response)) {
        try {
            return json::parse(response);
        } catch (const json::exception&) {
            return json::array();
        }
    }
    return json::array();
}

LlamaInterface::ChatCompletionResponse LlamaInterface::create_chat_completion(const ChatCompletionRequest& request) {
    ChatCompletionResponse response;
    
    json req_data;
    req_data["model"] = request.model;
    req_data["messages"] = json::array();
    
    for (const auto& msg : request.messages) {
        req_data["messages"].push_back(msg.to_json());
    }
    
    req_data["max_tokens"] = request.max_tokens;
    req_data["temperature"] = request.temperature;
    req_data["stream"] = false; // Synchronous mode
    if (!request.stop.empty()) {
        req_data["stop"] = request.stop;
    }
    
    std::string response_str;
    if (pImpl->make_http_request("/v1/chat/completions", "POST", req_data, response_str)) {
        try {
            json j = json::parse(response_str);
            
            response.id = j.value("id", "");
            response.object = j.value("object", "");
            response.created = j.value("created", 0);
            response.model = j.value("model", "");
            
            if (j.contains("choices")) {
                for (const auto& choice : j["choices"]) {
                    ChatChoice chat_choice;
                    chat_choice.index = choice.value("index", 0);
                    chat_choice.finish_reason = choice.value("finish_reason", "");
                    chat_choice.message = ChatMessage::from_json(choice["message"]);
                    response.choices.push_back(chat_choice);
                }
            }
            
            response.usage = j.value("usage", json{});
            
        } catch (const json::exception& e) {
            last_error_ = std::string("Failed to parse response: ") + e.what();
        }
    } else {
        last_error_ = pImpl->get_last_error();
    }
    
    return response;
}

void LlamaInterface::create_chat_completion_streaming(const ChatCompletionRequest& request, StreamCallback callback) {
    json req_data;
    req_data["model"] = request.model;
    req_data["messages"] = json::array();
    
    for (const auto& msg : request.messages) {
        req_data["messages"].push_back(msg.to_json());
    }
    
    req_data["max_tokens"] = request.max_tokens;
    req_data["temperature"] = request.temperature;
    req_data["stream"] = true; // Streaming mode
    if (!request.stop.empty()) {
        req_data["stop"] = request.stop;
    }
    
    if (!pImpl->make_streaming_request("/v1/chat/completions", "POST", req_data, callback)) {
        last_error_ = pImpl->get_last_error();
        callback("", true); // Signal error to caller
    }
}

LlamaInterface::EmbeddingResponse LlamaInterface::create_embedding(const EmbeddingRequest& request) {
    EmbeddingResponse response;
    
    json req_data;
    req_data["model"] = request.model;
    req_data["input"] = request.input;
    req_data["encoding_format"] = request.encoding_format;
    
    std::string response_str;
    if (pImpl->make_http_request("/v1/embeddings", "POST", req_data, response_str)) {
        try {
            json j = json::parse(response_str);
            
            response.object = j.value("object", "");
            
            if (j.contains("data")) {
                for (const auto& item : j["data"]) {
                    EmbeddingData data;
                    data.index = item.value("index", 0);
                    data.embedding = item.value("embedding", std::vector<float>{});
                    response.data.push_back(data);
                }
            }
            
            response.usage = j.value("usage", json{});
            
        } catch (const json::exception& e) {
            last_error_ = std::string("Failed to parse embedding response: ") + e.what();
        }
    } else {
        last_error_ = pImpl->get_last_error();
    }
    
    return response;
}

void LlamaInterface::set_api_key(const std::string& api_key) {
    api_key_ = api_key;
}

void LlamaInterface::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
}


} // namespace core
} // namespace llama_gui