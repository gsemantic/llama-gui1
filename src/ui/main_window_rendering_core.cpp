#include "../include/ui/main_window.h"
#include <iostream>
#include <algorithm>

#ifdef USE_OPENGL
#include "../external/imgui/backends/imgui_impl_opengl3.h"
#include <GL/gl.h>
#endif

#include "../external/imgui/imgui.h"
#include "../external/imgui/imgui_internal.h"  // Для FindWindowByName

namespace llama_gui {
namespace ui {

void MainWindow::render() {
    render_ui();
    render_status_bar();
    render_error_dialogs();
}

void MainWindow::render_ui() {
    process_pending_dialog_results();
    handle_keyboard_shortcuts();

    if (show_menu_bar_) {
        render_menu_bar();
    }

    render_main_layout();
    render_open_conversation_dialog();
    render_save_conversation_dialog();
    render_model_selection_dialog();
    render_settings_dialog();
    render_rag_settings_dialog();
    render_embedding_settings_dialog();
    render_settings_viewer_dialog();
    render_grid_snapping_dialog();
    render_profile_manager_dialog();
    render_backup_manager_dialog();
#ifdef ENABLE_LLAMA_BENCH
    render_llama_bench_dialog();
#endif
    render_workspace_save_dialog();
    render_workspace_load_dialog();
    
    // Cloud services dialog - render LAST to be on top of all popups
    render_cloud_services_dialog();
}

void MainWindow::render_menu_bar() {
    if (force_ui_update_ || ui_dirty_menu_) {
        advanced_menu_system_.updateMenuStates();
        ui_dirty_menu_ = false;
    }
    advanced_menu_system_.renderMainMenu();
}

void MainWindow::render_main_layout() {
    auto [safe_width, safe_height] = settings_.get_safe_window_bounds();
    
    // Отрисовка сетки примагничивания (если включена визуализация)
    auto& grid_system = window_manager_.getGridSnappingSystem();
    if (grid_system.isEnabled() && grid_system.isGridOverlayVisible()) {
        grid_system.renderGridOverlay(safe_width, safe_height);
    }
    
    int margin = 10;
    int menu_height = 30;

    int left_panel_width = std::min(400, safe_width / 3);
    int chat_width = safe_width - left_panel_width - 2 * margin;
    int conversations_height = (safe_height - menu_height - 3 * margin) / 2;
    int files_height = (safe_height - menu_height - 3 * margin) - conversations_height;
    int component_height = static_cast<int>((safe_height - menu_height - 2 * margin) * 0.97f);

    float conversations_x = margin;
    float conversations_y = menu_height + margin;
    float files_x = margin;
    float files_y = conversations_y + conversations_height + margin;
    float chat_x = left_panel_width + 2 * margin;
    float chat_y = menu_height + margin;

    bool show_conversations = window_manager_.isWindowVisible("conversations");
    bool show_files = window_manager_.isWindowVisible("files");
    bool show_chat = window_manager_.isWindowVisible("chat");

    bool use_smart_redraw = settings_.performance().enable_smart_redraw;

    // Определяем условие для применения позиций
    ImGuiCond position_cond = force_apply_window_positions_ ? ImGuiCond_Always : 
                              (workspace_just_loaded_ ? ImGuiCond_Once : ImGuiCond_FirstUseEver);

    // Сбрасываем флаг после определения условия
    if (force_apply_window_positions_) {
        force_apply_window_positions_ = false;
    }

    // Проверяем, отпустил ли пользователь кнопку мыши (для примагничивания после перетаскивания)
    bool mouse_left = ImGui::GetIO().MouseDown[0];
    bool mouse_released = !mouse_left && prev_mouse_left_;
    prev_mouse_left_ = mouse_left;

    // Проверяем, перетаскивается или масштабируется ли окно в данный момент
    // Используем IsWindowHovered с флагом AnyWindow для проверки активности окон
    bool is_window_being_dragged = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow) && 
                                   ImGui::IsMouseDragging(0);
    
    // Проверяем, масштабируется ли окно - курсор должен быть одним из resize-курсоров
    bool is_window_being_resized = false;
    ImGuiMouseCursor cursor = ImGui::GetMouseCursor();
    if (mouse_left && (cursor == ImGuiMouseCursor_ResizeEW || cursor == ImGuiMouseCursor_ResizeNS ||
                       cursor == ImGuiMouseCursor_ResizeNESW || cursor == ImGuiMouseCursor_ResizeNWSE)) {
        is_window_being_resized = true;
    }

    // Отрисовка окон
    if (show_conversations && (use_smart_redraw ? ui_dirty_conversations_ : true)) {
        ImVec2 conv_pos(static_cast<float>(conversations_x), static_cast<float>(conversations_y));
        ImVec2 conv_size(static_cast<float>(left_panel_width), static_cast<float>(conversations_height));

        // Проверяем, есть ли сохраненная позиция в window_manager
        auto conv_state = window_manager_.getWindowState("conversations");
        bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();

        if (conv_state.position.x != 0 || conv_state.position.y != 0) {
            // Если примагничивание включено И кнопка мыши отпущена, применяем примагниченную позицию
            if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
                ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(conv_state.position);
                ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(conv_state.size);
                ImGui::SetNextWindowPos(snapped_pos, ImGuiCond_Always);
                ImGui::SetNextWindowSize(snapped_size, ImGuiCond_Always);
            } else if (snap_enabled && !is_window_being_dragged && !is_window_being_resized) {
                // В обычном режиме (не перетаскивание/масштабирование) используем сохраненную позицию
                ImGui::SetNextWindowPos(conv_state.position, ImGuiCond_Always);
                ImGui::SetNextWindowSize(conv_state.size, ImGuiCond_Always);
            } else {
                // Во время перетаскивания/масштабирования не устанавливаем позицию/размер явно
                // позволяя ImGui обрабатывать это самостоятельно
                ImGui::SetNextWindowPos(conv_state.position, ImGuiCond_Once);
                ImGui::SetNextWindowSize(conv_state.size, ImGuiCond_Once);
            }
        } else {
            ImGui::SetNextWindowPos(conv_pos, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(conv_size, ImGuiCond_FirstUseEver);
        }

        conversation_manager_->render(&show_conversations);

        if (use_smart_redraw) ui_dirty_conversations_ = false;
    }
    // Получаем фактическую позицию окна и сохраняем её в window_manager
    if (ImGuiWindow* win = ImGui::FindWindowByName(TR("conversations.title"))) {
        bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();
        // Обновляем позицию всегда, чтобы отслеживать перемещение в реальном времени
        if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
            // Применяем примагничивание только после отпускания мыши
            ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(win->Pos);
            ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(win->Size);
            window_manager_.updateWindowPosition("conversations", snapped_pos, false);
            window_manager_.updateWindowSize("conversations", snapped_size, false);
        } else if (!is_window_being_resized) {
            // Во время перетаскивания обновляем позицию без примагничивания
            // Но во время масштабирования НЕ обновляем размер, чтобы избежать неконтролируемого роста
            window_manager_.updateWindowPosition("conversations", win->Pos, true);
            window_manager_.updateWindowSize("conversations", win->Size, true);
        }
    }

    if (show_files && (use_smart_redraw ? ui_dirty_files_ : true)) {
        ImVec2 files_pos(static_cast<float>(files_x), static_cast<float>(files_y));
        ImVec2 files_size(static_cast<float>(left_panel_width), static_cast<float>(files_height));

        // Проверяем, есть ли сохраненная позиция в window_manager
        auto files_state = window_manager_.getWindowState("files");
        bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();

        if (files_state.position.x != 0 || files_state.position.y != 0) {
            // Если примагничивание включено И кнопка мыши отпущена, применяем примагниченную позицию
            if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
                ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(files_state.position);
                ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(files_state.size);
                ImGui::SetNextWindowPos(snapped_pos, ImGuiCond_Always);
                ImGui::SetNextWindowSize(snapped_size, ImGuiCond_Always);
            } else if (snap_enabled && !is_window_being_dragged && !is_window_being_resized) {
                // В обычном режиме (не перетаскивание/масштабирование) используем сохраненную позицию
                ImGui::SetNextWindowPos(files_state.position, ImGuiCond_Always);
                ImGui::SetNextWindowSize(files_state.size, ImGuiCond_Always);
            } else {
                // Во время перетаскивания/масштабирования не устанавливаем позицию/размер явно
                ImGui::SetNextWindowPos(files_state.position, ImGuiCond_Once);
                ImGui::SetNextWindowSize(files_state.size, ImGuiCond_Once);
            }
        } else {
            ImGui::SetNextWindowPos(files_pos, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(files_size, ImGuiCond_FirstUseEver);
        }

        file_manager_->render(&show_files);

        if (use_smart_redraw) ui_dirty_files_ = false;
    }
    // Получаем фактическую позицию окна и сохраняем её в window_manager
    if (ImGuiWindow* win = ImGui::FindWindowByName(TR("files.title"))) {
        bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();
        // Обновляем позицию всегда, чтобы отслеживать перемещение в реальном времени
        if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
            // Применяем примагничивание только после отпускания мыши
            ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(win->Pos);
            ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(win->Size);
            window_manager_.updateWindowPosition("files", snapped_pos, false);
            window_manager_.updateWindowSize("files", snapped_size, false);
        } else if (!is_window_being_resized) {
            // Во время перетаскивания обновляем позицию без примагничивания
            // Но во время масштабирования НЕ обновляем размер, чтобы избежать неконтролируемого роста
            window_manager_.updateWindowPosition("files", win->Pos, true);
            window_manager_.updateWindowSize("files", win->Size, true);
        }
    }

    if (show_chat && (use_smart_redraw ? ui_dirty_chat_ : true)) {
        ImVec2 chat_pos(static_cast<float>(chat_x), static_cast<float>(chat_y));
        ImVec2 chat_size(static_cast<float>(chat_width), static_cast<float>(component_height));

        // Проверяем, есть ли сохраненная позиция в window_manager
        auto chat_state = window_manager_.getWindowState("chat");
        bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();

        if (chat_state.position.x != 0 || chat_state.position.y != 0) {
            // Если примагничивание включено И кнопка мыши отпущена, применяем примагниченную позицию
            if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
                ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(chat_state.position);
                ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(chat_state.size);
                ImGui::SetNextWindowPos(snapped_pos, ImGuiCond_Always);
                ImGui::SetNextWindowSize(snapped_size, ImGuiCond_Always);
            } else if (snap_enabled && !is_window_being_dragged && !is_window_being_resized) {
                // В обычном режиме (не перетаскивание/масштабирование) используем сохраненную позицию
                ImGui::SetNextWindowPos(chat_state.position, ImGuiCond_Always);
                ImGui::SetNextWindowSize(chat_state.size, ImGuiCond_Always);
            } else {
                // Во время перетаскивания/масштабирования не устанавливаем позицию/размер явно
                ImGui::SetNextWindowPos(chat_state.position, ImGuiCond_Once);
                ImGui::SetNextWindowSize(chat_state.size, ImGuiCond_Once);
            }
        } else {
            ImGui::SetNextWindowPos(chat_pos, ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(chat_size, ImGuiCond_FirstUseEver);
        }

        chat_interface_->render(&show_chat);

        if (use_smart_redraw) ui_dirty_chat_ = false;
    }
    // Получаем фактическую позицию окна и сохраняем её в window_manager
    if (ImGuiWindow* win = ImGui::FindWindowByName("Chat")) {
        bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();
        // Обновляем позицию всегда, чтобы отслеживать перемещение в реальном времени
        if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
            // Применяем примагничивание только после отпускания мыши
            ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(win->Pos);
            ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(win->Size);
            window_manager_.updateWindowPosition("chat", snapped_pos, false);
            window_manager_.updateWindowSize("chat", snapped_size, false);
        } else if (!is_window_being_resized) {
            // Во время перетаскивания обновляем позицию без примагничивания
            // Но во время масштабирования НЕ обновляем размер, чтобы избежать неконтролируемого роста
            window_manager_.updateWindowPosition("chat", win->Pos, true);
            window_manager_.updateWindowSize("chat", win->Size, true);
        }
    }

    // RAG окно
    bool show_rag = window_manager_.isWindowVisible("rag");
    if (rag_interface_ && show_rag) {
        // Устанавливаем позицию и размер перед открытием окна
        auto rag_state = window_manager_.getWindowState("rag");
        bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();

        if (rag_state.position.x != 0 || rag_state.position.y != 0) {
            if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
                ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(rag_state.position);
                ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(rag_state.size);
                ImGui::SetNextWindowPos(snapped_pos, ImGuiCond_Always);
                ImGui::SetNextWindowSize(snapped_size, ImGuiCond_Always);
            } else if (snap_enabled && !is_window_being_dragged && !is_window_being_resized) {
                // В обычном режиме (не перетаскивание/масштабирование) используем сохраненную позицию
                ImGui::SetNextWindowPos(rag_state.position, ImGuiCond_Always);
                ImGui::SetNextWindowSize(rag_state.size, ImGuiCond_Always);
            } else {
                // Во время перетаскивания/масштабирования не устанавливаем позицию/размер явно
                ImGui::SetNextWindowPos(rag_state.position, ImGuiCond_Once);
                ImGui::SetNextWindowSize(rag_state.size, ImGuiCond_Once);
            }
        }

        rag_interface_->render_ui(&show_rag);
    }
    // Получаем фактическую позицию окна и сохраняем её в window_manager
    if (ImGuiWindow* win = ImGui::FindWindowByName("RAG")) {
        bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();
        // Обновляем позицию всегда, чтобы отслеживать перемещение в реальном времени
        if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
            // Применяем примагничивание только после отпускания мыши
            ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(win->Pos);
            ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(win->Size);
            window_manager_.updateWindowPosition("rag", snapped_pos, false);
            window_manager_.updateWindowSize("rag", snapped_size, false);
        } else if (!is_window_being_resized) {
            // Во время перетаскивания обновляем позицию без примагничивания
            // Но во время масштабирования НЕ обновляем размер, чтобы избежать неконтролируемого роста
            window_manager_.updateWindowPosition("rag", win->Pos, true);
            window_manager_.updateWindowSize("rag", win->Size, true);
        }
    }

    // Agents окно - ОТКЛЮЧЕНО: агенты временно отключены
    // bool show_agents = window_manager_.isWindowVisible("agents");
    // if (agent_chat_integration_ && show_agents) {
    //     // Устанавливаем позицию и размер перед открытием окна
    //     auto agents_state = window_manager_.getWindowState("agents");
    //     bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();

    //     if (agents_state.position.x != 0 || agents_state.position.y != 0) {
    //         if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
    //             ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(agents_state.position);
    //             ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(agents_state.size);
    //             ImGui::SetNextWindowPos(snapped_pos, ImGuiCond_Always);
    //             ImGui::SetNextWindowSize(snapped_size, ImGuiCond_Always);
    //         } else if (snap_enabled && !is_window_being_dragged && !is_window_being_resized) {
    //             // В обычном режиме (не перетаскивание/масштабирование) используем сохраненную позицию
    //             ImGui::SetNextWindowPos(agents_state.position, ImGuiCond_Always);
    //             ImGui::SetNextWindowSize(agents_state.size, ImGuiCond_Always);
    //         } else {
    //             // Во время перетаскивания/масштабирования не устанавливаем позицию/размер явно
    //             ImGui::SetNextWindowPos(agents_state.position, ImGuiCond_Once);
    //             ImGui::SetNextWindowSize(agents_state.size, ImGuiCond_Once);
    //         }
    //     }

    //     agent_chat_integration_->get_agent_panel()->render(&show_agents);
    // }
    // // Получаем фактическую позицию окна и сохраняем её в window_manager
    // if (ImGuiWindow* win = ImGui::FindWindowByName(TR("agents.title"))) {
    //     bool snap_enabled = window_manager_.getGridSnappingSystem().isEnabled();
    //     // Обновляем позицию всегда, чтобы отслеживать перемещение в реальном времени
    //     if (snap_enabled && mouse_released && !is_window_being_dragged && !is_window_being_resized) {
    //         // Применяем примагничивание только после отпускания мыши
    //         ImVec2 snapped_pos = window_manager_.getGridSnappingSystem().snapPosition(win->Pos);
    //         ImVec2 snapped_size = window_manager_.getGridSnappingSystem().snapSize(win->Size);
    //         window_manager_.updateWindowPosition("agents", snapped_pos, false);
    //         window_manager_.updateWindowSize("agents", snapped_size, false);
    //     } else if (!is_window_being_resized) {
    //         // Во время перетаскивания обновляем позицию без примагничивания
    //         // Но во время масштабирования НЕ обновляем размер, чтобы избежать неконтролируемого роста
    //         window_manager_.updateWindowPosition("agents", win->Pos, true);
    //         window_manager_.updateWindowSize("agents", win->Size, true);
    //     }
    // }

    // Сбрасываем флаг после первого кадра с примененными позициями
    if (workspace_just_loaded_) {
        workspace_just_loaded_ = false;
    }

    // Синхронизация состояния видимости с window_manager_
    if (!show_conversations && window_manager_.isWindowVisible("conversations")) {
        window_manager_.setWindowVisible("conversations", false);
    }
    if (!show_files && window_manager_.isWindowVisible("files")) {
        window_manager_.setWindowVisible("files", false);
    }
    if (!show_chat && window_manager_.isWindowVisible("chat")) {
        window_manager_.setWindowVisible("chat", false);
    }
    if (!show_rag && window_manager_.isWindowVisible("rag")) {
        window_manager_.setWindowVisible("rag", false);
    }
    // if (!show_agents && window_manager_.isWindowVisible("agents")) {
    //     window_manager_.setWindowVisible("agents", false);
    // }
}

void MainWindow::render_status_bar() {
    if (!show_status_bar_) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    
    // Позиция: внизу экрана
    ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x, viewport->Pos.y + viewport->Size.y - 24));
    ImGui::SetNextWindowSize(ImVec2(viewport->Size.x, 24));

    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 1.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));

    // Рендерим статус-бар
    if (ImGui::Begin("StatusBar", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoBringToFrontOnFocus)) {

        // Левая часть: статус
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Llama GUI");
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();

        // Статус сервера
        if (server_manager_) {
            auto status = server_manager_->get_server_status();
            const char* status_icon = (status == "running") ? "[OK]" : "[STOP]";
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s Server: %s", status_icon, status.c_str());
        } else {
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[STOP] Server: N/A");
        }

        // Правая часть: FPS
        ImGui::SameLine();
        ImGui::Separator();
        ImGui::SameLine();
        ImGui::SetCursorPosX(viewport->Pos.x + viewport->Size.x - 100);
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "FPS: %.1f", ImGui::GetIO().Framerate);
    }
    ImGui::End();

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor();
}

void MainWindow::render_error_dialogs() {
    if (!error_queue_.empty()) {
        auto [title, message] = error_queue_[0];
        error_queue_.erase(error_queue_.begin());
        show_error(title, message);
    }
}

void MainWindow::render_model_selection_dialog() {
    if (ImGui::BeginPopupModal("Confirm Model Reload", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("A different model is currently loaded.");
        ImGui::Text("Do you want to reload the server with the new model?");
        ImGui::Text("Current model: %s", model_manager_->get_current_model().c_str());
        ImGui::Text("New model: %s", pending_model_path_.c_str());
        ImGui::Separator();

        ImVec2 button_size(120.0f, 0.0f);
        if (ImGui::Button("Yes, Reload Server", button_size)) {
            ImGui::CloseCurrentPopup();
            load_model_and_restart_server(pending_model_path_);
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", button_size)) {
            ImGui::CloseCurrentPopup();
            show_info("Model Selection", "Model selection cancelled");
        }

        ImGui::EndPopup();
    }
}

void MainWindow::render_settings_dialog() {
    if (settings_dialog_) {
        settings_dialog_->render();
    }
}

void MainWindow::render_rag_settings_dialog() {
    if (rag_settings_dialog_) {
        rag_settings_dialog_->render();
        // Обработка диалога выбора файла модели эмбеддингов
        rag_settings_dialog_->open_embedding_model_file_dialog();
    }
}

void MainWindow::render_embedding_settings_dialog() {
    if (embedding_settings_dialog_) {
        embedding_settings_dialog_->render();
    }
}

void MainWindow::render_cloud_services_dialog() {
    if (cloud_services_dialog_) {
        cloud_services_dialog_->render();
    }
}

void MainWindow::render_settings_viewer_dialog() {
    if (settings_viewer_dialog_) {
        settings_viewer_dialog_->render();
    }
}

void MainWindow::render_grid_snapping_dialog() {
    if (grid_snapping_dialog_) {
        bool show = grid_snapping_dialog_->isVisible();
        grid_snapping_dialog_->render(&show);
        if (!show) {
            grid_snapping_dialog_->hide();
        }
    }
}

void MainWindow::render_profile_manager_dialog() {
    if (profile_manager_dialog_) {
        profile_manager_dialog_->render();
    }
}

void MainWindow::render_backup_manager_dialog() {
    if (backup_manager_dialog_) {
        backup_manager_dialog_->render();
    }
}

#ifdef ENABLE_LLAMA_BENCH
void MainWindow::render_llama_bench_dialog() {
    if (llama_bench_dialog_) {
        llama_bench_dialog_->render();
    }
}
#endif

void MainWindow::setup_window_layout() {}
void MainWindow::save_window_layout() {}
void MainWindow::load_window_layout() {}
void MainWindow::apply_theme() {}
void MainWindow::render_custom_styling() {}
void MainWindow::update_performance_metrics() {}
void MainWindow::render_performance_overlay() {}

} // namespace ui
} // namespace llama_gui
