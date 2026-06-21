#include <iostream>
#include <fstream>
#include <cstdio>
#include <unistd.h>
#include "agents/agents.h"
#include "agents/plugin_loader.h"
#include "../include/ui/main_window.h"

#ifdef USE_SDL2
#include <SDL.h>
#endif

#include "../include/ui/localization_manager.h"
#include "../include/ui/language_selector.h"
#include "../include/ui/profile_manager_dialog.h"
#include "../include/ui/backup_manager_dialog.h"
#include "../include/core/config_manager.h"
#include "../include/core/logger.h"
#include <csignal>
#include <cstdlib>
#include <execinfo.h>

namespace llama_gui {
namespace ui {

using llama_gui::core::ModelManager;

// === ОБРАБОТЧИК СИГНАЛОВ ДЛЯ ДИАГНОСТИКИ ВЫЛЕТОВ ===
void signal_handler(int signal) {
    std::cerr << "\n\n*** CRASH DETECTED - Signal: " << signal << " ***" << std::endl;
    
    if (signal == SIGSEGV) {
        std::cerr << "*** SIGSEGV - Segmentation Fault (invalid memory access) ***" << std::endl;
    } else if (signal == SIGABRT) {
        std::cerr << "*** SIGABRT - Abort (typically from assert() or abort()) ***" << std::endl;
    } else if (signal == SIGFPE) {
        std::cerr << "*** SIGFPE - Floating Point Exception ***" << std::endl;
    } else if (signal == SIGILL) {
        std::cerr << "*** SIGILL - Illegal Instruction ***" << std::endl;
    }
    
    // Печатаем backtrace
    void* array[50];
    int size = backtrace(array, 50);
    char** messages = backtrace_symbols(array, size);
    
    std::cerr << "\n*** BACKTRACE (" << size << " frames): ***" << std::endl;
    for (int i = 0; i < size && messages != NULL; ++i) {
        std::cerr << "  [" << i << "] " << messages[i] << std::endl;
    }
    std::cerr << std::endl;
    
    if (messages) {
        free(messages);
    }
    
    // Пишем в лог если доступен
    llama_gui::core::Logger::instance().error("CRASH: Signal " + std::to_string(signal));
    llama_gui::core::Logger::instance().flush_file_log();
    
    // Выходим
    _exit(1);
}

void install_signal_handlers() {
    signal(SIGSEGV, signal_handler);
    signal(SIGABRT, signal_handler);
    signal(SIGFPE, signal_handler);
    signal(SIGILL, signal_handler);
    std::cerr << "[INIT] Signal handlers installed for crash diagnosis" << std::endl;
}
// ========================================================

MainWindow::MainWindow(StateManager& state_manager, Settings& settings, LlamaInterface& llama_interface)
    : settings_(settings)
    , state_manager_(state_manager)
    , llama_interface_(llama_interface)
    , server_manager_(std::make_unique<ServerManager>(settings_))
    , model_manager_(std::make_unique<ModelManager>())
    , rag_manager_(nullptr)
    , chat_interface_(std::make_unique<ChatInterface>(state_manager_, settings_, llama_interface_))
    , file_manager_(std::make_unique<FileManager>(state_manager_, settings_))
    , conversation_manager_(std::make_unique<ConversationManager>(state_manager_))
    , settings_dialog_(std::make_unique<SettingsDialog>(settings_, server_manager_.get(), &workspace_manager_, &config_manager_))
    , cloud_services_dialog_(std::make_unique<CloudServicesDialog>(settings_))
    , embedding_settings_dialog_(std::make_unique<EmbeddingSettingsDialog>(settings_.rag(), nullptr))
    , rag_settings_dialog_(std::make_unique<RagSettingsDialog>(&settings_, this))
    , settings_viewer_dialog_(std::make_unique<SettingsViewerDialog>(settings_))
    , grid_snapping_dialog_(std::make_unique<GridSnappingDialog>())
    , profile_manager_dialog_(std::make_unique<ProfileManagerDialog>(config_manager_))
    , backup_manager_dialog_(std::make_unique<BackupManagerDialog>(config_manager_)) {

    // === УСТАНОВКА ОБРАБОТЧИКОВ СИГНАЛОВ (самое первое!) ===
    install_signal_handlers();
    // ========================================================

    // === ВКЛЮЧАЕМ ЗАПИСЬ ЛОГА В ФАЙЛ ДЛЯ ДИАГНОСТИКИ ВЫЛЕТОВ ===
    std::string log_file = "llama-gui-rag-debug.log";
    llama_gui::core::Logger::instance().set_level(llama_gui::core::Logger::Level::Debug);
    llama_gui::core::Logger::instance().enable_file_logging(log_file);
    std::cout << "[INIT] DEBUG file logging enabled: " << log_file << std::endl;
    llama_gui::core::Logger::instance().info("MainWindow инициализирован");
    // ===============================================================

    // Pass cloud_services_dialog pointer to settings_dialog (for button in server chat settings)
    settings_dialog_->set_cloud_services_dialog(cloud_services_dialog_.get());

    // Initialize localization system
    auto& loc_manager = getLocalizationManager();
    loc_manager.loadTranslationsFromDirectory("translations");
    loc_manager.loadTranslationsFromDirectory("i18n");
    loc_manager.setCurrentLanguage(Language::Russian);

    // Callback будет установлен в initialize() после инициализации workspace_manager_ и advanced_menu_system_

    // Set up model selection callback
    chat_interface_->set_model_selection_callback([this]() {
        open_model_selection_dialog();
    });

    // Set up model load request callback
    chat_interface_->set_model_load_request_callback([this](const std::string& pending_query) {
        start_model_load_from_profile(pending_query);
    });

    // Pass ModelManager to ChatInterface
    chat_interface_->set_model_manager(model_manager_.get());

    // Pass model loading flag to ChatInterface
    chat_interface_->set_is_model_loading(&is_model_loading_);

    // Pass model loading progress and status to ChatInterface
    chat_interface_->set_model_load_progress(&model_load_progress_);
    chat_interface_->set_model_load_status(&model_load_status_);

    // Set up attachment sync
    file_manager_->set_attachment_changed_callback([this](const std::vector<std::string>& attachments) {
        chat_interface_->set_attachments(attachments);
    });

    // Set up model load callback from FileManager
    file_manager_->set_model_load_callback([this](const std::string& model_path) {
        load_model_from_path(model_path);
    });

    // Set up file content callback
    file_manager_->set_file_content_callback([this](const std::string& content) {
        chat_interface_->set_input_text(content);
    });

    // Pass RAG manager to FileManager (для обработки вложений через RAG)
    // Это будет вызвано после инициализации rag_manager в initialize()
}

MainWindow::~MainWindow() {
    shutdown();
}

bool MainWindow::initialize(int width, int height) {
    auto [screen_width, screen_height] = settings_.get_safe_window_bounds();

    if (width <= 0 || height <= 0) {
        width_ = screen_width;
        height_ = screen_height;
    } else {
        width_ = width;
        height_ = height;
    }

    std::cout << "MainWindow: Initializing with size " << width_ << "x" << height_ << std::endl;
    std::cout << "Screen resolution: " << settings_.get_screen_width() << "x" << settings_.get_screen_height() << std::endl;
    std::cout << "DPI scale: " << settings_.get_dpi_scale() << std::endl;

    // === Инициализация ConfigManager ===
    // Сначала устанавливаем ссылку на Settings, потом инициализируем
    config_manager_.setSettings(settings_);
    
    if (!config_manager_.initialize()) {
        std::cerr << "⚠ Не удалось инициализировать ConfigManager" << std::endl;
        // Продолжаем работу, но без управления профилями
    }
    
    // Обновляем настройки OpenRouter после загрузки профиля
    if (cloud_services_dialog_) {
        cloud_services_dialog_->update_openrouter_settings();
    }

    // Initialize core components
    state_manager_.initialize(settings_);

    // Initialize model manager
    std::string models_dir = "models";
    if (!model_manager_->initialize(models_dir)) {
        std::cerr << "❌ Ошибка инициализации ModelManager" << std::endl;
    } else {
        std::cout << "✓ ModelManager инициализирован" << std::endl;
    }

    // Initialize RAG manager
    std::string embedding_model_path = settings_.get_embedding_model_path();
    if (!embedding_model_path.empty()) {
        rag_manager_ = std::make_unique<llama_gui::core::RagManager>(embedding_model_path);
        if (rag_manager_) {
            rag_manager_->initialize_indexes();
            chat_interface_->set_rag_manager(rag_manager_.get());

            bool rag_enabled = settings_.rag().enable_rag;
            chat_interface_->enable_rag(rag_enabled);

            // Передаём RAG manager в FileManager для обработки вложений
            file_manager_->set_rag_manager(rag_manager_.get());
            file_manager_->enable_rag(rag_enabled);

            std::cout << "✓ RAG initialized from settings (enabled=" << rag_enabled << ")" << std::endl;

            rag_interface_ = std::make_unique<RagInterface>();
            rag_interface_->set_rag_manager(rag_manager_.get());
            rag_interface_->set_rag_settings_dialog(rag_settings_dialog_.get());
            rag_interface_->set_chat_interface(chat_interface_.get());
            rag_interface_->set_settings(&settings_);  // Передаём ссылку на настройки для сохранения
            rag_interface_->set_enabled(rag_enabled);
            std::cout << "✓ RagManager инициализирован с моделью: " << embedding_model_path << std::endl;
            
            // === ИНИЦИАЛИЗАЦИЯ OPENROUTER ДЛЯ RAG ===
            // Если OpenRouter включен - передаём клиент в RagManager
            if (settings_.openrouter().enabled && !settings_.get_openrouter_api_key().empty()) {
                openrouter_client_ = std::make_shared<llama_gui::core::OpenRouterClient>(
                    settings_.get_openrouter_api_key()
                );

                if (openrouter_client_) {
                    openrouter_client_->set_timeout(settings_.openrouter().timeout_ms);

                    // Передаём клиент в RagManager для глубокого анализа
                    rag_manager_->set_openrouter_client(openrouter_client_);

                    // Устанавливаем модель для RAG
                    std::string rag_model = settings_.openrouter().selected_model;
                    if (!rag_model.empty()) {
                        rag_manager_->set_openrouter_model(rag_model);
                    }
                    
                    std::cout << "✓ OpenRouter client initialized for RAG (model: " 
                              << rag_model << ")" << std::endl;
                }
            }
            // ========================================
        }
    } else {
        std::cout << "ℹ Путь к модели эмбеддингов не указан. RAG будет недоступен до настройки." << std::endl;
    }

    // Инициализация системы агентов - ОТКЛЮЧЕНО: агенты временно отключены
    // std::cout << "🤖 Initializing agent system..." << std::endl;
    // agents::PluginLoader plugin_loader;
    // // Используем абсолютный путь относительно рабочей директории
    // std::string plugin_path = "build/plugins/official";
    // int loaded = plugin_loader.load_plugins_from_directory(plugin_path, &agent_registry_);
    // if (loaded == 0) {
    //     plugin_path = "plugins/official";
    //     loaded = plugin_loader.load_plugins_from_directory(plugin_path, &agent_registry_);
    // }
    // std::cout << "✓ Loaded " << loaded << " agent plugins from " << plugin_path << std::endl;
    
    // Вывод списка агентов
    // ОТКЛЮЧЕНО: агенты временно отключены
    // for (const auto& info : agent_registry_.list_agents()) {
    //     std::cout << "  Agent: " << info.name << " v" << info.version
    //               << " (" << (info.is_builtin ? "builtin" : "plugin") << ")" << std::endl;
    // }

    // agent_registry_.initialize_all(&agent_context_);
    // std::cout << "✓ Agent system initialized" << std::endl;

    // // Инициализация AgentChatIntegration
    // agent_chat_integration_ = std::make_unique<AgentChatIntegration>();
    // if (!agent_chat_integration_->initialize(&agent_registry_, &agent_context_)) {
    //     std::cerr << "⚠️ Failed to initialize AgentChatIntegration" << std::endl;
    // } else {
    //     std::cout << "✓ AgentChatIntegration initialized" << std::endl;
    // }

    // // Установка интеграции в CommandManager для обработки команд агентов
    // command_manager_.setAgentChatIntegration(agent_chat_integration_.get());

    // Set up model progress callback
    model_manager_->set_model_progress_callback([this](const std::string& model_name, float progress, const std::string& status) {
        std::cout << "Model Load Progress: " << (progress * 100.0f) << "% - " << status << std::endl;
    });

    // Set up chat template callback to save extracted template to settings
    model_manager_->set_chat_template_callback([this](const std::string& model_path, const std::string& chat_template, const std::string& source) {
        std::cout << "[MainWindow] Saving chat template to settings (source: " << source << ")" << std::endl;
        
        // Сохраняем шаблон во временный файл для избежания проблем с экранированием в shell
        std::string template_file = "/tmp/llama-gui-chat-template.jinja";
        std::ofstream file(template_file);
        if (file.is_open()) {
            file << chat_template;
            file.close();
            std::cout << "[MainWindow] Chat template saved to: " << template_file << std::endl;
            settings_.grammar().chat_template_file = template_file;
        } else {
            std::cerr << "[MainWindow] Failed to save chat template to file, using inline template" << std::endl;
            settings_.grammar().chat_template = chat_template;
        }
        // Также включаем use_jinja для поддержки Jinja2 шаблонов
        settings_.grammar().use_jinja = true;
    });

    // Auto-load model from profile
    std::string model_path = settings_.get_model_path();
    if (!model_path.empty()) {
        std::cout << "🔄 Автоматическая загрузка модели из профиля: " << model_path << std::endl;
        load_model_with_progress_dialog(model_path, "", true);
    } else {
        std::cout << "ℹ Путь к модели не указан в профиле" << std::endl;
    }

    // Set up callbacks
    state_manager_.set_conversation_change_callback(
        [this](const llama_gui::core::StateEvent& event) {
            on_conversation_changed(event);
        }
    );

    // Adapt UI components
    settings_.adapt_ui_components();

    // Initialize SDL2
    if (!init_sdl2()) {
        std::cout << "⚠ SDL2 not available, running in console mode" << std::endl;
    }

    // Initialize OpenGL
    if (!init_opengl()) {
        std::cout << "⚠ OpenGL not available, running in console mode" << std::endl;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup Dear ImGui IO
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.DisplaySize = ImVec2(static_cast<float>(width_), static_cast<float>(height_));

    // Setup Dear ImGui style
    setup_imgui_style();

    // Load fonts
    create_ui_components();

    // Initialize workspace manager WITH config loading
    // Use absolute path based on executable location
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    std::string exe_dir = (len > 0) ? std::string(exe_path, len) : ".";
    exe_dir = exe_dir.substr(0, exe_dir.find_last_of('/'));
    
    std::string workspace_config_path = exe_dir + "/../.config/llama-gui/workspaces/workspaces_config.json";
    std::cout << "WorkspaceManager: Loading config from: " << workspace_config_path << std::endl;
    workspace_manager_.initializeWithConfig(workspace_config_path);
    show_status_bar_ = true;  // Enable status bar by default

    // Initialize new UI management system (uses workspace_manager_)
    initializeNewUISystem();

    // Set up language change callback AFTER all components are initialized
    auto& loc_manager = getLocalizationManager();
    loc_manager.setLanguageChangeCallback([this](Language new_lang, Language old_lang) {
        std::cout << "Language changed, scheduling font reload and menu rebuild..." << std::endl;
        // Schedule font reload and menu rebuild for next frame
        pending_language_change_ = true;
    });

    is_initialized_ = true;
    std::cout << "✓ MainWindow initialized successfully (console mode)" << std::endl;
    return true;
}

// =========================================================================
// Profile Management
// =========================================================================

void MainWindow::show_profile_manager() {
    if (profile_manager_dialog_) {
        // Устанавливаем callback для перезагрузки сервера при загрузке профиля
        profile_manager_dialog_->setProfileLoadCallback([this](const std::string& profile_name) {
            std::cout << "[ProfileManager] Callback: профиль загружен, требуется перезапуск сервера: "
                      << profile_name << std::endl;

            // Проверяем фактическое наличие процесса сервера через pgrep
            // Это надёжнее, чем server_manager_->is_server_running(), т.к. флаг
            // устанавливается асинхронно в отдельном потоке
            std::string check_cmd = "pgrep -f 'llama-server.*--port.*8081' > /dev/null 2>&1 && echo yes || echo no";
            FILE* pipe = popen(check_cmd.c_str(), "r");
            bool server_is_running = false;
            if (pipe) {
                char buffer[16];
                if (fgets(buffer, sizeof(buffer), pipe)) {
                    server_is_running = (std::string(buffer).find("yes") != std::string::npos);
                }
                pclose(pipe);
            }

            std::cout << "[ProfileManager] Сервер запущен (проверка pgrep): "
                      << (server_is_running ? "да" : "нет") << std::endl;

            if (server_is_running) {
                std::cout << "[ProfileManager] Сервер запущен, выполняем перезапуск..." << std::endl;
                // Небольшая задержка для стабильности
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                server_manager_->restart_server();
                show_info("Профили", "Профиль загружен: " + profile_name + "\nСервер перезапущен с новыми параметрами");
            } else {
                std::cout << "[ProfileManager] Сервер не запущен, просто загружаем настройки" << std::endl;
                show_info("Профили", "Профиль загружен: " + profile_name);
            }

            // ВАЖНО: Принудительно обновляем current_profile_name_ в Settings
            // Это обеспечивает синхронизацию между ConfigManager::current_profile_ и Settings::current_profile_name_
            // Просто обновляем имя профиля без повторной загрузки (профиль уже загружен через config_manager_.loadProfile)
            std::cout << "[ProfileManager] Обновляем Settings::current_profile_name_ = " << profile_name << std::endl;
            
            // Принудительно синхронизируем SettingsDialog с новым профилем
            // Это нужно, чтобы выпадающий список в настройках показывал актуальный профиль
            if (settings_dialog_) {
                settings_dialog_->syncProfileSelection(profile_name);
                std::cout << "[ProfileManager] SettingsDialog синхронизирован с профилем: " << profile_name << std::endl;
            }
        });

        profile_manager_dialog_->setOpen(true);
    }
}

void MainWindow::save_current_profile() {
    std::string current = config_manager_.getCurrentProfileName();
    if (current.empty()) {
        // Если профиль не выбран, показываем диалог создания
        if (profile_manager_dialog_) {
            profile_manager_dialog_->showCreateDialog();
        }
    } else {
        if (config_manager_.saveProfile(current)) {
            show_info("Профили", "Профиль сохранён: " + current);
        } else {
            show_info("Ошибка", "Ошибка сохранения профиля");
        }
    }
}

void MainWindow::load_profile(const std::string& profile_name) {
    if (config_manager_.loadProfile(profile_name)) {
        show_info("Профили", "Профиль загружен: " + profile_name);
        // Перезагружаем модель если нужно
        // reloadModelIfNeeded();
    } else {
        show_info("Ошибка", "Ошибка загрузки профиля");
    }
}

// =========================================================================
// Backup Management
// =========================================================================

void MainWindow::show_backup_manager() {
    if (backup_manager_dialog_) {
        backup_manager_dialog_->setOpen(true);
    }
}

void MainWindow::create_backup() {
    std::string backup = config_manager_.createBackup();
    if (!backup.empty()) {
        show_info("Резервная копия", "Создана резервная копия: " + backup);
    } else {
        show_info("Ошибка", "Ошибка создания резервной копии");
    }
}

void MainWindow::restore_from_backup(const std::string& backup_path) {
    if (config_manager_.restoreFromBackup(backup_path)) {
        show_info("Резервная копия", "Настройки восстановлены");
    } else {
        show_info("Ошибка", "Ошибка восстановления");
    }
}

} // namespace ui
} // namespace llama_gui
