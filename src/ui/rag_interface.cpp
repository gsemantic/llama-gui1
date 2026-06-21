#include "../include/ui/rag_interface.h"
#include "../include/ui/rag_settings_dialog.h"
#include "../include/ui/file_dialog_helper.h"
#include "../include/core/rag_manager.h"
#include "../include/core/settings.h"
#include "../include/ui/chat_interface.h"
#include "../include/ui/localization_manager.h"
#include <imgui.h>
#include <thread>
#include <algorithm>
#include <iostream>
#include <vector>
#include <string>
#include <cstring>

namespace llama_gui {
namespace ui {

RagInterface::RagInterface() = default;

RagInterface::~RagInterface() = default;

void RagInterface::show_profile_selector() {
    if (!rag_manager_) {
        ImGui::Text("RAG manager not initialized");
        return;
    }

    auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);

    // Получаем список профилей
    std::vector<std::string> profiles = rag_manager->get_index_profile_names();

    // ComboBox для выбора профиля
    ImGui::Text("Index Profile:");
    ImGui::SameLine();

    // Создаём массив C-string для ComboBox
    // Первым всегда идёт "New профиль" для создания нового
    std::vector<const char*> items;
    items.reserve(profiles.size() + 1);
    items.push_back("+ Новый профиль");
    for (const auto& profile : profiles) {
        items.push_back(profile.c_str());
    }

    // Текущий выбранный элемент
    static int current_item = -1;
    // Инициализируем current_item на основе текущего профиля
    if (current_item == -1 && !profiles.empty()) {
        std::string current_profile = rag_manager->get_current_index_profile();
        if (!current_profile.empty()) {
            for (size_t i = 0; i < profiles.size(); ++i) {
                if (profiles[i] == current_profile) {
                    current_item = static_cast<int>(i) + 1; // +1 потому что "New профиль" первый
                    break;
                }
            }
        } else {
            // Если нет текущего профиля, выбираем "New профиль"
            current_item = 0;
        }
    }

    ImGui::SetNextItemWidth(200);
    
    // Блокируем переключение профилей, пока открыт диалог выбора
    bool was_opened = ImGui::IsPopupOpen("Выбор профиля для документа", ImGuiPopupFlags_AnyPopupId);
    if (show_profile_choice_dialog_) {
        ImGui::BeginDisabled();
    }
    
    if (ImGui::Combo("##profile_selector", &current_item, items.data(), static_cast<int>(items.size()))) {
        if (current_item == 0) {
            // Выбран "New профиль" - открываем диалог создания
            show_create_profile_dialog_ = true;
            current_item = -1; // Сбрасываем, чтобы можно было выбрать снова
        } else if (current_item > 0 && current_item <= static_cast<int>(profiles.size())) {
            // Переключаем профиль при выборе
            rag_manager->switch_index_profile(profiles[current_item - 1]);
            status_message_ = "Switched to profile: " + profiles[current_item - 1];
            
            // Очищаем список загруженных документов при переключении профиля
            loaded_documents_.clear();
            std::cout << "[RAG UI] Cleared loaded_documents_ after profile switch" << std::endl;
        }
    }
    
    if (show_profile_choice_dialog_) {
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "⚠ Сначала выберите профиль для документа");
    }

    // Определяем выбранный профиль для проверки can_delete
    bool can_delete = current_item > 0 && current_item <= static_cast<int>(profiles.size());
    std::string selected_profile = can_delete ? profiles[current_item - 1] : "";

    ImGui::SameLine();
    if (ImGui::Button("Create")) {
        show_create_profile_dialog_ = true;
    }

    ImGui::SameLine();
    // Кнопка Delete - удаляет выбранный профиль (включая текущий, с авто-переключением)
    if (ImGui::Button("Delete", ImVec2(0, 0)) && can_delete) {
        if (rag_manager->delete_index_profile(selected_profile, true)) {
            status_message_ = "Deleted profile: " + selected_profile;
            current_item = -1;
        } else {
            status_message_ = "Error: Cannot delete profile";
        }
    }

    // Диалог создания профиля
    if (show_create_profile_dialog_) {
        ImGui::OpenPopup("Create Index Profile");
    }

    if (ImGui::BeginPopupModal("Create Index Profile", &show_create_profile_dialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char profile_name_buf[256] = "";
        static char source_dir_buf[512] = "";

        // Если есть предварительно заполненное имя, используем его при первом открытии
        if (!pending_profile_name_.empty() && strlen(profile_name_buf) == 0) {
            std::strncpy(profile_name_buf, pending_profile_name_.c_str(), sizeof(profile_name_buf) - 1);
            profile_name_buf[sizeof(profile_name_buf) - 1] = '\0';
        }

        ImGui::Text("Profile Name:");
        ImGui::InputText("##profile_name", profile_name_buf, sizeof(profile_name_buf));

        ImGui::Text("Source Directory (optional):");
        ImGui::InputText("##source_dir", source_dir_buf, sizeof(source_dir_buf));

        ImGui::Separator();

        if (ImGui::Button("Create", ImVec2(120, 0))) {
            if (strlen(profile_name_buf) > 0) {
                rag_manager->create_index_profile(profile_name_buf, source_dir_buf);
                status_message_ = "Created profile: " + std::string(profile_name_buf);
                
                std::cout << "[RAG UI] Profile created: " << profile_name_buf << std::endl;
                std::cout << "[RAG UI] pending_document_path_ empty: " << (pending_document_path_.empty() ? "YES" : "NO") << std::endl;
                std::cout << "[RAG UI] pending_document_path_: " << pending_document_path_ << std::endl;
                
                // Если есть ожидающий документ, загружаем его после создания профиля
                if (!pending_document_path_.empty()) {
                    std::string doc_path_copy = pending_document_path_;
                    loaded_documents_.push_back(doc_path_copy);
                    
                    std::cout << "[RAG UI] Will process document after profile creation: " << doc_path_copy << std::endl;

                    // Обрабатываем только новый документ
                    if (rag_manager_) {
                        auto* rag_manager_ptr = static_cast<llama_gui::core::RagManager*>(rag_manager_);
                        processing_ = true;
                        progress_ = 0.0f;
                        current_operation_ = "Processing";

                        std::thread processing_thread([this, rag_manager_ptr, doc_path_copy]() {
                            std::cout << "[RAG UI] Thread started, processing: " << doc_path_copy << std::endl;
                            try {
                                rag_manager_ptr->process_document(doc_path_copy);
                                std::cout << "[RAG UI] Document processed successfully: " << doc_path_copy << std::endl;
                                std::cout << "[RAG UI] External chunks count: " << rag_manager_ptr->get_external_chunks_count() << std::endl;
                                progress_ = 1.0f;
                                processing_ = false;
                                status_message_ = "Документ обработан и профиль создан";
                            } catch (const std::exception& e) {
                                std::cerr << "[RAG UI] Error in profile creation thread: " << e.what() << std::endl;
                                progress_ = 1.0f;
                                processing_ = false;
                                status_message_ = "Ошибка: " + std::string(e.what());
                            }
                        });
                        processing_thread.detach();
                        std::cout << "[RAG UI] Processing thread launched" << std::endl;
                    } else {
                        std::cerr << "[RAG UI] ERROR: rag_manager_ is null during profile creation!" << std::endl;
                    }

                    pending_document_path_.clear();
                } else {
                    std::cout << "[RAG UI] No pending document to process" << std::endl;
                }

                profile_name_buf[0] = '\0';
                source_dir_buf[0] = '\0';
                pending_profile_name_.clear();
                show_create_profile_dialog_ = false;
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            profile_name_buf[0] = '\0';
            source_dir_buf[0] = '\0';
            pending_profile_name_.clear();
            pending_document_path_.clear();
            show_create_profile_dialog_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void RagInterface::show_profile_choice_dialog() {
    if (!rag_manager_) return;
    
    // Открываем popup если флаг установлен
    if (show_profile_choice_dialog_) {
        std::cout << "[RAG UI] Opening profile choice dialog popup" << std::endl;
        ImGui::OpenPopup("Выбор профиля для документа");
    }

    if (ImGui::BeginPopupModal("Выбор профиля для документа", &show_profile_choice_dialog_, ImGuiWindowFlags_AlwaysAutoResize)) {
        std::cout << "[RAG UI] Profile choice dialog popup is VISIBLE" << std::endl;
        std::string doc_name = get_filename_without_extension(pending_doc_for_choice_);

        ImGui::Text("Документ: %s", doc_name.c_str());
        ImGui::Separator();
        ImGui::Text("Текущий профиль: %s", current_profile_name_display_.c_str());
        ImGui::Separator();
        ImGui::Text("");
        ImGui::TextWrapped("Выберите действие:");
        ImGui::Text("");

        // Кнопка: Добавить к текущему профилю - большая и заметная
        if (ImGui::Button("  Добавить к текущему профилю  ", ImVec2(400, 35))) {
            std::cout << "[RAG UI] User clicked 'Add to current profile'" << std::endl;
            std::cout << "[RAG UI] pending_doc_for_choice_: " << pending_doc_for_choice_ << std::endl;
            std::cout << "[RAG UI] current_profile_name_display_: " << current_profile_name_display_ << std::endl;
            
            add_document_to_current_profile(pending_doc_for_choice_);
            show_profile_choice_dialog_ = false;
            pending_doc_for_choice_.clear();
            current_profile_name_display_.clear();
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::Text("  Профиль: %s", current_profile_name_display_.c_str());
        ImGui::Separator();
        ImGui::Text("");

        // Кнопка: Создать новый профиль - большая и заметная
        if (ImGui::Button("  Создать новый профиль с именем документа  ", ImVec2(400, 35))) {
            std::cout << "[RAG UI] ========== USER CLICKED 'CREATE NEW' ==========" << std::endl;
            std::cout << "[RAG UI] pending_doc_for_choice_ = '" << pending_doc_for_choice_ << "'" << std::endl;
            std::cout << "[RAG UI] doc_name = '" << doc_name << "'" << std::endl;
            std::cout << "[RAG UI] BEFORE create_new_profile_for_document" << std::endl;
            
            create_new_profile_for_document(pending_doc_for_choice_);
            
            std::cout << "[RAG UI] AFTER create_new_profile_for_document" << std::endl;
            std::cout << "[RAG UI] pending_document_path_ is now: '" << pending_document_path_ << "'" << std::endl;
            std::cout << "[RAG UI] show_create_profile_dialog_ = " << (show_create_profile_dialog_ ? "true" : "false") << std::endl;
            std::cout << "[RAG UI] ========== END CLICK HANDLER ==========" << std::endl;
            
            show_profile_choice_dialog_ = false;
            pending_doc_for_choice_.clear();
            current_profile_name_display_.clear();
            
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::Text("  Новый профиль будет назван: %s", doc_name.c_str());
        ImGui::Separator();
        ImGui::Text("");

        // Кнопка: Отмена
        if (ImGui::Button("Отмена", ImVec2(150, 0))) {
            show_profile_choice_dialog_ = false;
            pending_doc_for_choice_.clear();
            current_profile_name_display_.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void RagInterface::add_document_to_current_profile(const std::string& doc_path) {
    loaded_documents_.push_back(doc_path);
    
    // Сохраняем имя профиля до очистки
    std::string profile_name = current_profile_name_display_;
    
    // Обрабатываем только новый документ
    if (rag_manager_) {
        auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);
        processing_ = true;
        progress_ = 0.0f;
        current_operation_ = "Processing";
        status_message_ = "Обработка документа: " + get_filename_without_extension(doc_path);
        
        std::cout << "[RAG UI] Starting document processing: " << doc_path << std::endl;
        std::cout << "[RAG UI] Target profile: " << profile_name << std::endl;
        
        std::thread processing_thread([this, rag_manager, doc_path, profile_name]() {
            try {
                rag_manager->process_document(doc_path);
                std::cout << "[RAG UI] Document processed successfully: " << doc_path << std::endl;
                std::cout << "[RAG UI] External chunks count: " << rag_manager->get_external_chunks_count() << std::endl;
                
                progress_ = 1.0f;
                processing_ = false;
                status_message_ = "Документ добавлен к профилю: " + profile_name;
            } catch (const std::exception& e) {
                std::cerr << "[RAG UI] Error processing document: " << e.what() << std::endl;
                progress_ = 1.0f;
                processing_ = false;
                status_message_ = "Ошибка при обработке документа: " + std::string(e.what());
            }
        });
        processing_thread.detach();
    } else {
        status_message_ = "Документ добавлен к профилю: " + profile_name;
    }
}

void RagInterface::create_new_profile_for_document(const std::string& doc_path) {
    std::cout << "[RAG UI] >>> create_new_profile_for_document CALLED" << std::endl;
    std::cout << "[RAG UI] >>> doc_path argument = '" << doc_path << "'" << std::endl;
    
    std::string doc_name = get_filename_without_extension(doc_path);
    std::cout << "[RAG UI] >>> doc_name extracted = '" << doc_name << "'" << std::endl;
    
    pending_profile_name_ = doc_name;
    pending_document_path_ = doc_path;
    show_create_profile_dialog_ = true;
    
    std::cout << "[RAG UI] >>> After assignment:" << std::endl;
    std::cout << "[RAG UI] >>>   pending_document_path_ = '" << pending_document_path_ << "'" << std::endl;
    std::cout << "[RAG UI] >>>   show_create_profile_dialog_ = " << (show_create_profile_dialog_ ? "true" : "false") << std::endl;
    
    status_message_ = "Создайте новый профиль для документа: " + doc_name;
    std::cout << "[RAG UI] <<< create_new_profile_for_document RETURNING" << std::endl;
}

void RagInterface::sync_rag_state_with_chat() {
    // Синхронизируем состояние RAG между RagInterface и ChatInterface
    if (chat_interface_) {
        // Проверяем, изменилось ли состояние
        bool chat_rag_enabled = chat_interface_->is_rag_enabled();
        if (rag_enabled_ != chat_rag_enabled) {
            // Обновляем локальное состояние
            rag_enabled_ = chat_rag_enabled;
            std::cout << "RagInterface: Synced RAG state from ChatInterface (enabled=" << rag_enabled_ << ")" << std::endl;
        }
    }
}

void RagInterface::render_ui(bool* visible) {
    // Отладка: отслеживаем состояние pending_document_path_
    static bool prev_show_create = show_create_profile_dialog_;
    if (prev_show_create != show_create_profile_dialog_) {
        std::cout << "[RAG UI] show_create_profile_dialog_ changed: " << (show_create_profile_dialog_ ? "true" : "false") << std::endl;
        std::cout << "[RAG UI] pending_document_path_ at change: " << pending_document_path_ << std::endl;
        prev_show_create = show_create_profile_dialog_;
    }
    
    if (!pending_document_path_.empty() && show_create_profile_dialog_) {
        static float last_check = 0;
        float now = ImGui::GetTime();
        if (now - last_check > 1.0f) {
            std::cout << "[RAG UI] Still waiting with pending document: " << pending_document_path_ << std::endl;
            last_check = now;
        }
    }
    
    // Стандартное окно ImGui с заголовком и кнопками управления
    // Кнопка закрытия (×) и сворачивания (─) рисуются автоматически ImGui
    if (!ImGui::Begin("RAG", visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    // Синхронизируем состояние при каждом кадре
    sync_rag_state_with_chat();

    // === Селектор профилей индексов ===
    ImGui::Separator();
    show_profile_selector();
    
    // === Диалог выбора профиля для документа ===
    show_profile_choice_dialog();
    
    ImGui::Separator();

    bool prev_rag_enabled = rag_enabled_;
    ImGui::Checkbox("Enable RAG", &rag_enabled_);

    // Если состояние изменилось, синхронизируем с ChatInterface и сохраняем настройки
    if (prev_rag_enabled != rag_enabled_ && chat_interface_) {
        chat_interface_->enable_rag(rag_enabled_);
        std::cout << "RagInterface: RAG " << (rag_enabled_ ? "включен" : "выключен") << " (синхронизация с ChatInterface)" << std::endl;
        
        // Сохраняем настройки в профиль
        if (settings_) {
            std::string profile_name = settings_->get_current_profile_name();
            if (settings_->save_profile(profile_name)) {
                std::cout << "✓ RAG settings saved to profile: " << profile_name << std::endl;
            } else {
                std::cerr << "✗ Failed to save RAG settings to profile: " << profile_name << std::endl;
            }
        }
    }

    if (ImGui::Button("Load Documents")) {
        handle_document_upload();
    }

    ImGui::SameLine();
    if (ImGui::Button("Clear Documents")) {
        clear_documents();
    }

    ImGui::SameLine();
    if (ImGui::Button("RAG Settings")) {
        // Открываем диалог настроек RAG
        if (rag_settings_dialog_) {
            // Приводим void* к правильному типу и вызываем метод
            // Это немного небезопасно, но в рамках текущего проекта приемлемо
            static_cast<RagSettingsDialog*>(rag_settings_dialog_)->set_visible(true);
        }
    }

    // === ПЕРСИСТЕНТНОСТЬ: Кнопки управления индексом ===
    ImGui::Separator();
    ImGui::Text(TR("rag.persistence"));
    
    if (ImGui::Button(TR("rag.save_index"))) {
        save_index();
    }
    ImGui::SameLine();
    
    if (ImGui::Button(TR("rag.load_index"))) {
        load_index();
    }
    ImGui::SameLine();
    
    if (ImGui::Button(TR("rag.clear_index"))) {
        clear_index();
    }

    // Показываем прогресс обработки
    if (processing_) {
        ImGui::Text("%s... %d%%", current_operation_.c_str(), static_cast<int>(progress_ * 100));
        ImGui::ProgressBar(progress_);
    }

    // Показываем статус
    if (!status_message_.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "%s", status_message_.c_str());
    }

    // === Отображение статуса персистентности ===
    ImGui::Separator();
    ImGui::Text(TR("rag.index_status"));
    
    // Показываем информацию о загруженных чанках
    ImGui::Text(TR("rag.loaded_chunks"), get_loaded_chunks_count());
    
    // Показываем путь к персистентному индексу
    std::string index_path = get_persistent_index_path();
    bool index_exists = persistent_index_exists();

    char index_info[512];
    snprintf(index_info, sizeof(index_info), "%s %s", TR("rag.index_file"), index_path.c_str());
    ImGui::Text("%s", index_info);
    ImGui::SameLine();
    if (index_exists) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "%s", TR("rag.index_exists"));
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", TR("rag.index_not_found"));
    }

    // Показываем загруженные документы
    if (!loaded_documents_.empty()) {
        ImGui::Separator();
        ImGui::Text("Loaded Documents (%zu):", loaded_documents_.size());

        for (size_t i = 0; i < loaded_documents_.size(); ++i) {
            ImGui::PushID(static_cast<int>(i));

            std::string filename = get_filename_from_path(loaded_documents_[i]);
            ImGui::Text("%s", filename.c_str());

            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) {
                handle_document_remove(static_cast<int>(i));
            }

            ImGui::PopID();
        }
    }

    ImGui::End();
}

void RagInterface::handle_document_upload() {
    // Вызываем метод FileDialogHelper для выбора файла
    FileDialogHelper helper;
    helper.open_file_dialog("Select document", [this](const std::string& path) {
        std::cout << "[RAG UI] File selected: " << path << std::endl;
        if (!path.empty()) {
            // Проверяем, есть ли текущий профиль
            if (rag_manager_) {
                std::cout << "[RAG UI] rag_manager_ is valid" << std::endl;
                auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);
                std::string current_profile = rag_manager->get_current_index_profile();
                std::cout << "[RAG UI] Current profile: '" << current_profile << "'" << std::endl;
                
                // Если текущего профиля нет, предлагаем создать профиль с именем документа
                if (current_profile.empty()) {
                    std::cout << "[RAG UI] No profile, showing create dialog" << std::endl;
                    std::string doc_name = get_filename_without_extension(path);
                    std::cout << "[RAG UI] Setting pending_document_path_ to: " << path << std::endl;
                    pending_profile_name_ = doc_name;
                    pending_document_path_ = path;
                    show_create_profile_dialog_ = true;
                    status_message_ = "Создайте профиль для документа: " + doc_name;
                    std::cout << "[RAG UI] pending_document_path_ is now: " << pending_document_path_ << std::endl;
                    return; // Ждем создания профиля перед обработкой
                } else {
                    // Профиль есть - показываем диалог выбора
                    std::cout << "[RAG UI] Profile exists, showing choice dialog" << std::endl;
                    current_profile_name_display_ = current_profile;
                    pending_doc_for_choice_ = path;
                    show_profile_choice_dialog_ = true;
                    status_message_ = "Выберите профиль для документа: " + get_filename_without_extension(path);
                    return; // Ждем выбора пользователя
                }
            } else {
                std::cerr << "[RAG UI] ERROR: rag_manager_ is null!" << std::endl;
            }
            
            // Если rag_manager не инициализирован, просто добавляем документ
            std::cout << "[RAG UI] Fallback: adding document directly" << std::endl;
            loaded_documents_.push_back(path);
            std::thread processing_thread(&RagInterface::process_uploaded_documents, this);
            processing_thread.detach();
        }
    });
}

void RagInterface::process_uploaded_documents() {
    processing_ = true;
    progress_ = 0.0f;
    current_operation_ = "Processing";
    
    // Обновляем статус
    status_message_ = "Processing documents...";
    
    // Получаем RagManager через ссылку
    if (rag_manager_) {
        auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);
        
        int total_docs = static_cast<int>(loaded_documents_.size());
        for (int i = 0; i < total_docs; ++i) {
            if (rag_manager) {
                rag_manager->process_document(loaded_documents_[i]);
            }
            
            // Обновляем прогресс
            progress_ = static_cast<float>(i + 1) / total_docs;
        }
    }
    
    processing_ = false;
    current_operation_ = "";
    status_message_ = "Documents processed successfully!";
}

void RagInterface::handle_document_remove(int index) {
    if (index >= 0 && index < static_cast<int>(loaded_documents_.size())) {
        loaded_documents_.erase(loaded_documents_.begin() + index);
        status_message_ = "Document removed";
    }
}

void RagInterface::clear_documents() {
    loaded_documents_.clear();
    status_message_ = "Documents cleared";
}

// === Персистентность: реализация методов управления индексом ===
void RagInterface::save_index() {
    if (rag_manager_) {
        auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);
        if (rag_manager->save_index()) {
            status_message_ = "Index saved successfully!";
            std::cout << "[RAG UI] Index saved successfully" << std::endl;
        } else {
            status_message_ = "Failed to save index";
            std::cerr << "[RAG UI] Failed to save index" << std::endl;
        }
    } else {
        status_message_ = "RAG manager not initialized";
        std::cerr << "[RAG UI] Cannot save index: RAG manager not initialized" << std::endl;
    }
}

void RagInterface::load_index() {
    if (rag_manager_) {
        auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);
        if (rag_manager->load_index()) {
            status_message_ = "Index loaded successfully!";
            std::cout << "[RAG UI] Index loaded successfully" << std::endl;
        } else {
            status_message_ = "Failed to load index (file may not exist)";
            std::cerr << "[RAG UI] Failed to load index" << std::endl;
        }
    } else {
        status_message_ = "RAG manager not initialized";
        std::cerr << "[RAG UI] Cannot load index: RAG manager not initialized" << std::endl;
    }
}

void RagInterface::clear_index() {
    if (rag_manager_) {
        auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);
        rag_manager->clear_all_indexes();
        status_message_ = "Index cleared";
        std::cout << "[RAG UI] Index cleared" << std::endl;
    } else {
        status_message_ = "RAG manager not initialized";
        std::cerr << "[RAG UI] Cannot clear index: RAG manager not initialized" << std::endl;
    }
}

size_t RagInterface::get_loaded_chunks_count() {
    if (rag_manager_) {
        auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);
        return rag_manager->get_external_chunks_count();
    }
    return 0;
}

std::string RagInterface::get_persistent_index_path() {
    if (rag_manager_) {
        auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);
        
        // Получаем путь из текущего профиля
        std::string index_path = rag_manager->get_current_index_path();
        
        // Если профиль не установлен или путь пустой, используем путь по умолчанию
        if (index_path.empty()) {
            const char* home = getenv("HOME");
            if (!home) {
                home = ".";
            }
            return std::string(home) + "/.llama-gui/rag_indexes/rag_index.faiss";
        }
        
        return index_path;
    }
    return "";
}

bool RagInterface::persistent_index_exists() {
    if (rag_manager_) {
        auto* rag_manager = static_cast<llama_gui::core::RagManager*>(rag_manager_);
        return rag_manager->has_persistent_index();
    }
    return false;
}

std::string RagInterface::get_filename_from_path(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

std::string RagInterface::get_filename_without_extension(const std::string& path) {
    std::string filename = get_filename_from_path(path);
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(0, pos);
    }
    return filename;
}

std::vector<std::string> RagInterface::get_supported_extensions() {
    return {".txt"}; // Пока поддерживаем только текстовые файлы
    // В будущем можно добавить: {".txt", ".pdf", ".docx"}
}

void RagInterface::set_rag_manager(void* rag_manager) {
    rag_manager_ = rag_manager;
}

void RagInterface::update_settings_from_manager() {
    // Этот метод будет вызываться для обновления настроек из RAG-менеджера
    // В текущей реализации он может быть использован для синхронизации состояния
    if (rag_manager_) {
        // Пример: обновление состояния включения RAG
        // В реальной реализации здесь может быть больше логики
    }
}

} // namespace ui
} // namespace llama_gui