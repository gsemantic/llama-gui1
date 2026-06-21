#include "../include/ui/settings_dialog.h"
#include "../include/ui/settings_dialog_model.h"
#include "../include/ui/settings_dialog_gpu.h"
#include "../include/ui/settings_dialog_sampling_basic.h"
#include "../include/ui/settings_dialog_sampling_advanced.h"
#include "../include/ui/settings_dialog_context.h"
#include "../include/ui/settings_dialog_rope.h"
#include "../include/ui/settings_dialog_advanced.h"
#include "../include/ui/settings_dialog_grammar.h"
#include "../include/ui/settings_dialog_server_runtime.h"
#include "../include/ui/settings_dialog_batch.h"
#include "../include/ui/settings_dialog_logging.h"
#include "../include/ui/quick_settings_dialog.h"
#include "../include/ui/advanced_settings_dialog.h"
#include "../include/ui/openrouter_models_dialog.h"
#include "../include/ui/localization_manager.h"
#include "../include/core/logger.h"
#include "../include/core/config_manager.h"
#include "../external/imgui/imgui.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>

namespace llama_gui {
namespace ui {

// Helper function for tooltips with Russian text
void SettingsDialog::HelpMarker(const std::string& desc) {
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc.c_str());
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

SettingsDialog::SettingsDialog(Settings& settings, ServerManager* server_manager, WorkspaceManager* workspace_manager, llama_gui::core::ConfigManager* config_manager)
    : settings_(settings)
    , server_manager_(server_manager)
    , workspace_manager_(workspace_manager)
    , config_manager_(config_manager) {

    // Initialize current profile name
    current_profile_name_[0] = '\0';

    // Initialize system prompt settings module
    system_prompt_settings_ = std::make_unique<SystemPromptSettings>(settings_);

    // =========================================================================
    // Инициализация новых диалогов (двухуровневая система)
    // =========================================================================

    // Диалог быстрых настроек
    quick_settings_dialog_ = std::make_unique<QuickSettingsDialog>(settings_);

    // Диалог расширенных настроек
    advanced_settings_dialog_ = std::make_unique<AdvancedSettingsDialog>(settings_);

    // =========================================================================
    // Старые диалоги (для обратной совместимости)
    // =========================================================================

    // Initialize model settings dialog
    model_settings_dialog_ = std::make_unique<ModelSettingsDialog>(settings_);

    // Initialize GPU settings dialog
    gpu_settings_dialog_ = std::make_unique<GPUSettingsDialog>(settings_);

    // Initialize sampling settings dialogs
    sampling_basic_dialog_ = std::make_unique<SamplingBasicDialog>(settings_);
    sampling_advanced_dialog_ = std::make_unique<SamplingAdvancedDialog>(settings_);

    // Initialize context and RoPE settings dialogs
    context_dialog_ = std::make_unique<ContextDialog>(settings_);
    rope_dialog_ = std::make_unique<RoPEDialog>(settings_);

    // Initialize Advanced and Grammar settings dialogs
    advanced_dialog_ = std::make_unique<AdvancedDialog>(settings_);
    grammar_dialog_ = std::make_unique<GrammarDialog>(settings_);

    // Initialize Server Runtime, Batch and Logging settings dialogs
    server_runtime_dialog_ = std::make_unique<ServerRuntimeDialog>(settings_);
    batch_dialog_ = std::make_unique<BatchDialog>(settings_);
    logging_dialog_ = std::make_unique<LoggingDialog>(settings_);

    // Initialize OpenRouter models dialog
    openrouter_models_dialog_ = std::make_unique<OpenRouterModelsDialog>(settings_);
}

// Деструктор должен быть после всех include для корректного уничтожения unique_ptr
SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::render() {
    if (!show_dialog_) return;

    // Для расширенных настроек используем окно большего размера
    if (!show_quick_) {
        ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    }

    ImGui::OpenPopup("Settings");

    if (ImGui::BeginPopupModal("Settings", &show_dialog_, show_quick_ ? ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_None)) {

        // Profiles management section at the top
        // Показываем управление профилями ТОЛЬКО в расширенных настройках
        // В быстрых настройках это вызывает рассинхронизацию с ProfileManagerDialog
        if (!show_quick_) {
            render_profile_manager();
            ImGui::Separator();
        }

        // Переключатель между быстрыми и расширенными настройками
        // Скрываем для User режима - оставляем только быстрые настройки
        bool is_user_mode = workspace_manager_ && 
                           workspace_manager_->getCurrentWorkspaceType() == WorkspaceType::User;

        if (!is_user_mode) {
            // Показываем переключатель только для Developer и Admin
            ImGui::Text("%s", TR("settings.mode"));
            ImGui::SameLine();

            if (ImGui::Button(show_quick_ ? TR("settings.quick") : TR("settings.advanced"))) {
                show_quick_ = !show_quick_;
            }

            ImGui::SameLine();
            ImGui::TextDisabled(show_quick_
                ? TR("settings.frequently_used")
                : TR("settings.detailed"));
        }
        // Для User режима не показываем заголовок - он будет в секции ниже

        ImGui::Separator();

        if (show_quick_) {
            // Рендерим быстрые настройки - показываем только одну секцию за раз
            // В режиме пользователя не показываем заголовок секции
            if (!is_user_mode) {
                ImGui::Text(TR("settings.quick"));
                ImGui::Separator();
            }
            
            switch (current_tab_) {
                case SettingsTab::Server:
                    render_server_settings();
                    break;
                case SettingsTab::Chat:
                    render_chat_settings();
                    break;
                case SettingsTab::Models:
                    render_model_settings();
                    break;
                case SettingsTab::UI:
                    render_ui_settings();
                    break;
                default:
                    render_server_settings();
                    break;
            }
        } else {
            // Рендерим расширенные настройки через AdvancedSettingsDialog
            if (advanced_settings_dialog_) {
                advanced_settings_dialog_->render();
            }
        }

        ImGui::Separator();

        // Buttons
        if (ImGui::Button(TR("button.save"))) {
            save_settings();
            hide();
        }
        ImGui::SameLine();
        if (ImGui::Button(TR("button.apply"))) {
            apply_settings();
        }
        ImGui::SameLine();
        if (ImGui::Button(TR("button.reset"))) {
            reset_settings();
        }
        ImGui::SameLine();
        if (ImGui::Button(TR("button.cancel"))) {
            cancel_settings();
        }

        ImGui::EndPopup();
    }
}

void SettingsDialog::render_profile_manager() {
    ImGui::Text(TR("profiles.title"));

    // Profile selection combo
    std::vector<std::string> profiles = settings_.list_profiles();

    // Инициализируем current_loaded_profile_ при первом запуске
    if (current_loaded_profile_.empty() && !profiles.empty()) {
        std::string initial_profile = settings_.get_current_profile_name();
        if (!initial_profile.empty()) {
            current_loaded_profile_ = initial_profile;
        } else {
            current_loaded_profile_ = profiles[0];
        }
    }

    // Синхронизируем UI с current_loaded_profile_ (который обновляется через syncProfileSelection)
    // НЕ используем settings_.get_current_profile_name() напрямую, т.к. он может быть не обновлён
    if (!current_loaded_profile_.empty()) {
        // Находим индекс текущего профиля
        for (int i = 0; i < static_cast<int>(profiles.size()); i++) {
            if (profiles[i] == current_loaded_profile_) {
                current_profile_idx_ = i;
                break;
            }
        }
    }

    // Update current profile index if needed
    if (current_profile_idx_ == -1 && !profiles.empty()) {
        current_profile_idx_ = 0;
    }

    // Ensure index is valid
    if (current_profile_idx_ >= static_cast<int>(profiles.size())) {
        current_profile_idx_ = -1;
    }

    const char* preview_value = (current_profile_idx_ >= 0 && current_profile_idx_ < profiles.size())
                              ? profiles[current_profile_idx_].c_str()
                              : TR("profiles.select");

    // Устанавливаем ширину для combo box
    ImGui::SetNextItemWidth(250.0f);
    if (ImGui::BeginCombo("##profile_combo", preview_value)) {
        for (int i = 0; i < static_cast<int>(profiles.size()); i++) {
            const bool is_selected = (current_profile_idx_ == i);
            const bool is_current_active = (profiles[i] == settings_.get_current_profile_name());
            
            // Формируем текст с индикатором текущего профиля
            std::string display_name = profiles[i];
            if (is_current_active) {
                display_name += " (активный)";
            }
            
            // Выделяем текущий активный профиль цветом
            if (is_current_active) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            }
            
            if (ImGui::Selectable(display_name.c_str(), is_selected)) {
                // Загружаем профиль сразу при выборе через ConfigManager
                // Это обеспечивает синхронизацию ConfigManager::current_profile_ и Settings::current_profile_name_
                if (config_manager_ && config_manager_->loadProfile(profiles[i])) {
                    current_profile_idx_ = i;
                    current_loaded_profile_ = profiles[i];
                    status_message_ = TRF("profiles.loaded", profiles[i].c_str());
                    profile_loaded_pending_apply_ = true;
                } else if (!config_manager_ && settings_.load_profile(profiles[i])) {
                    // Fallback: если config_manager не установлен, используем напрямую settings_
                    current_profile_idx_ = i;
                    current_loaded_profile_ = profiles[i];
                    status_message_ = TRF("profiles.loaded", profiles[i].c_str());
                    profile_loaded_pending_apply_ = true;
                } else {
                    status_message_ = TR("profiles.error_load");
                }
            }
            
            if (is_current_active) {
                ImGui::PopStyleColor();
            }
            
            if (is_selected) {
                ImGui::SetItemDefaultFocus();
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Load button
    if (ImGui::Button(TR("profiles.load")) && current_profile_idx_ >= 0 && current_profile_idx_ < profiles.size()) {
        if (config_manager_ && config_manager_->loadProfile(profiles[current_profile_idx_])) {
            current_loaded_profile_ = profiles[current_profile_idx_];
            status_message_ = TRF("profiles.loaded", profiles[current_profile_idx_].c_str());
            profile_loaded_pending_apply_ = true;
        } else if (!config_manager_ && settings_.load_profile(profiles[current_profile_idx_])) {
            // Fallback: если config_manager не установлен, используем напрямую settings_
            current_loaded_profile_ = profiles[current_profile_idx_];
            status_message_ = TRF("profiles.loaded", profiles[current_profile_idx_].c_str());
            profile_loaded_pending_apply_ = true;
        } else {
            status_message_ = TR("profiles.error_load");
        }
    }

    ImGui::SameLine();

    // Apply Profile button (shows after loading a profile)
    if (profile_loaded_pending_apply_) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.7f, 0.0f, 1.0f));
        if (ImGui::Button(TR("profiles.apply_restart"))) {
            apply_profile();
            profile_loaded_pending_apply_ = false;
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", TR("profiles.apply_restart_hint"));
        ImGui::SameLine();
    }

    // Delete button - нельзя удалить текущий активный профиль
    std::string current_active_profile = settings_.get_current_profile_name();
    bool is_current_profile = (current_profile_idx_ >= 0 && current_profile_idx_ < profiles.size() && 
                               profiles[current_profile_idx_] == current_active_profile);
    
    if (ImGui::Button(TR("profiles.delete")) && current_profile_idx_ >= 0 && current_profile_idx_ < profiles.size()) {
        if (is_current_profile) {
            status_message_ = TR("profiles.error_delete_current");
        } else if (settings_.delete_profile(profiles[current_profile_idx_])) {
            status_message_ = TRF("profiles.deleted", profiles[current_profile_idx_].c_str());
            current_profile_idx_ = -1; // Reset selection
        } else {
            status_message_ = TR("profiles.error_delete");
        }
    }
    
    // Показываем подсказку если выбран текущий профиль
    if (is_current_profile && current_profile_idx_ >= 0 && current_profile_idx_ < profiles.size()) {
        ImGui::SameLine();
        ImGui::TextDisabled("(active - %s)", TR("profiles.cannot_delete_active"));
    }

    // Save new profile
    ImGui::InputTextWithHint("##new_profile_name", TR("profiles.new_hint"), current_profile_name_, sizeof(current_profile_name_));
    ImGui::SameLine();

    if (ImGui::Button(TR("profiles.save_as"))) {
        std::string name = current_profile_name_;
        if (!name.empty()) {
            if (settings_.save_profile(name)) {
                status_message_ = TRF("profiles.saved", name.c_str());
                // Clear input
                current_profile_name_[0] = '\0';
            } else {
                status_message_ = TR("profiles.error_save");
            }
        } else {
            status_message_ = TR("profiles.error_no_name");
        }
    }

    // Status message
    if (!status_message_.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "%s", status_message_.c_str());
    }
}

void SettingsDialog::save_settings() {
    std::cout << "SettingsDialog: Saving settings" << std::endl;

    // Apply system prompt settings before saving
    system_prompt_settings_->apply_settings();

    // Сохраняем в текущий загруженный профиль
    if (!current_loaded_profile_.empty()) {
        if (settings_.save_profile(current_loaded_profile_)) {
            status_message_ = TRF("profiles.saved_to", current_loaded_profile_.c_str());
            std::cout << "Settings saved to profile: " << current_loaded_profile_ << std::endl;
        } else {
            status_message_ = TR("profiles.error_save_file");
            std::cerr << "Failed to save profile: " << current_loaded_profile_ << std::endl;
        }
    } else {
        status_message_ = TR("profiles.error_no_loaded");
        std::cerr << "No profile loaded to save settings" << std::endl;
    }
}

void SettingsDialog::reset_settings() {
    std::cout << "SettingsDialog: Resetting settings" << std::endl;
    settings_.reset_to_defaults();

    // Also reset system prompt settings to default
    system_prompt_settings_->reset_to_default();
}

void SettingsDialog::apply_settings() {
    std::cout << "SettingsDialog: Applying settings" << std::endl;
}

void SettingsDialog::apply_profile() {
    std::cout << "SettingsDialog: Applying profile: " << current_loaded_profile_ << std::endl;
    
    // Перезапуск сервера с новыми настройками из профиля
    if (server_manager_) {
        std::cout << "SettingsDialog: Restarting server with profile settings..." << std::endl;
        
        // Останавливаем текущий сервер
        server_manager_->stop_server();
        
        // Небольшая задержка для остановки
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
        // Запускаем сервер с новыми настройками
        if (server_manager_->restart_server()) {
            status_message_ = TRF("profiles.applied", current_loaded_profile_.c_str());
            std::cout << "SettingsDialog: Server restarted with profile: " << current_loaded_profile_ << std::endl;
        } else {
            status_message_ = TR("profiles.error_apply");
            std::cerr << "SettingsDialog: Failed to restart server with profile" << std::endl;
        }
    } else {
        status_message_ = TR("profiles.error_no_server");
        std::cerr << "SettingsDialog: ServerManager not available" << std::endl;
    }
}

void SettingsDialog::cancel_settings() {
    restore_backup();
    // Also restore system prompt settings from backup
    system_prompt_settings_->restore_backup();
    hide();
}

void SettingsDialog::backup_current_settings() {
    // Create a backup by serializing and deserializing
    backup_json_ = settings_.serialize_to_json();

    // Also backup system prompt settings
    system_prompt_settings_->backup_current_settings();
}

void SettingsDialog::restore_backup() {
    if (!backup_json_.empty()) {
        settings_.deserialize_from_json(backup_json_);
    }

    // Also restore system prompt settings from backup
    system_prompt_settings_->restore_backup();
}

void SettingsDialog::syncProfileSelection(const std::string& profile_name) {
    // Принудительно синхронизируем индекс и имя профиля
    current_loaded_profile_ = profile_name;

    // ВАЖНО: Обновляем также current_profile_name_ в Settings
    // Это обеспечивает консистентность между SettingsDialog и Settings
    settings_.set_current_profile_name(profile_name);

    // Находим индекс профиля в списке
    std::vector<std::string> profiles = settings_.list_profiles();
    current_profile_idx_ = -1;

    for (int i = 0; i < static_cast<int>(profiles.size()); i++) {
        if (profiles[i] == profile_name) {
            current_profile_idx_ = i;
            break;
        }
    }

    std::cout << "[SettingsDialog] syncProfileSelection: профиль=" << profile_name
              << ", индекс=" << current_profile_idx_ << std::endl;
}

} // namespace ui
} // namespace llama_gui
