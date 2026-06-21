#include "../include/core/llama_interface.h"
#include "../include/core/llama_interface_impl.h"
#include "../include/core/logger.h"
#include <thread>
#include <iostream>
#include <sstream>

namespace llama_gui {
namespace core {

void LlamaInterface::create_chat_completion_streaming(const ChatCompletionRequest& request, StreamCallback callback) {
    // Prepare the request endpoint
    std::string endpoint = server_url_ + "/v1/chat/completions";

    // Convert request to JSON
    json request_json;
    request_json["model"] = request.model;
    request_json["stream"] = true;

    // Convert messages to JSON format
    json messages_json = json::array();
    for (const auto& msg : request.messages) {
        json msg_json;
        msg_json["role"] = (msg.role == MessageRole::User) ? "user" :
                          (msg.role == MessageRole::Assistant) ? "assistant" : "system";
        msg_json["content"] = msg.content;
        messages_json.push_back(msg_json);
    }
    request_json["messages"] = messages_json;

    // Add generation parameters
    request_json["max_tokens"] = request.max_tokens;
    request_json["temperature"] = request.temperature;
    request_json["top_p"] = request.top_p;
    request_json["top_k"] = request.top_k;
    request_json["min_p"] = request.min_p;
    request_json["repeat_penalty"] = request.repeat_penalty;
    request_json["presence_penalty"] = request.presence_penalty;
    request_json["frequency_penalty"] = request.frequency_penalty;

    // Mirostat parameters
    request_json["mirostat_mode"] = request.mirostat_mode;
    request_json["mirostat_tau"] = request.mirostat_tau;
    request_json["mirostat_eta"] = request.mirostat_eta;

    // Additional parameters
    request_json["stop_on_newline"] = request.stop_on_newline;

    if (!request.stop.empty()) {
        request_json["stop"] = request.stop;
    }

    // DEBUG: Print request
    if (Logger::instance().is_debug_mode()) {
        std::cout << "[DEBUG] Chat request: messages=" << messages_json.size()
                  << ", max_tokens=" << request.max_tokens
                  << ", temperature=" << request.temperature
                  << ", stream=true" << std::endl;
        std::cout << "[DEBUG] Last message role: " << (request.messages.back().role == MessageRole::User ? "user" : "assistant") << std::endl;
        std::cout << "[DEBUG] Last message length: " << request.messages.back().content.size() << " chars" << std::endl;
        
        // RAG DEBUG: Print full messages for debugging
        if (messages_json.size() > 0) {
            std::cout << "[DEBUG] === FULL MESSAGES START ===" << std::endl;
            for (size_t i = 0; i < messages_json.size(); ++i) {
                std::cout << "[DEBUG] Message[" << i << "] role=" << messages_json[i]["role"] 
                          << ", content_length=" << messages_json[i]["content"].get<std::string>().size() << std::endl;
                std::cout << "[DEBUG] Content[" << i << "]: " << messages_json[i]["content"].get<std::string>().substr(0, 500) << "..." << std::endl;
            }
            std::cout << "[DEBUG] === FULL MESSAGES END ===" << std::endl;
        }
    }

    // Start streaming request
    make_streaming_http_request(endpoint, "POST", request_json, callback);
}

std::future<ChatCompletionResponse> LlamaInterface::create_chat_completion_async(const ChatCompletionRequest& request) {
    // Prepare the request endpoint
    std::string endpoint = server_url_ + "/v1/chat/completions";

    // Convert request to JSON
    json request_json;
    request_json["model"] = request.model;
    request_json["stream"] = false;

    // Convert messages to JSON format
    json messages_json = json::array();
    for (const auto& msg : request.messages) {
        json msg_json;
        msg_json["role"] = (msg.role == MessageRole::User) ? "user" : 
                          (msg.role == MessageRole::Assistant) ? "assistant" : "system";
        msg_json["content"] = msg.content;
        messages_json.push_back(msg_json);
    }
    request_json["messages"] = messages_json;

    // Add generation parameters
    request_json["max_tokens"] = request.max_tokens;
    request_json["temperature"] = request.temperature;
    request_json["top_p"] = request.top_p;
    request_json["top_k"] = request.top_k;
    request_json["min_p"] = request.min_p;
    request_json["repeat_penalty"] = request.repeat_penalty;
    request_json["presence_penalty"] = request.presence_penalty;
    request_json["frequency_penalty"] = request.frequency_penalty;

    // Mirostat parameters
    request_json["mirostat_mode"] = request.mirostat_mode;
    request_json["mirostat_tau"] = request.mirostat_tau;
    request_json["mirostat_eta"] = request.mirostat_eta;

    // Additional parameters
    request_json["stop_on_newline"] = request.stop_on_newline;

    if (!request.stop.empty()) {
        request_json["stop"] = request.stop;
    }

    // Create a promise for the async result
    auto promise = std::make_shared<std::promise<ChatCompletionResponse>>();
    auto future = promise->get_future();

    // Make async HTTP request
    make_async_http_request(endpoint, "POST", request_json, [promise, request](const std::string& response) {
        try {
            ChatCompletionResponse completion_response;
            completion_response.id = "chatcmpl-" + std::to_string(std::time(nullptr));
            completion_response.object = "chat.completion";
            completion_response.created = std::time(nullptr);
            completion_response.model = request.model;

            if (response.empty()) {
                // Error response
                ChatCompletionResponse::ChatChoice choice;
                choice.index = 0;
                choice.message.role = MessageRole::Assistant;
                choice.message.content = "Error: No response from server";
                choice.finish_reason = "error";
                completion_response.choices.push_back(choice);
                promise->set_value(completion_response);
                return;
            }

            // Parse JSON response
            json response_json = json::parse(response);

            if (response_json.contains("choices") && !response_json["choices"].empty()) {
                for (const auto& choice_json : response_json["choices"]) {
                    ChatCompletionResponse::ChatChoice choice;
                    choice.index = choice_json.value("index", 0);
                    
                    if (choice_json.contains("message")) {
                        const auto& msg_json = choice_json["message"];
                        std::string role_str = msg_json.value("role", "assistant");
                        choice.message.role = (role_str == "user") ? MessageRole::User : 
                                            (role_str == "system") ? MessageRole::System : MessageRole::Assistant;
                        choice.message.content = msg_json.value("content", "");
                    }
                    
                    choice.finish_reason = choice_json.value("finish_reason", "stop");
                    completion_response.choices.push_back(choice);
                }
            }

            if (response_json.contains("usage")) {
                completion_response.usage = response_json["usage"];
            }

            promise->set_value(completion_response);
        } catch (const std::exception& e) {
            std::cerr << "Error in async chat completion: " << e.what() << std::endl;
            
            ChatCompletionResponse error_response;
            error_response.id = "error_" + std::to_string(std::time(nullptr));
            error_response.object = "chat.completion";
            error_response.created = std::time(nullptr);
            error_response.model = request.model;

            ChatCompletionResponse::ChatChoice choice;
            choice.index = 0;
            choice.message.role = MessageRole::Assistant;
            choice.message.content = "Error: " + std::string(e.what());
            choice.finish_reason = "error";
            error_response.choices.push_back(choice);

            promise->set_value(error_response);
        }
    });

    return future;
}

void LlamaInterface::create_chat_completion_async_callback(const ChatCompletionRequest& request, ChatCompletionCallback callback) {
    // Convert to async request
    auto future = create_chat_completion_async(request);
    std::string model = request.model; // Capture model for error handling

    // Start a thread to wait for the result and call the callback
    std::thread([future = std::move(future), callback, model]() mutable {
        try {
            auto response = future.get();
            if (callback) {
                callback(response);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error in async callback: " << e.what() << std::endl;
            if (callback) {
                ChatCompletionResponse error_response;
                error_response.id = "callback_error_" + std::to_string(std::time(nullptr));
                error_response.object = "chat.completion";
                error_response.created = std::time(nullptr);
                error_response.model = model;

                ChatCompletionResponse::ChatChoice choice;
                choice.index = 0;
                choice.message.role = MessageRole::Assistant;
                choice.message.content = "Error: " + std::string(e.what());
                choice.finish_reason = "error";
                error_response.choices.push_back(choice);

                callback(error_response);
            }
        }
    }).detach();
}

bool LlamaInterface::parse_streaming_response(const std::string& response, StreamCallback callback) {
    // Parse Server-Sent Events (SSE) format
    std::istringstream stream(response);
    std::string line;
    line.reserve(256); // OPTIMIZATION: Pre-allocate buffer for lines

    while (std::getline(stream, line)) {
        // Remove carriage return if present
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Skip empty lines
        if (line.empty()) continue;

        // Check for data prefix
        if (line.rfind("data: ", 0) == 0) {
            std::string data = line.substr(6);

            // Check for [DONE] marker
            if (data == "[DONE]") {
                if (callback) callback("", true);
                return true;
            }

            // Parse JSON chunk
            try {
                json chunk = json::parse(data);
                if (callback) callback(chunk.dump(), false);
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse streaming chunk: " << e.what() << std::endl;
            }
        }
        
        // Clear line content for reuse instead of creating new string
        line.clear();
    }

    return true;
}

EmbeddingResponse LlamaInterface::create_embedding(const EmbeddingRequest& request) {
    EmbeddingResponse response;
    response.object = "list";

    try {
        std::string endpoint = server_url_ + "/v1/embeddings";

        json request_json;
        request_json["model"] = request.model;
        request_json["input"] = request.input;

        std::string response_str = make_http_request(endpoint, "POST", request_json);
        json response_json = json::parse(response_str);

        if (response_json.contains("data") && response_json["data"].is_array()) {
            for (size_t i = 0; i < response_json["data"].size(); i++) {
                EmbeddingResponse::EmbeddingData data;
                data.index = response_json["data"][i].value("index", static_cast<int>(i));
                data.embedding = response_json["data"][i]["embedding"].get<std::vector<float>>();
                response.data.push_back(data);
            }
        }

        if (response_json.contains("usage")) {
            response.usage = response_json["usage"];
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to create embedding: " << e.what() << std::endl;
    }

    return response;
}

} // namespace core
} // namespace llama_gui
