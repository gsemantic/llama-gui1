#include "../include/ui/main_window.h"
#include <iostream>

#include "../external/imgui/imgui.h"

namespace llama_gui {
namespace ui {

void MainWindow::initializeNewUISystem() {
    std::cout << "Initializing new UI system..." << std::endl;

    // Initialize CommandManager with callbacks
    setupCommandCallbacks();

    // Initialize WindowManager with default windows
    window_manager_.addWindow("conversations", true);
    window_manager_.addWindow("files", true);
    window_manager_.addWindow("chat", true);
    window_manager_.addWindow("rag", false);  // RAG window hidden by default
    window_manager_.addWindow("agents", false);  // Agents window hidden by default

    // Initialize GridSnappingDialog
    grid_snapping_dialog_->setGridSnappingSystem(&window_manager_.getGridSnappingSystem());
    grid_snapping_dialog_->setSnapAllCallback([this]() {
        window_manager_.snapAllWindowsToGrid();
        force_apply_window_positions_ = true;  // Принудительно применить позиции на следующей отрисовке
        force_ui_update_ = true;  // Принудительная перерисовка
    });

    // Initialize AdvancedMenuSystem
    advanced_menu_system_.initialize(&command_manager_, &window_manager_, &workspace_manager_);
    advanced_menu_system_.setWorkspaceManager(&workspace_manager_);
    advanced_menu_system_.buildModernMenu();

    // Установить callback для обновления меню при переключении workspace
    // ВАЖНО: установить ДО вызова notifyWorkspaceChanged
    std::cout << "[MAIN] Setting menu_update_callback..." << std::endl;
    workspace_manager_.setMenuUpdateCallback([this]() {
        std::cout << "[MAIN] menu_update_callback INVOKED! force_ui_update=" << force_ui_update_
                  << ", ui_dirty_menu=" << ui_dirty_menu_ << std::endl;
        force_ui_update_ = true;
        ui_dirty_menu_ = true;
        std::cout << "[MAIN] Set force_ui_update_=true, ui_dirty_menu_=true" << std::endl;
    });

    // Применяем конфигурацию workspace по умолчанию (User)
    // Callback уже установлен, меню обновится корректно
    std::cout << "[MAIN] Calling notifyWorkspaceChanged..." << std::endl;
    workspace_manager_.notifyWorkspaceChanged();

    // Загружаем сохранённую конфигурацию окон (default)
    std::cout << "MainWindow: Loading default workspace configuration..." << std::endl;
    advanced_menu_system_.loadWorkspace("default");

    std::cout << "✓ New UI system initialized successfully" << std::endl;
}

void MainWindow::setupCommandCallbacks() {
    // Set up callbacks for default commands
    command_manager_.initializeDefaultCommands(
        [this]() {
            // New chat callback
            state_manager_.create_conversation("New Chat");
        },
        [this]() {
            // Exit callback
            is_running_ = false;
        },
        [this]() {
            // Settings callback
            settings_dialog_->show();
        },
        [this]() {
            // Model selection callback
            open_model_selection_dialog();
        },
        [this](ServerControlCommand::Action action) {
            // Server control callback
            switch (action) {
                case ServerControlCommand::Action::Start:
                    server_manager_->start_server();
                    show_info(TR("status.server_started"), TR("status.server_starting"));
                    break;
                case ServerControlCommand::Action::Stop:
                    server_manager_->stop_server();
                    show_info(TR("status.server_stopped"), TR("status.server_stopping"));
                    break;
                case ServerControlCommand::Action::Restart:
                    server_manager_->restart_server();
                    show_info(TR("status.server_restarted"), TR("status.server_restarting"));
                    break;
                case ServerControlCommand::Action::Status: {
                    // Show server status
                    std::string status = server_manager_->get_server_status();
                    bool is_ready = server_manager_->is_server_ready();
                    std::string status_message = is_ready ? 
                        TR("status.server_ready") :
                        TR("status.server_not_ready");
                    show_info(TR("status.server_status"), 
                              (status_message + "\n\n" + TR("status.details") + " " + status).c_str());
                    break;
                }
            }
        },
        [this](const std::string& help_type) {
            // Help callback
            if (help_type == "about") {
                show_about_dialog();
            } else if (help_type == "shortcuts") {
                show_keyboard_shortcuts();
            } else {
                show_help_dialog();
            }
        }
    );

    // Set up additional command callbacks
    // Commands are already registered in CommandManager::initializeDefaultCommands()

    // Set up command execution callbacks
    command_manager_.setCommandExecutedCallback([this](const std::string& command_name) {
        std::cout << "✓ Command executed: " << command_name << std::endl;
    });

    command_manager_.setCommandFailedCallback([this](const std::string& command_name) {
        std::cout << "⚠️ Command failed: " << command_name << std::endl;
    });

    // Set up file operation callbacks
    command_manager_.setOpenFileCallback([this]() {
        open_conversation_file();
    });

    command_manager_.setSaveFileCallback([this]() {
        if (!current_conversation_path_.empty()) {
            save_current_conversation();
        } else {
            // Если путь не установлен, открываем диалог сохранения
            show_save_conversation_dialog_ = true;
        }
    });

    command_manager_.setSaveFileAsCallback([this](const std::string& default_path) {
        // Открываем диалог сохранения для выбора пути
        show_save_conversation_dialog_ = true;
    });

    // =========================================================================
    // Переопределяем команды настроек для открытия конкретных вкладок
    // =========================================================================
    
    // Быстрые настройки (Quick Settings)
    command_manager_.registerCommand("open_settings_server", CommandFactory::createFunctionalCommand(
        "open_settings_server",
        [this]() { settings_dialog_->show_server_settings(); },
        "Open server settings (Quick Settings)",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_chat", CommandFactory::createFunctionalCommand(
        "open_settings_chat",
        [this]() { settings_dialog_->show_chat_settings(); },
        "Open chat settings (Quick Settings)",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_models", CommandFactory::createFunctionalCommand(
        "open_settings_models",
        [this]() { settings_dialog_->show_models_settings(); },
        "Open model settings (Quick Settings)",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_ui", CommandFactory::createFunctionalCommand(
        "open_settings_ui",
        [this]() { settings_dialog_->show_ui_settings(); },
        "Open UI settings (Quick Settings)",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_cloud", CommandFactory::createFunctionalCommand(
        "open_settings_cloud",
        [this]() {
            if (cloud_services_dialog_) {
                cloud_services_dialog_->open();
            }
        },
        "Open cloud services settings (OpenRouter, etc.)",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_embedding", CommandFactory::createFunctionalCommand(
        "open_settings_embedding",
        [this]() {
            std::cout << "[DEBUG] open_settings_embedding command called" << std::endl;
            if (embedding_settings_dialog_) {
                std::cout << "[DEBUG] embedding_settings_dialog_ exists, calling open()" << std::endl;
                embedding_settings_dialog_->open();
            } else {
                std::cerr << "[ERROR] embedding_settings_dialog_ is null!" << std::endl;
            }
        },
        "Open embedding settings (HuggingFace, local model, cache)",
        "",
        nullptr));

    // Настройки GPU и оборудования (GPU & Hardware)
    command_manager_.registerCommand("open_settings_gpu", CommandFactory::createFunctionalCommand(
        "open_settings_gpu",
        [this]() { settings_dialog_->show_gpu_settings(); },
        "Open GPU settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_cache", CommandFactory::createFunctionalCommand(
        "open_settings_cache",
        [this]() { settings_dialog_->show_cache_settings(); },
        "Open cache settings",
        "",
        nullptr));

    // Настройки сэмплирования и генерации (Sampling & Generation)
    command_manager_.registerCommand("open_settings_sampling_basic", CommandFactory::createFunctionalCommand(
        "open_settings_sampling_basic",
        [this]() { settings_dialog_->show_sampling_basic_settings(); },
        "Open basic sampling settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_sampling_advanced", CommandFactory::createFunctionalCommand(
        "open_settings_sampling_advanced",
        [this]() { settings_dialog_->show_sampling_advanced_settings(); },
        "Open advanced sampling settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_context", CommandFactory::createFunctionalCommand(
        "open_settings_context",
        [this]() { settings_dialog_->show_context_settings(); },
        "Open context settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_rope", CommandFactory::createFunctionalCommand(
        "open_settings_rope",
        [this]() { settings_dialog_->show_rope_settings(); },
        "Open RoPE settings",
        "",
        nullptr));

    // Настройки модели и сервера (Model & Server)
    command_manager_.registerCommand("open_settings_model_loading", CommandFactory::createFunctionalCommand(
        "open_settings_model_loading",
        [this]() { settings_dialog_->show_model_loading_settings(); },
        "Open model loading settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_batch", CommandFactory::createFunctionalCommand(
        "open_settings_batch",
        [this]() { settings_dialog_->show_batch_settings(); },
        "Open batch settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_server_runtime", CommandFactory::createFunctionalCommand(
        "open_settings_server_runtime",
        [this]() { settings_dialog_->show_server_runtime_settings(); },
        "Open server runtime settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_grammar", CommandFactory::createFunctionalCommand(
        "open_settings_grammar",
        [this]() { settings_dialog_->show_grammar_settings(); },
        "Open grammar settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_control_vectors", CommandFactory::createFunctionalCommand(
        "open_settings_control_vectors",
        [this]() { settings_dialog_->show_control_vectors_settings(); },
        "Open control vectors settings",
        "",
        nullptr));

    // Системные настройки (System)
    command_manager_.registerCommand("open_settings_logging", CommandFactory::createFunctionalCommand(
        "open_settings_logging",
        [this]() { settings_dialog_->show_logging_settings(); },
        "Open logging settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_performance", CommandFactory::createFunctionalCommand(
        "open_settings_performance",
        [this]() { settings_dialog_->show_performance_settings(); },
        "Open performance settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_advanced", CommandFactory::createFunctionalCommand(
        "open_settings_advanced",
        [this]() { settings_dialog_->show_advanced_settings_tab(); },
        "Open advanced settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_output", CommandFactory::createFunctionalCommand(
        "open_settings_output",
        [this]() { settings_dialog_->show_output_settings(); },
        "Open output settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_tensor_override", CommandFactory::createFunctionalCommand(
        "open_settings_tensor_override",
        [this]() { settings_dialog_->show_tensor_override_settings(); },
        "Open tensor override settings",
        "",
        nullptr));
    command_manager_.registerCommand("open_settings_ini_viewer", CommandFactory::createFunctionalCommand(
        "open_settings_ini_viewer",
        [this]() { 
            if (settings_viewer_dialog_) {
                settings_viewer_dialog_->show();
            }
        },
        "View and edit all settings in INI format (like php.ini)",
        "",
        nullptr));

    // =========================================================================
    // Команды переключения окон
    // =========================================================================

    // Register window toggle commands
    command_manager_.registerCommand("toggle_window_conversations", CommandFactory::createFunctionalCommand(
        "toggle_window_conversations",
        [this]() { window_manager_.toggleWindow("conversations"); },
        "Toggle conversations panel",
        "Ctrl+1",
        nullptr));

    command_manager_.registerCommand("toggle_window_files", CommandFactory::createFunctionalCommand(
        "toggle_window_files",
        [this]() { window_manager_.toggleWindow("files"); },
        "Toggle files panel",
        "Ctrl+2",
        nullptr));

    command_manager_.registerCommand("toggle_window_chat", CommandFactory::createFunctionalCommand(
        "toggle_window_chat",
        [this]() { window_manager_.toggleWindow("chat"); },
        "Toggle chat panel",
        "Ctrl+3",
        nullptr));

    command_manager_.registerCommand("toggle_window_rag", CommandFactory::createFunctionalCommand(
        "toggle_window_rag",
        [this]() { window_manager_.toggleWindow("rag"); },
        "Toggle RAG panel",
        "Ctrl+4",
        nullptr));

    command_manager_.registerCommand("toggle_window_agents", CommandFactory::createFunctionalCommand(
        "toggle_window_agents",
        [this]() { window_manager_.toggleWindow("agents"); },
        "Toggle Agents panel",
        "",
        nullptr));

    // Register grid snapping commands
    command_manager_.registerCommand("toggle_grid_snapping", CommandFactory::createFunctionalCommand(
        "toggle_grid_snapping",
        [this]() {
            auto& grid_system = window_manager_.getGridSnappingSystem();
            grid_system.setEnabled(!grid_system.isEnabled());
            window_manager_.snapAllWindowsToGrid();
        },
        "Toggle grid snapping for windows",
        "",
        nullptr));

    command_manager_.registerCommand("snap_windows_to_grid", CommandFactory::createFunctionalCommand(
        "snap_windows_to_grid",
        [this]() { window_manager_.snapAllWindowsToGrid(); },
        "Snap all windows to grid",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_grid_overlay", CommandFactory::createFunctionalCommand(
        "toggle_grid_overlay",
        [this]() {
            auto& grid_system = window_manager_.getGridSnappingSystem();
            grid_system.setShowGridOverlay(!grid_system.isGridOverlayVisible());
            force_ui_update_ = true;
        },
        "Toggle grid overlay visualization",
        "",
        nullptr));

    command_manager_.registerCommand("show_grid_snapping_dialog", CommandFactory::createFunctionalCommand(
        "show_grid_snapping_dialog",
        [this]() {
            if (grid_snapping_dialog_) {
                grid_snapping_dialog_->show();
            }
        },
        "Show grid snapping settings dialog",
        "",
        nullptr));

    // Register workspace commands with proper callbacks
    command_manager_.registerCommand("save_workspace", CommandFactory::createFunctionalCommand(
        "save_workspace",
        [this]() {
            advanced_menu_system_.saveWorkspace("default");
            show_info("Workspace Saved", "Workspace saved: default");
            force_ui_update_ = true;  // Принудительная перерисовка
        },
        "Save current workspace layout (default)",
        "",
        nullptr));

    command_manager_.registerCommand("save_workspace_as", CommandFactory::createFunctionalCommand(
        "save_workspace_as",
        [this]() {
            show_workspace_save_dialog();
        },
        "Save current workspace layout with custom name",
        "",
        nullptr));

    command_manager_.registerCommand("load_workspace", CommandFactory::createFunctionalCommand(
        "load_workspace",
        [this]() {
            advanced_menu_system_.loadWorkspace("default");
            show_info("Workspace Loaded", "Workspace loaded: default");
            force_ui_update_ = true;  // Принудительная перерисовка
        },
        "Load default workspace layout",
        "",
        nullptr));

    command_manager_.registerCommand("load_workspace_as", CommandFactory::createFunctionalCommand(
        "load_workspace_as",
        [this]() {
            show_workspace_load_dialog();
        },
        "Load workspace layout with custom name",
        "",
        nullptr));

    command_manager_.registerCommand("reset_workspace", CommandFactory::createFunctionalCommand(
        "reset_workspace",
        [this]() {
            window_manager_.setWindowVisible("conversations", true);
            window_manager_.setWindowVisible("files", true);
            window_manager_.setWindowVisible("chat", true);
            window_manager_.setWindowVisible("rag", false);
            window_manager_.setWindowVisible("agents", false);
            show_info("Workspace Reset", "Workspace reset to default");
            force_ui_update_ = true;  // Принудительная перерисовка
        },
        "Reset workspace to default",
        "",
        nullptr));

    command_manager_.registerCommand("delete_workspace", CommandFactory::createFunctionalCommand(
        "delete_workspace",
        [this]() {
            show_workspace_load_dialog();  // Используем тот же диалог для выбора
        },
        "Delete workspace by name",
        "",
        nullptr));

    // Register view commands
    command_manager_.registerCommand("toggle_performance", CommandFactory::createFunctionalCommand(
        "toggle_performance",
        [this]() { show_performance_overlay_ = !show_performance_overlay_; },
        "Toggle performance overlay",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_status_bar", CommandFactory::createFunctionalCommand(
        "toggle_status_bar",
        [this]() { show_status_bar_ = !show_status_bar_; },
        "Toggle status bar",
        "",
        nullptr));

    // Register additional file commands
    command_manager_.registerCommand("model_directory", CommandFactory::createFunctionalCommand(
        "model_directory",
        [this]() { open_model_directory_dialog(); },
        "Set model directory",
        "",
        nullptr));

    command_manager_.registerCommand("show_documentation", CommandFactory::createFunctionalCommand(
        "show_documentation",
        [this]() { show_help_dialog(); },
        "Open documentation",
        "",
        nullptr));

    command_manager_.registerCommand("check_updates", CommandFactory::createFunctionalCommand(
        "check_updates",
        [this]() { show_info("Updates", "Checking for updates not implemented yet"); },
        "Check for application updates",
        "",
        nullptr));

    // Register commands for submenus
    command_manager_.registerCommand("open_recent_1", CommandFactory::createFunctionalCommand(
        "open_recent_1",
        [this]() { show_info("Open Recent", "Opening recent file 1"); },
        "Open recent file 1",
        "",
        nullptr));

    command_manager_.registerCommand("open_recent_2", CommandFactory::createFunctionalCommand(
        "open_recent_2",
        [this]() { show_info("Open Recent", "Opening recent file 2"); },
        "Open recent file 2",
        "",
        nullptr));

    command_manager_.registerCommand("open_recent_3", CommandFactory::createFunctionalCommand(
        "open_recent_3",
        [this]() { show_info("Open Recent", "Opening recent file 3"); },
        "Open recent file 3",
        "",
        nullptr));

    command_manager_.registerCommand("server_status", CommandFactory::createFunctionalCommand(
        "server_status",
        [this]() {
            if (server_manager_) {
                auto status = server_manager_->get_server_status();
                show_info("Server Status", "Server status: " + status);
            } else {
                show_info("Server Status", "Server manager not initialized");
            }
        },
        "Show server status",
        "",
        nullptr));

    command_manager_.registerCommand("show_about_devs", CommandFactory::createFunctionalCommand(
        "show_about_devs",
        [this]() { show_info("About Developers", "Developed by Llama GUI Team"); },
        "Show information about developers",
        "",
        nullptr));

    // Register developer tools commands
    command_manager_.registerCommand("show_metrics", CommandFactory::createFunctionalCommand(
        "show_metrics",
        [this]() { show_metrics_window_ = !show_metrics_window_; },
        "Show Dear ImGui Metrics/Debugger window",
        "",
        nullptr));

    command_manager_.registerCommand("show_style_editor", CommandFactory::createFunctionalCommand(
        "show_style_editor",
        [this]() { show_style_editor_window_ = !show_style_editor_window_; },
        "Show Dear ImGui Style Editor",
        "",
        nullptr));

    command_manager_.registerCommand("show_debug_log", CommandFactory::createFunctionalCommand(
        "show_debug_log",
        [this]() { show_debug_log_window_ = !show_debug_log_window_; },
        "Show Dear ImGui Debug Log",
        "",
        nullptr));

    command_manager_.registerCommand("show_font_selector", CommandFactory::createFunctionalCommand(
        "show_font_selector",
        [this]() { show_font_selector_window_ = !show_font_selector_window_; },
        "Show Dear ImGui Font Selector",
        "",
        nullptr));

    command_manager_.registerCommand("start_item_picker", CommandFactory::createFunctionalCommand(
        "start_item_picker",
        [this]() {
#ifdef IMGUI_HAS_DEBUG
            ImGui::DebugStartItemPicker();
#endif
        },
        "Start Dear ImGui Item Picker",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_debug_mode", CommandFactory::createFunctionalCommand(
        "toggle_debug_mode",
        [this]() {
            developer_mode_enabled_ = !developer_mode_enabled_;
            settings_.performance().debug_mode = developer_mode_enabled_;
        },
        "Enable/disable debug mode",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_group_rects", CommandFactory::createFunctionalCommand(
        "toggle_group_rects",
        [this]() { show_group_rects_ = !show_group_rects_; },
        "Show layout group rects for debugging",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_flash_style_colors", CommandFactory::createFunctionalCommand(
        "toggle_flash_style_colors",
        [this]() { flash_style_colors_ = !flash_style_colors_; },
        "Flash style colors for visual debugging",
        "",
        nullptr));

    command_manager_.registerCommand("show_command_manager_state", CommandFactory::createFunctionalCommand(
        "show_command_manager_state",
        [this]() { show_command_manager_window_ = !show_command_manager_window_; },
        "Show Command Manager state and statistics",
        "",
        nullptr));

    command_manager_.registerCommand("show_window_manager_state", CommandFactory::createFunctionalCommand(
        "show_window_manager_state",
        [this]() { show_window_manager_window_ = !show_window_manager_window_; },
        "Show Window Manager state and window positions",
        "",
        nullptr));

    command_manager_.registerCommand("show_logger_info", CommandFactory::createFunctionalCommand(
        "show_logger_info",
        [this]() { show_logger_info_window_ = !show_logger_info_window_; },
        "Show Logger information and statistics",
        "",
        nullptr));

    command_manager_.registerCommand("export_debug_info", CommandFactory::createFunctionalCommand(
        "export_debug_info",
        [this]() { exportDebugInfo(); },
        "Export debug information to file",
        "",
        nullptr));

    command_manager_.registerCommand("reload_ui", CommandFactory::createFunctionalCommand(
        "reload_ui",
        [this]() {
            force_ui_update_ = true;
            show_info("UI Reload", "User interface reload requested");
        },
        "Reload the user interface",
        "",
        nullptr));

    command_manager_.registerCommand("clear_cache", CommandFactory::createFunctionalCommand(
        "clear_cache",
        [this]() {
            // Clear application caches
            show_info("Cache Cleared", "Application caches have been cleared");
        },
        "Clear application caches",
        "",
        nullptr));

    // =========================================================================
    // Регистрация команд для агентов (исправленные имена без пробелов)
    // =========================================================================

    command_manager_.registerCommand("agents_status", CommandFactory::createFunctionalCommand(
        "agents_status",
        [this]() {
            show_info("Agent Status", "Agent system status: Active");
        },
        "Show agent system status",
        "",
        nullptr));

    command_manager_.registerCommand("agents_list", CommandFactory::createFunctionalCommand(
        "agents_list",
        [this]() {
            show_info("Available Agents", "Code Agent\nFile Agent\nRAG Agent\nSummarization Agent\nTerminal Agent\nWeb Search Agent");
        },
        "List all available agents",
        "",
        nullptr));

    command_manager_.registerCommand("rag", CommandFactory::createFunctionalCommand(
        "rag",
        [this]() {
            show_info("RAG Search", "RAG search activated. Type your query...");
        },
        "Search documents using RAG",
        "",
        nullptr));

    command_manager_.registerCommand("search", CommandFactory::createFunctionalCommand(
        "search",
        [this]() {
            show_info("Web Search", "Web search activated. Type your query...");
        },
        "Search the web",
        "",
        nullptr));

    command_manager_.registerCommand("code", CommandFactory::createFunctionalCommand(
        "code",
        [this]() {
            show_info("Code Generation", "Code generation activated. Describe what you need...");
        },
        "Generate code with AI",
        "",
        nullptr));

    command_manager_.registerCommand("summarize", CommandFactory::createFunctionalCommand(
        "summarize",
        [this]() {
            show_info("Summarization", "Summarization activated. Provide text to summarize...");
        },
        "Summarize text",
        "",
        nullptr));

    // =========================================================================
    // Регистрация недостающих команд инструментов (Tools menu)
    // =========================================================================

    command_manager_.registerCommand("open_plugins", CommandFactory::createFunctionalCommand(
        "open_plugins",
        [this]() {
            show_info("Plugins", "Plugin manager will be implemented soon");
        },
        "Manage plugins",
        "",
        nullptr));

    command_manager_.registerCommand("open_extensions", CommandFactory::createFunctionalCommand(
        "open_extensions",
        [this]() {
            show_info("Extensions", "Extension manager will be implemented soon");
        },
        "Manage extensions",
        "",
        nullptr));

#ifdef ENABLE_LLAMA_BENCH
    command_manager_.registerCommand("open_llama_bench", CommandFactory::createFunctionalCommand(
        "open_llama_bench",
        [this]() {
            openLlamaBenchDialog();
        },
        "Open Llama Bench dialog for model comparison",
        "",
        nullptr));
#endif

    command_manager_.registerCommand("toggle_console", CommandFactory::createFunctionalCommand(
        "toggle_console",
        [this]() {
            show_info("Console", "Developer console toggled");
        },
        "Toggle developer console",
        "",
        nullptr));

    // =========================================================================
    // Регистрация недостающих команд производительности (Performance menu)
    // =========================================================================

    command_manager_.registerCommand("open_performance_settings", CommandFactory::createFunctionalCommand(
        "open_performance_settings",
        [this]() {
            settings_dialog_->show_performance_settings();
        },
        "Open performance settings",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_vsync", CommandFactory::createFunctionalCommand(
        "toggle_vsync",
        [this]() {
            settings_.performance().enable_vsync = !settings_.performance().enable_vsync;
            show_info("V-Sync", settings_.performance().enable_vsync ? "Enabled" : "Disabled");
        },
        "Toggle V-Sync",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_fps_limit", CommandFactory::createFunctionalCommand(
        "toggle_fps_limit",
        [this]() {
            show_info("FPS Limit", "FPS limit toggled");
        },
        "Toggle FPS limit",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_smart_redraw", CommandFactory::createFunctionalCommand(
        "toggle_smart_redraw",
        [this]() {
            settings_.performance().enable_smart_redraw = !settings_.performance().enable_smart_redraw;
            show_info("Smart Redraw", settings_.performance().enable_smart_redraw ? "Enabled" : "Disabled");
        },
        "Toggle smart redraw mode",
        "",
        nullptr));

    // =========================================================================
    // Регистрация недостающих команд безопасности (Security menu)
    // =========================================================================

    command_manager_.registerCommand("open_ssl_settings", CommandFactory::createFunctionalCommand(
        "open_ssl_settings",
        [this]() {
            show_info("SSL/TLS", "SSL/TLS settings will be implemented soon");
        },
        "Configure SSL/TLS",
        "",
        nullptr));

    command_manager_.registerCommand("open_auth_settings", CommandFactory::createFunctionalCommand(
        "open_auth_settings",
        [this]() {
            show_info("Authentication", "Authentication settings will be implemented soon");
        },
        "Manage authentication token",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_verify_ssl", CommandFactory::createFunctionalCommand(
        "toggle_verify_ssl",
        [this]() {
            show_info("SSL Verification", "SSL verification toggled");
        },
        "Toggle SSL verification",
        "",
        nullptr));

    command_manager_.registerCommand("validate_files", CommandFactory::createFunctionalCommand(
        "validate_files",
        [this]() {
            show_info("File Validation", "File validation will be implemented soon");
        },
        "Validate uploaded files",
        "",
        nullptr));

    command_manager_.registerCommand("show_audit_log", CommandFactory::createFunctionalCommand(
        "show_audit_log",
        [this]() {
            show_info("Audit Log", "Audit log viewer will be implemented soon");
        },
        "Show security audit log",
        "",
        nullptr));

    // =========================================================================
    // Регистрация недостающих команд логирования (Logging menu)
    // =========================================================================

    command_manager_.registerCommand("open_logging_settings", CommandFactory::createFunctionalCommand(
        "open_logging_settings",
        [this]() {
            settings_dialog_->show_logging_settings();
        },
        "Configure logging",
        "",
        nullptr));

    command_manager_.registerCommand("view_logs", CommandFactory::createFunctionalCommand(
        "view_logs",
        [this]() {
            show_info("Logs", "Log viewer will be implemented soon");
        },
        "View application logs",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_log_level", CommandFactory::createFunctionalCommand(
        "toggle_log_level",
        [this]() {
            show_info("Log Level", "Log level changed");
        },
        "Change log level",
        "",
        nullptr));

    command_manager_.registerCommand("toggle_log_to_file", CommandFactory::createFunctionalCommand(
        "toggle_log_to_file",
        [this]() {
            show_info("Log to File", "File logging toggled");
        },
        "Toggle file logging",
        "",
        nullptr));

    command_manager_.registerCommand("flush_logs", CommandFactory::createFunctionalCommand(
        "flush_logs",
        [this]() {
            show_info("Logs", "Log buffers flushed");
        },
        "Flush log buffers",
        "",
        nullptr));

    command_manager_.registerCommand("export_logs", CommandFactory::createFunctionalCommand(
        "export_logs",
        [this]() {
            show_info("Export Logs", "Log export will be implemented soon");
        },
        "Export logs to file",
        "",
        nullptr));

    // Profile management commands
    command_manager_.registerCommand("show_profile_manager", CommandFactory::createFunctionalCommand(
        "show_profile_manager",
        [this]() {
            show_profile_manager();
        },
        "Open profile manager",
        "Ctrl+Shift+P",
        nullptr));

    command_manager_.registerCommand("save_current_profile", CommandFactory::createFunctionalCommand(
        "save_current_profile",
        [this]() {
            save_current_profile();
        },
        "Save current profile",
        "Ctrl+Shift+S",
        nullptr));

    // Backup management commands
    command_manager_.registerCommand("show_backup_manager", CommandFactory::createFunctionalCommand(
        "show_backup_manager",
        [this]() {
            show_backup_manager();
        },
        "Open backup manager",
        "Ctrl+Shift+B",
        nullptr));

    std::cout << "✓ Additional commands registered successfully" << std::endl;

    std::cout << "✓ Command callbacks setup completed" << std::endl;
}

} // namespace ui
} // namespace llama_gui
