#include "../include/core/llama_interface.h"
#include "../include/core/llama_interface_impl.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <condition_variable>

namespace llama_gui {
namespace core {

void LlamaInterface::make_async_http_request(const std::string& endpoint, const std::string& method, const json& data, HttpResponseCallback callback) {
    std::thread([this, endpoint, method, data, callback]() {
        try {
            std::string response = pImpl->make_http_request_async(endpoint, method, data, timeout_seconds_);

            bool request_complete = false;
            auto start_time = std::chrono::steady_clock::now();
            constexpr int INITIAL_SLEEP_MS = 5;
            constexpr int MAX_SLEEP_MS = 50;
            int current_sleep_ms = INITIAL_SLEEP_MS;

            while (!request_complete) {
                pImpl->process_async_requests();

                {
                    std::lock_guard<std::mutex> lock(pImpl->responses_mutex_);
                    if (pImpl->pending_responses_.empty()) {
                        request_complete = true;
                        break;
                    }
                }

                // Экспоненциальная задержка для снижения CPU usage
                std::this_thread::sleep_for(std::chrono::milliseconds(current_sleep_ms));
                current_sleep_ms = std::min(current_sleep_ms * 2, MAX_SLEEP_MS);

                auto current_time = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
                if (elapsed >= timeout_seconds_) {
                    std::cerr << "Async HTTP request timed out after " << timeout_seconds_ << " seconds" << std::endl;
                    if (callback) {
                        callback("");
                    }
                    return;
                }
            }

            if (callback) {
                callback(response);
            }
        } catch (const std::exception& e) {
            std::cerr << "Async HTTP request failed: " << e.what() << std::endl;
            if (callback) {
                callback("");
            }
        }
    }).detach();
}

} // namespace core
} // namespace llama_gui
