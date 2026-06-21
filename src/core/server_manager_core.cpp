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
ServerManager::ServerManager(Settings& settings)
    : settings_(settings),
      server_running_(false),
      shutting_down_(false), // Initialize shutdown flag
      server_port_(8081) {  // Changed from 8080 to 8081
    // Initialize with default values
    model_path_ = settings.get_model_path();
    server_host_ = "localhost";
}

ServerManager::~ServerManager() {
    shutting_down_ = true; // Signal that we are shutting down
    
    // Clear callback to prevent calling it during destruction
    status_callback_ = nullptr;
    
    // Stop server synchronously
    stop_server(true);
}

bool ServerManager::start_server() {
    // Update status immediately to show we're working on it
    server_status_ = "Starting server...";
    if (status_callback_) {
        status_callback_(server_status_, false);
    }

    // Use thread execution to avoid blocking UI
    // Use shared_from_this() to ensure ServerManager stays alive during thread execution
    auto self = shared_from_this();
    std::thread worker_thread([this, self]() {
        // First, check if server is already running by trying to connect
        std::cout << "ServerManager: Checking if server is already running..." << std::endl;

        // Try to connect to the server first
        std::string test_url = "http://" + server_host_ + ":" + std::to_string(server_port_) + "/props";
        std::cout << "Testing connection to: " << test_url << std::endl;

        // Simple curl test to check if server is responsive
        std::string curl_command = "curl -s -o /dev/null -w \"%{http_code}\" " + test_url;

        // Use popen to get the actual HTTP status code from curl stdout
        FILE* pipe = popen(curl_command.c_str(), "r");
        std::string http_status = "";
        if (pipe) {
            char buffer[128];
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
                http_status += buffer;
            }
            pclose(pipe);
        }

        // Clean up the status code (remove whitespace)
        http_status.erase(0, http_status.find_first_not_of(" \t\n\r\f\v"));
        http_status.erase(http_status.find_last_not_of(" \t\n\r\f\v") + 1);

        std::cout << "ServerManager: Initial curl HTTP status: '" << http_status << "'" << std::endl;

        bool success = false;
        if (http_status == "200") {
            std::cout << "ServerManager: Server is already running, no need to start" << std::endl;
            success = true;
        } else {
            std::cout << "ServerManager: Server not running (HTTP status: '" << http_status << "'), attempting to start..." << std::endl;
            // Build the actual server command with current model path
            std::string command = build_server_command();
            std::cout << "Starting server with command: " << command << std::endl;

            // Test the command directly to see if it's valid
            std::cout << "ServerManager: Testing server command..." << std::endl;
            std::string test_cmd = command + " --help 2>&1 | head -5";
            FILE* test_pipe = popen(test_cmd.c_str(), "r");
            if (test_pipe) {
                char test_buffer[256];
                std::string test_output = "";
                while (fgets(test_buffer, sizeof(test_buffer), test_pipe) != nullptr) {
                    test_output += test_buffer;
                }
                pclose(test_pipe);
                
                if (!test_output.empty()) {
                    std::cout << "ServerManager: Command test output: " << test_output.substr(0, 200) << std::endl;
                }
            }

            // Note: We don't kill existing processes here because restart_server() 
            // already calls stop_server() before start_server(). Killing processes 
            // here would interfere with the server we just started.

            // Run the server in the background by appending '&' to the command
            std::string background_command = command + " &";

            // Try to actually execute the server command in the background
            std::cout << "ServerManager: Executing command: " << background_command << std::endl;
            
            // Use a more robust approach to start the server
            // First, let's try to execute the command directly and capture any immediate errors
            FILE* test_exec_pipe = popen(("exec " + command + " 2>&1").c_str(), "r");
            if (test_exec_pipe) {
                char test_buffer[512];
                std::string immediate_output = "";
                while (fgets(test_buffer, sizeof(test_buffer), test_exec_pipe) != nullptr) {
                    immediate_output += test_buffer;
                }
                pclose(test_exec_pipe);
                
                if (!immediate_output.empty()) {
                    std::cout << "ServerManager: Command immediate output: " << immediate_output << std::endl;
                }
            }
            
            int result = system(background_command.c_str());
            std::cout << "ServerManager: system() returned: " << result << std::endl;
            success = (result == 0);
            
            // Additional check: verify if llama-server process is actually running
            std::string check_process = "pgrep -f llama-server";
            FILE* proc_pipe = popen(check_process.c_str(), "r");
            if (proc_pipe) {
                char proc_buffer[128];
                std::string proc_result = "";
                while (fgets(proc_buffer, sizeof(proc_buffer), proc_pipe) != nullptr) {
                    proc_result += proc_buffer;
                }
                pclose(proc_pipe);
                
                if (!proc_result.empty()) {
                    std::cout << "ServerManager: Found llama-server processes: " << proc_result;
                } else {
                    std::cout << "ServerManager: No llama-server processes found after start attempt" << std::endl;
                }
            }

            // If server started successfully, update status and wait asynchronously
            if (success) {
                server_running_ = true;
                server_status_ = "Server started successfully with model: " + model_path_;
                server_output_ = "Llama.cpp server is running on " + server_host_ + ":" + std::to_string(server_port_) +
                                " with model: " + model_path_;
                std::cout << "Server started successfully in background" << std::endl;

                // Асинхронно ждем готовности сервера (не блокируем основной поток)
                std::thread([this]() {
                    int max_attempts = 600; // Wait up to 60 seconds (10 minutes) for large models
                    for (int i = 0; i < max_attempts; i++) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));

                        // Проверяем доступность сервера
                        std::string props_url = "http://" + server_host_ + ":" + std::to_string(server_port_) + "/props";
                        std::string status = check_http_status(props_url);
                        if (status == "200") {
                            server_status_ = "ready";
                            std::cout << "ServerManager: Server is ready!" << std::endl;
                            break;
                        } else if (i % 10 == 0) {
                            // Log every 1 second
                            std::cout << "ServerManager: Waiting for server... (" << (i+1) << "/" << max_attempts << ") Status: " << status << std::endl;
                        }
                    }
                    if (server_status_ != "ready") {
                        std::cerr << "ServerManager: Server failed to become ready within " << (max_attempts * 100) << " ms" << std::endl;
                        server_status_ = "Server not responding";
                    }
                    if (status_callback_) {
                        status_callback_(server_status_, server_status_ == "ready");
                    }
                }).detach();
            } else {
                server_running_ = false;
                server_status_ = "Failed to start server with model: " + model_path_;
                server_output_ = "Error starting llama.cpp server";
                std::cerr << "Failed to start server" << std::endl;
            }

            if (status_callback_) {
                status_callback_(server_status_, success);
            }
        }
    });

    // Detach the thread to let it run in background
    worker_thread.detach();

    // Return true immediately to avoid blocking UI
    return true;
}

bool ServerManager::stop_server(bool blocking) {
    if (!server_running_) {
        return true; // Already stopped
    }

    // Update status immediately
    server_status_ = "Stopping server...";
    if (status_callback_) {
        status_callback_(server_status_, false);
    }

    auto cleanup_task = [this]() {
        // Check shutdown flag at start of task
        if (shutting_down_) return;

        // Actually kill the server process
        std::cout << "ServerManager: Stopping server..." << std::endl;
        kill_server_process(true);

        // Update status
        server_running_ = false;

        // Only update member strings if not shutting down to avoid memory issues
        if (!shutting_down_) {
            server_status_ = "Server stopped";
            server_output_ = "Llama.cpp server has been stopped";

            if (status_callback_) {
                status_callback_(server_status_, false);
            }
        }

        std::cout << "ServerManager: Server stopped successfully" << std::endl;
    };

    if (blocking) {
        cleanup_task();
    } else {
        // Use thread execution to avoid blocking UI
        auto self = shared_from_this();
        std::thread worker_thread([cleanup_task, self]() {
            cleanup_task();
        });
        // Detach the thread to let it run in background
        worker_thread.detach();
    }

    // Return true immediately
    return true;
}

} // namespace core
} // namespace llama_gui
