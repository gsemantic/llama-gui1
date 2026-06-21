#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>

namespace llama_gui {
namespace core {

/**
 * Logger with two modes: User (minimal) and Debug (verbose)
 * Supports file logging for crash diagnosis
 */
class Logger {
public:
    enum class Level {
        None = 0,    // No logging
        Error = 1,   // Only errors
        Warning = 2, // Errors + warnings
        Info = 3,    // Errors + warnings + info
        Debug = 4    // All logs including debug
    };

    static Logger& instance() {
        static Logger logger;
        return logger;
    }

    void set_level(Level level) {
        level_ = level;
    }

    Level get_level() const {
        return level_;
    }

    void set_debug_mode(bool debug) {
        level_ = debug ? Level::Debug : Level::Warning;
    }

    bool is_debug_mode() const {
        return level_ >= Level::Debug;
    }

    // === FILE LOGGING ===
    void enable_file_logging(const std::string& filepath) {
        // БЕЗ mutex чтобы избежать deadlock при инициализации
        file_stream_.open(filepath, std::ios::out | std::ios::app);
        if (file_stream_.is_open()) {
            file_logging_enabled_ = true;
            file_path_ = filepath;
            std::cerr << "[Logger] File logging enabled: " << filepath << std::endl;
            log_to_file("[LOGGER] File logging started, path: " + filepath);
        } else {
            std::cerr << "[Logger] Failed to open log file: " << filepath << std::endl;
        }
    }

    void disable_file_logging() {
        if (file_stream_.is_open()) {
            file_stream_.close();
        }
        file_logging_enabled_ = false;
        file_path_.clear();
    }

    void flush_file_log() {
        if (file_stream_.is_open()) {
            file_stream_.flush();
        }
    }
    // ====================

    // Error logging (always enabled if level >= Error)
    void error(const std::string& message) {
        if (level_ >= Level::Error) {
            std::cerr << "❌ ERROR: " << message << std::endl;
            log_to_file("ERROR: " + message);
        }
    }

    template<typename... Args>
    void error(const std::string& format, Args... args) {
        if (level_ >= Level::Error) {
            std::cerr << "❌ ERROR: " << format << std::endl;
            log_to_file("ERROR: " + format);
        }
    }

    // Warning logging
    void warning(const std::string& message) {
        if (level_ >= Level::Warning) {
            std::cerr << "⚠️  WARNING: " << message << std::endl;
            log_to_file("WARNING: " + message);
        }
    }

    // Info logging
    void info(const std::string& message) {
        if (level_ >= Level::Info) {
            std::cout << "ℹ️  INFO: " << message << std::endl;
            log_to_file("INFO: " + message);
        }
    }

    // Debug logging (only in debug mode)
    void debug(const std::string& message) {
        if (level_ >= Level::Debug) {
            std::cout << "🔧 DEBUG: " << message << std::endl;
            log_to_file("DEBUG: " + message);
        }
    }

    template<typename T>
    void debug(const std::string& prefix, const T& value) {
        if (level_ >= Level::Debug) {
            std::cout << "🔧 DEBUG: " << prefix << " = " << value << std::endl;
            log_to_file("DEBUG: " + prefix + " = " + std::to_string(value));
        }
    }

    // Stream-like logging
    class LogStream {
    public:
        LogStream(Level level, const std::string& prefix)
            : level_(level), prefix_(prefix), enabled_(level <= Logger::instance().level_) {
            if (enabled_) {
                stream_ << prefix << " ";
            }
        }

        template<typename T>
        LogStream& operator<<(const T& value) {
            if (enabled_) {
                stream_ << value;
            }
            return *this;
        }

        ~LogStream() {
            if (enabled_) {
                std::string msg = stream_.str();
                if (level_ == Level::Error) {
                    std::cerr << msg << std::endl;
                } else {
                    std::cout << msg << std::endl;
                }
                Logger::instance().log_to_file(msg);
            }
        }

    private:
        Level level_;
        std::string prefix_;
        bool enabled_;
        std::ostringstream stream_;
    };

    LogStream stream(Level level, const std::string& prefix) {
        return LogStream(level, prefix);
    }

private:
    Logger() : level_(Level::Warning), file_logging_enabled_(false) {}
    
    void log_to_file(const std::string& message) {
        if (!file_logging_enabled_ || !file_stream_.is_open()) return;
        
        // Добавляем timestamp
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        file_stream_ << std::put_time(std::localtime(&time), "%H:%M:%S")
                     << "." << std::setfill('0') << std::setw(3) << ms.count()
                     << " " << message << std::endl;
    }
    
    Level level_;
    bool file_logging_enabled_;
    std::string file_path_;
    std::ofstream file_stream_;
    std::mutex file_mutex_;
};

// Convenience macros
#define LOG_ERROR(msg) llama_gui::core::Logger::instance().error(msg)
#define LOG_WARNING(msg) llama_gui::core::Logger::instance().warning(msg)
#define LOG_INFO(msg) llama_gui::core::Logger::instance().info(msg)
#define LOG_DEBUG(msg) llama_gui::core::Logger::instance().debug(msg)

#define LOG_DEBUG_VAR(var) llama_gui::core::Logger::instance().debug(#var, var)

} // namespace core
} // namespace llama_gui
