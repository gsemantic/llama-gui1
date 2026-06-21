#include "../include/ui/main_window.h"
#include "../include/core/logger.h"
#include "../include/ui/localization_manager.h"
#include "../external/imgui/imgui.h"
#include <iostream>
#include <fstream>

namespace llama_gui {
namespace ui {

void MainWindow::renderDeveloperTools() {
    if (show_metrics_window_) {
        ImGui::ShowMetricsWindow(&show_metrics_window_);
    }

    if (show_style_editor_window_) {
        ImGui::ShowStyleEditor();
        if (ImGui::Button("Close Style Editor")) {
            show_style_editor_window_ = false;
        }
    }

    if (show_debug_log_window_) {
        ImGui::ShowDebugLogWindow(&show_debug_log_window_);
    }

    if (show_font_selector_window_) {
        ImGui::ShowFontSelector("Font Selector");
    }

    if (show_command_manager_window_) {
        renderCommandManagerWindow();
    }

    if (show_window_manager_window_) {
        renderWindowManagerWindow();
    }

    if (show_logger_info_window_) {
        renderLoggerInfoWindow();
    }
}

void MainWindow::renderCommandManagerWindow() {
    ImGui::Begin(TRF("developer.command_manager", "Command Manager State"), &show_command_manager_window_);

    auto stats = command_manager_.getStatistics();

    ImGui::Text("Statistics:");
    ImGui::Separator();
    ImGui::Text("  Total Commands: %zu", stats.total_commands);
    ImGui::Text("  Total Shortcuts: %zu", stats.total_shortcuts);
    ImGui::Text("  History Size: %zu", stats.history_size);
    ImGui::Text("  Can Undo: %s", command_manager_.canUndo() ? "Yes" : "No");

    ImGui::Separator();
    ImGui::Text("Registered Commands:");

    auto commands = command_manager_.getAllCommandNames();
    if (ImGui::BeginListBox("##CommandsList", ImVec2(-FLT_MIN, 200))) {
        for (const auto& cmd : commands) {
            ImGui::Selectable(cmd.c_str());
        }
        ImGui::EndListBox();
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Shortcuts:");

    auto shortcuts = command_manager_.getAvailableShortcuts();
    if (ImGui::BeginListBox("##ShortcutsList", ImVec2(-FLT_MIN, 100))) {
        for (const auto& shortcut : shortcuts) {
            auto cmd_name = command_manager_.getCommandForShortcut(shortcut);
            ImGui::Text("%s -> %s", shortcut.c_str(), cmd_name.c_str());
        }
        ImGui::EndListBox();
    }

    ImGui::End();
}

void MainWindow::renderWindowManagerWindow() {
    ImGui::Begin(TRF("developer.window_manager", "Window Manager State"), &show_window_manager_window_);

    auto window_names = window_manager_.getWindowNames();
    auto window_states = window_manager_.getAllWindowStates();

    ImGui::Text("Windows (%zu):", window_names.size());
    ImGui::Separator();

    if (ImGui::BeginTable("WindowsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Visible");
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Size");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < window_names.size() && i < window_states.size(); ++i) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", window_names[i].c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", window_states[i].visible ? "Yes" : "No");

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("(%.0f, %.0f)", window_states[i].position.x, window_states[i].position.y);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.0fx%.0f", window_states[i].size.x, window_states[i].size.y);
        }
        ImGui::EndTable();
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Quick Actions:");

    if (ImGui::Button("Reset All Windows")) {
        for (const auto& name : window_names) {
            window_manager_.setWindowVisible(name, true);
        }
    }

    ImGui::End();
}

void MainWindow::renderLoggerInfoWindow() {
    ImGui::Begin(TRF("developer.logger_info", "Logger Info"), &show_logger_info_window_);

    auto& logger = llama_gui::core::Logger::instance();

    ImGui::Text("Logger Status:");
    ImGui::Separator();

    const char* level_names[] = {"None", "Error", "Warning", "Info", "Debug"};
    int current_level = static_cast<int>(logger.get_level());

    ImGui::Text("Current Level: %s", level_names[current_level]);
    ImGui::Text("Debug Mode: %s", logger.is_debug_mode() ? "Enabled" : "Disabled");

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Change Log Level:");

    if (ImGui::Button("Error Only")) {
        logger.set_level(llama_gui::core::Logger::Level::Error);
    }
    ImGui::SameLine();
    if (ImGui::Button("Warnings+")) {
        logger.set_level(llama_gui::core::Logger::Level::Warning);
    }
    ImGui::SameLine();
    if (ImGui::Button("Info+")) {
        logger.set_level(llama_gui::core::Logger::Level::Info);
    }
    ImGui::SameLine();
    if (ImGui::Button("Debug")) {
        logger.set_level(llama_gui::core::Logger::Level::Debug);
    }

    ImGui::Separator();
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Log Level Descriptions:");
    ImGui::BulletText("None: No logging");
    ImGui::BulletText("Error: Only errors");
    ImGui::BulletText("Warning: Errors + warnings");
    ImGui::BulletText("Info: Errors + warnings + info");
    ImGui::BulletText("Debug: All logs including debug");

    ImGui::End();
}

void MainWindow::exportDebugInfo() {
    std::ofstream file("llama_gui_debug_info.txt");
    if (!file.is_open()) {
        show_error("Export Debug Info", "Failed to create debug info file");
        return;
    }

    file << "=== Llama GUI Debug Information ===" << std::endl;
    file << "Export Time: " << __DATE__ << " " << __TIME__ << std::endl;
    file << std::endl;

    file << "--- Application State ---" << std::endl;
    file << "Window Size: " << width_ << "x" << height_ << std::endl;
    file << "Window Title: " << title_ << std::endl;
    file << "Developer Mode: " << (developer_mode_enabled_ ? "Enabled" : "Disabled") << std::endl;
    file << "Debug Mode: " << (settings_.performance().debug_mode ? "Enabled" : "Disabled") << std::endl;
    file << std::endl;

    file << "--- Window States ---" << std::endl;
    auto window_states = window_manager_.getAllWindowStates();
    for (const auto& state : window_states) {
        file << "  " << state.name << ": "
             << "visible=" << (state.visible ? "true" : "false") << ", "
             << "pos=(" << state.position.x << ", " << state.position.y << "), "
             << "size=(" << state.size.x << ", " << state.size.y << ")" << std::endl;
    }
    file << std::endl;

    file << "--- Command Manager Statistics ---" << std::endl;
    auto cmd_stats = command_manager_.getStatistics();
    file << "  Total Commands: " << cmd_stats.total_commands << std::endl;
    file << "  Total Shortcuts: " << cmd_stats.total_shortcuts << std::endl;
    file << "  History Size: " << cmd_stats.history_size << std::endl;
    file << "  Can Undo: " << (command_manager_.canUndo() ? "Yes" : "No") << std::endl;
    file << std::endl;

    file << "--- Registered Commands ---" << std::endl;
    auto commands = command_manager_.getAllCommandNames();
    for (const auto& cmd : commands) {
        file << "  " << cmd << std::endl;
    }
    file << std::endl;

    file << "--- Logger Information ---" << std::endl;
    auto& logger = llama_gui::core::Logger::instance();
    const char* level_names[] = {"None", "Error", "Warning", "Info", "Debug"};
    int current_level = static_cast<int>(logger.get_level());
    file << "  Log Level: " << level_names[current_level] << std::endl;
    file << "  Debug Mode: " << (logger.is_debug_mode() ? "Enabled" : "Disabled") << std::endl;
    file << std::endl;

    file << "--- Model Information ---" << std::endl;
    if (model_manager_) {
        file << "  Current Model: " << model_manager_->get_current_model() << std::endl;
        file << "  Model Loaded: " << (model_manager_->is_model_loaded() ? "Yes" : "No") << std::endl;
    } else {
        file << "  Model Manager: Not initialized" << std::endl;
    }
    file << std::endl;

    file << "--- Server Information ---" << std::endl;
    if (server_manager_) {
        file << "  Server Status: " << server_manager_->get_server_status() << std::endl;
    } else {
        file << "  Server Manager: Not initialized" << std::endl;
    }
    file << std::endl;

    file << "--- Dear ImGui Information ---" << std::endl;
    file << "  Version: " << IMGUI_VERSION << std::endl;
    file << "  Demo Window: " << (show_demo_window_ ? "Open" : "Closed") << std::endl;
    file << "  Metrics Window: " << (show_metrics_window_ ? "Open" : "Closed") << std::endl;
    file << "  Style Editor: " << (show_style_editor_window_ ? "Open" : "Closed") << std::endl;
    file << std::endl;

    file << "=== End of Debug Information ===" << std::endl;
    file.close();

    show_info("Export Debug Info", "Debug information exported to llama_gui_debug_info.txt");
}

} // namespace ui
} // namespace llama_gui
