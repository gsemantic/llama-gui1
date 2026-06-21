#include "../include/core/llama_interface.h"
#include "../include/core/llama_interface_impl.h"
#include <iostream>

namespace llama_gui {
namespace core {

LlamaInterface::LlamaInterface(const std::string& server_url)
    : pImpl(std::make_unique<Impl>(server_url))
    , server_url_(server_url)
    , timeout_seconds_(300)
    , last_error_("") {}

LlamaInterface::~LlamaInterface() = default;

bool LlamaInterface::initialize(const std::string& server_url) {
    server_url_ = server_url;

    std::cout << "LlamaInterface: Initializing connection to " << server_url << std::endl;

    // Test connection by getting server info
    try {
        json server_info = get_server_info();
        if (!server_info.empty() && server_info.contains("data") && !server_info["data"].empty()) {
            std::cout << "✓ Connected successfully to llama.cpp server" << std::endl;
            return true;
        }
    } catch (const std::exception& e) {
        std::cerr << "Failed to connect to server: " << e.what() << std::endl;
    }

    // Try to connect to localhost:8081 as fallback
    if (server_url != "http://localhost:8081") {
        std::cout << "⚠ Trying fallback connection to localhost:8081" << std::endl;
        try {
            server_url_ = "http://localhost:8081";
            json server_info = get_server_info();
            if (!server_info.empty() && server_info.contains("data") && !server_info["data"].empty()) {
                std::cout << "✓ Connected successfully to llama.cpp server on localhost:8081" << std::endl;
                return true;
            }
        } catch (const std::exception& e) {
            std::cerr << "Failed to connect to localhost:8081: " << e.what() << std::endl;
        }
    }

    std::cerr << "✗ Failed to connect to llama.cpp server" << std::endl;
    return false;
}

void LlamaInterface::set_api_key(const std::string& api_key) {
    api_key_ = api_key;
}

void LlamaInterface::set_timeout(int seconds) {
    timeout_seconds_ = seconds;
}

} // namespace core
} // namespace llama_gui
