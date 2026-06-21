#pragma once

#include <string>
#include <vector>
#include <functional>

namespace llama_gui {
namespace core {
    class Settings; // Forward declaration
}
}

namespace llama_gui {
namespace ui {

// Forward declaration
class RagSettingsDialog;
class ChatInterface;

class RagInterface {
public:
    RagInterface();
    ~RagInterface();

    void render_ui(bool* visible = nullptr);
    void set_rag_manager(void* rag_manager); // Установка ссылки на RagManager
    void set_enabled(bool enabled) { rag_enabled_ = enabled; }
    bool is_enabled() const { return rag_enabled_; }
    void set_rag_settings_dialog(void* rag_settings_dialog) { rag_settings_dialog_ = rag_settings_dialog; }
    void update_settings_from_manager();

    // Установка ссылки на ChatInterface для синхронизации
    void set_chat_interface(ChatInterface* chat_interface) { chat_interface_ = chat_interface; }
    
    // Установка ссылки на Settings для сохранения
    void set_settings(llama_gui::core::Settings* settings) { settings_ = settings; }

    // Синхронизация состояния RAG с ChatInterface
    void sync_rag_state_with_chat();

    // === Управление профилями индексов ===
    void show_profile_selector();

private:
    std::vector<std::string> loaded_documents_;
    bool rag_enabled_ = false;
    bool show_rag_panel_ = false;
    float progress_ = 0.0f;
    bool processing_ = false;
    std::string status_message_ = "";
    std::string current_operation_ = "";
    bool show_create_profile_dialog_ = false;  // Флаг диалога создания профиля
    std::string pending_profile_name_;         // Предварительно заполненное имя профиля (из имени документа)
    std::string pending_document_path_;        // Путь документа, ожидающего привязки к новому профилю
    
    // Диалог выбора профиля при загрузке документа
    bool show_profile_choice_dialog_ = false;  // Флаг диалога выбора профиля
    std::string pending_doc_for_choice_;       // Документ для выбора профиля
    std::string current_profile_name_display_; // Имя текущего профиля для отображения в диалоге

    // Функции обработки
    void handle_document_upload();
    void handle_document_remove(int index);
    void process_uploaded_documents();
    void clear_documents();

    // === Персистентность: методы управления индексом ===
    void save_index();
    void load_index();
    void clear_index();
    size_t get_loaded_chunks_count();
    std::string get_persistent_index_path();
    bool persistent_index_exists();

    // Вспомогательные функции
    std::string get_filename_from_path(const std::string& path);
    std::string get_filename_without_extension(const std::string& path);
    std::vector<std::string> get_supported_extensions();
    
    // Диалог выбора профиля
    void show_profile_choice_dialog();
    void add_document_to_current_profile(const std::string& doc_path);
    void create_new_profile_for_document(const std::string& doc_path);

    // Callback для обновления прогресса
    std::function<void(float)> progress_callback_;

    // Указатель на RagManager
    void* rag_manager_ = nullptr;

    // Указатель на диалог настроек RAG
    void* rag_settings_dialog_ = nullptr;
    
    // Указатель на ChatInterface для синхронизации
    ChatInterface* chat_interface_ = nullptr;
    
    // Указатель на Settings для сохранения
    llama_gui::core::Settings* settings_ = nullptr;
};

} // namespace ui
} // namespace llama_gui