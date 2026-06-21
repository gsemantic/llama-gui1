#include "../include/core/server_manager.h"
#include "../include/core/settings.h"
#include "../include/core/state_manager.h"
#include <iostream>
#include <sstream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <system_error>
#include <filesystem>
#include <atomic>
#include <mutex>
#include <queue>
#include <functional>

namespace llama_gui {
namespace core {

// =========================================================================
// Thread-safe settings update implementation
// =========================================================================

void ServerManager::queue_settings_update(std::function<void(Settings&)> updater) {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    pending_settings_ = std::move(updater);
    settings_changed_ = true;
    std::cout << "ServerManager: Settings update queued for next restart" << std::endl;
}

bool ServerManager::has_pending_settings() const {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    return pending_settings_.has_value();
}

void ServerManager::apply_pending_settings() {
    std::lock_guard<std::mutex> lock(settings_mutex_);
    if (pending_settings_.has_value()) {
        (*pending_settings_)(settings_);
        pending_settings_.reset();
        settings_changed_ = false;
        std::cout << "ServerManager: Pending settings applied" << std::endl;
    }
}

bool ServerManager::restart_with_settings(std::function<void(Settings&)> new_settings) {
    // Queue the settings update
    queue_settings_update(std::move(new_settings));
    
    // Restart the server
    return restart_server();
}

// =========================================================================
// Refactored build_server_command() using categorized builders
// =========================================================================

std::string ServerManager::build_server_command() const {
    std::ostringstream cmd;

    // Server binary path
    std::string server_cmd = "/home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-b7472/llama-server";
    cmd << server_cmd;

    // Category-specific arguments
    cmd << build_model_args();
    cmd << build_gpu_args();
    cmd << build_batch_args();
    cmd << build_sampling_args();
    cmd << build_cache_args();
    cmd << build_rope_args();
    cmd << build_server_args();
    cmd << build_security_args();
    cmd << build_advanced_args();
    cmd << build_logging_args();
    cmd << build_grammar_args();
    cmd << build_output_args();

    std::cout << "ServerManager: Built server command with " << cmd.str().length() << " characters" << std::endl;
    return cmd.str();
}
bool ServerManager::restart_server() {
    // Blocking stop to ensure port is free before starting
    std::cout << "ServerManager: === RESTART SERVER ===" << std::endl;
    
    if (stop_server(true)) {
        // Wait for port and process to be fully released (up to 5 seconds)
        std::cout << "ServerManager: Waiting for port " << server_port_ << " and process to be released..." << std::endl;

        int max_attempts = 50;
        int attempt = 0;
        while (attempt < max_attempts) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            // Check if port is still in use
            std::string check_port_cmd = "fuser " + std::to_string(server_port_) + "/tcp 2>/dev/null";
            FILE* port_pipe = popen(check_port_cmd.c_str(), "r");
            bool port_busy = false;
            if (port_pipe) {
                char buffer[256];
                if (fgets(buffer, sizeof(buffer), port_pipe)) {
                    port_busy = true;
                }
                pclose(port_pipe);
            }

            // Check if llama-server process is still running
            std::string check_proc_cmd = "pgrep -f 'llama-server.*--port.*" + std::to_string(server_port_) + "' 2>/dev/null";
            FILE* proc_pipe = popen(check_proc_cmd.c_str(), "r");
            bool process_running = false;
            if (proc_pipe) {
                char buffer[256];
                if (fgets(buffer, sizeof(buffer), proc_pipe)) {
                    process_running = true;
                    std::cout << "ServerManager: Process still running (PID: " << buffer << ")" << std::endl;
                }
                pclose(proc_pipe);
            }

            // If port is free AND process is gone, we're good
            if (!port_busy && !process_running) {
                std::cout << "ServerManager: Port " << server_port_ << " is free after "
                          << (attempt + 1) * 100 << "ms" << std::endl;
                break;
            }

            attempt++;
        }

        if (attempt >= max_attempts) {
            std::cerr << "ServerManager: WARNING: Port " << server_port_
                      << " still in use after " << max_attempts * 100 << "ms, forcing restart..." << std::endl;
            // Force kill one more time
            kill_server_process(true);
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // Update model path from settings before restarting
        model_path_ = settings_.get_model_path();
        std::cout << "ServerManager: Model path from settings: " << model_path_ << std::endl;
        std::cout << "ServerManager: Starting server restart..." << std::endl;
        return start_server();
    } else {
        std::cerr << "ServerManager: Failed to stop server before restart" << std::endl;
        return false;
    }
}

bool ServerManager::is_server_running() const {
    return server_running_;
}

bool ServerManager::is_server_ready() const {
    // Проверяем, отвечает ли сервер на запросы
    // Для простоты - проверяем через server_running_ и server_status_
    return server_running_ && server_status_ == "ready";
}

std::string ServerManager::get_server_status() const {
    return server_status_;
}

std::string ServerManager::get_server_output() const {
    return server_output_;
}

void ServerManager::set_model_path(const std::string& model_path) {
    model_path_ = model_path;
}

void ServerManager::set_server_url(const std::string& url) {
    // Parse URL to extract host and port
    size_t colon_pos = url.find_last_of(':');
    if (colon_pos != std::string::npos) {
        server_host_ = url.substr(0, colon_pos);
        try {
            server_port_ = std::stoi(url.substr(colon_pos + 1));
        } catch (const std::exception&) {
            server_port_ = 8080; // Default port
        }
    } else {
        server_host_ = url;
        server_port_ = 8080; // Default port
    }
}

void ServerManager::set_host_port(const std::string& host, int port) {
    server_host_ = host;
    server_port_ = port;
}

void ServerManager::set_status_callback(StatusCallback callback) {
    status_callback_ = callback;
}

void ServerManager::server_thread_function() {
    // This would normally run the server process
    // For now, just simulate it with better CPU efficiency
    while (server_running_) {
        // Use longer sleep to reduce CPU usage
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

bool ServerManager::kill_server_process(bool blocking) {
    auto kill_task = [this]() {
        // Check shutdown flag at start of task
        // Note: We might still want to kill processes even if shutting down,
        // but we shouldn't access member variables indiscriminately

        // Kill any existing llama-server processes
        std::cout << "ServerManager: Killing existing server processes..." << std::endl;

        // Step 1: Try SIGTERM first (graceful shutdown)
        std::string kill_command = "pkill -SIGTERM -f llama-server";
        int kill_result = system(kill_command.c_str());
        std::cout << "ServerManager: pkill SIGTERM result: " << kill_result << std::endl;

        // Step 2: Wait briefly for graceful shutdown
        std::this_thread::sleep_for(std::chrono::milliseconds(300));

        // Step 3: Check if process is still running and force kill
        std::string check_cmd = "pgrep -f llama-server";
        FILE* pipe = popen(check_cmd.c_str(), "r");
        if (pipe) {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe)) {
                // Process still running, force kill with SIGKILL
                std::cout << "ServerManager: Process still running, sending SIGKILL..." << std::endl;
                std::string force_kill = "pkill -SIGKILL -f llama-server";
                int force_result = system(force_kill.c_str());
                std::cout << "ServerManager: pkill SIGKILL result: " << force_result << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            pclose(pipe);
        }

        // Step 4: Kill by port as fallback
        std::string kill_port_command = "fuser -k " + std::to_string(server_port_) + "/tcp 2>/dev/null";
        int kill_port_result = system(kill_port_command.c_str());
        if (kill_port_result == 0) {
            std::cout << "ServerManager: Killed processes using port " << server_port_ << std::endl;
        }

        // Step 5: Wait for port to be released
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    };

    if (blocking) {
        kill_task();
    } else {
        // Use thread execution to avoid blocking UI
        auto self = shared_from_this();
        std::thread worker_thread([kill_task, self]() {
            kill_task();
        });
        // Detach the thread to let it run in background
        worker_thread.detach();
    }

    // Return true immediately
    return true;
}

std::string ServerManager::check_http_status(const std::string& url) const {
    std::string curl_command = "curl -s -o /dev/null -w \"%{http_code}\" --connect-timeout 1 --max-time 2 " + url + " 2>/dev/null";
    
    FILE* pipe = popen(curl_command.c_str(), "r");
    std::string http_status = "";
    if (pipe) {
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            http_status += buffer;
        }
        pclose(pipe);
    }

    // Clean up the status code - remove ALL non-digit characters
    std::string clean_status;
    for (char c : http_status) {
        if (c >= '0' && c <= '9') {
            clean_status += c;
        }
    }
    
    return clean_status.empty() ? "000" : clean_status;
}


} // namespace core
} // namespace llama_gui
