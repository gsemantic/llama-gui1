#include "../include/ui/chat_interface.h"
#include "../include/core/state_manager.h"
#include "../include/ui/localization_manager.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <iostream>
#include <chrono>

namespace llama_gui {
namespace ui {

void ChatInterface::render(bool* visible) {
    // Стандартное окно ImGui с заголовком и кнопками управления
    // Кнопка закрытия (×) и сворачивания (─) рисуются автоматически ImGui
    if (!ImGui::Begin("Chat", visible, ImGuiWindowFlags_None)) {
        ImGui::End();
        return;
    }

    // Обработка результатов асинхронной RAG-обработки
    process_rag_queue();

    process_pending_responses();

    static int frame_counter = 0;
    if (++frame_counter % 200 == 0) {
        cleanup_old_cache_entries();
    }

    static bool show_cache_stats = true;  // Показывать статистику кэша
    if (show_cache_stats) {
        render_cache_stats(&show_cache_stats);
    }

    static float last_scroll_y = 0.0f;
    float current_scroll_y = ImGui::GetScrollY();
    float max_scroll_y = ImGui::GetScrollMaxY();

    if (last_scroll_y > 0 && current_scroll_y < last_scroll_y - 50.0f) {
        auto_scroll_ = false;
    }

    if (max_scroll_y > 0 && current_scroll_y >= max_scroll_y - 10.0f) {
        auto_scroll_ = true;
    }

    last_scroll_y = current_scroll_y;

    render_message_list();
    render_input_area();
    ImGui::End();
}

void ChatInterface::render_message_list() {
    float input_area_height = 120.0f;  // Уменьшили с 155 до 120 для большего места сообщениям
    float available_height = ImGui::GetContentRegionAvail().y - input_area_height;
    if (available_height < 150.0f) available_height = 150.0f;  // Увеличили мин. высоту с 100 до 150

    ImGui::BeginChild("MessagesScroll", ImVec2(0, available_height), true);

    std::vector<llama_gui::core::Conversation*> all_conversations = state_manager_.get_all_conversations();
    const llama_gui::core::Conversation* active_conversation = nullptr;
    for (llama_gui::core::Conversation* conv : all_conversations) {
        if (conv->is_active) {
            active_conversation = conv;
            break;
        }
    }

    if (!active_conversation) {
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", TR("chat.no_active_conversation"));
        ImGui::EndChild();
        return;
    }

    std::string formatted_title = TRF("chat.title", "Conversation: %s");
    size_t pos = formatted_title.find("%s");
    if (pos != std::string::npos) {
        formatted_title.replace(pos, 2, active_conversation->title);
    }
    ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f), "%s", formatted_title.c_str());
    ImGui::Separator();
    ImGui::Spacing();

    static const ImVec4 USER_COLOR(0.0f, 0.5f, 1.0f, 1.0f);
    static const ImVec4 ASSISTANT_COLOR(0.0f, 0.8f, 0.0f, 1.0f);
    static const ImVec4 SYSTEM_COLOR(0.8f, 0.8f, 0.0f, 1.0f);
    static const ImVec4 DEFAULT_COLOR(0.0f, 0.0f, 0.0f, 1.0f);

    static std::string cached_user_role, cached_assistant_role, cached_system_role;
    static bool roles_cached = false;

    for (size_t i = 0; i < active_conversation->messages.size(); ++i) {
        const auto& message = active_conversation->messages[i];

        const ImVec4& message_color = (message.role == "user") ? USER_COLOR :
                                      (message.role == "assistant") ? ASSISTANT_COLOR :
                                      (message.role == "system") ? SYSTEM_COLOR : DEFAULT_COLOR;

        ImGui::BeginGroup();

        if (!roles_cached) {
            cached_user_role = TRF("message.user", "User");
            cached_assistant_role = TRF("message.assistant", "Assistant");
            cached_system_role = TRF("message.system", "System");
            roles_cached = true;
        }

        const std::string& role_text = (message.role == "user") ? cached_user_role :
                                       (message.role == "assistant") ? cached_assistant_role :
                                       (message.role == "system") ? cached_system_role : message.role;

        ImGui::TextColored(message_color, "%s:", role_text.c_str());
        ImGui::SameLine();

        // Рендеринг текста сообщения БЕЗ прокрутки (как в веб-чате)
        // Используем Text с PushTextWrapPos для переноса строк
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(message.content.c_str());
        ImGui::PopTextWrapPos();

        // Контекстное меню для копирования текста
        if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            ImGui::OpenPopup(("copy_menu_" + std::to_string(i)).c_str());
        }

        if (ImGui::BeginPopup(("copy_menu_" + std::to_string(i)).c_str())) {
            // Копировать всё сообщение
            if (ImGui::MenuItem(TRF("chat.copy_message", "Copy Entire Message"))) {
                ImGui::SetClipboardText(message.content.c_str());
            }
            ImGui::EndPopup();
        }

        ImGui::EndGroup();
        ImGui::Spacing();
        ImGui::Spacing();
    }

    if (auto_scroll_) {
        // Всегда прокручивать к последнему сообщению при стриминге
        if (streaming_active_) {
            ImGui::SetScrollHereY(1.0f);  // 1.0f = прокрутка к низу
        } else {
            // Проверка: если пользователь не скроллил вручную
            float scroll_position = ImGui::GetScrollY();
            float max_scroll = ImGui::GetScrollMaxY();
            
            if (max_scroll <= 0.0f || scroll_position >= max_scroll - 100.0f) {
                ImGui::SetScrollHereY(1.0f);
            }
        }
    }

    // Performance metrics removed from message list - now displayed below input area

    if (streaming_active_) {
        ImGui::BeginGroup();

        if (current_stream_content_.empty()) {
            // Запрос отправлен, но генерация ещё не началась - показываем "Обрабатываю запрос..."
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", TRF("message.processing", "Processing request..."));
            ImGui::SameLine();

            static float dots_time = 0.0f;
            dots_time += ImGui::GetIO().DeltaTime;
            int dots = static_cast<int>(dots_time * 2.0f) % 3;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%.*s", dots + 1, "...");
        } else {
            // Генерация началась - показываем "Ассистент (генерирует):" и контент
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", TRF("message.generating", "Assistant (generating):"));
            ImGui::SameLine();

            static float dots_time = 0.0f;
            dots_time += ImGui::GetIO().DeltaTime;
            int dots = static_cast<int>(dots_time * 2.0f) % 3;
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "%.*s", dots + 1, "...");

            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - 10.0f);

            const size_t max_display_length = 10000;
            std::string display_content = current_stream_content_;
            if (display_content.length() > max_display_length) {
                display_content = display_content.substr(0, max_display_length) + "...";
            }

            ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%s", display_content.c_str());
            ImGui::PopTextWrapPos();
        }

        ImGui::EndGroup();

        // Принудительная прокрутка вниз при каждом кадре стриминга
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void ChatInterface::render_typing_indicator() {
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Assistant: ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.0f, 0.8f, 0.0f, 1.0f), "%s", current_stream_content_.c_str());
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "█");
}

void ChatInterface::render_performance_metrics() {
    // Render metrics in a single compact line
    if (current_metrics_.is_measuring) {
        ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.8f, 1.0f),
                          TRF("performance.generating_short", "Generating: %d tok | %.1f tok/s | %ds | Context: %d/%d"),
                          current_metrics_.tokens_generated,
                          current_metrics_.tokens_per_second,
                          static_cast<int>(current_metrics_.response_time_seconds),
                          current_metrics_.context_used + current_metrics_.tokens_generated,
                          current_metrics_.total_context);
    } else if (current_metrics_.response_time_seconds > 0) {
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f),
                          TRF("performance.completed_short", "✓ %d tok | %.1f tok/s | %ds | Context: %d/%d"),
                          current_metrics_.tokens_generated,
                          current_metrics_.tokens_per_second,
                          static_cast<int>(current_metrics_.response_time_seconds),
                          current_metrics_.context_used,
                          current_metrics_.total_context);
    }
}

void ChatInterface::render_cache_stats(bool* visible) {
    if (!visible || !*visible) return;
    
    // Показываем статистику только если были запросы
    if (cache_stats_.total_requests == 0) return;
    
    ImGui::Separator();
    
    // Заголовок с кнопкой закрытия
    if (ImGui::Button("×", ImVec2(20, 0))) {
        *visible = false;
    }
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), "Cache Statistics");
    
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(50, 0))) {
        reset_cache_stats();
    }
    
    // Основная статистика
    ImGui::Separator();
    
    // Процент попаданий в кэш
    double hit_rate = cache_stats_.get_cache_hit_rate();
    ImGui::Text("Cache Hit Rate: %.1f%%", hit_rate);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Percentage of requests served from cache");
    }
    
    // Экономия токенов
    double savings_rate = cache_stats_.get_token_savings_rate();
    ImGui::Text("Token Savings: %.1f%%", savings_rate);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Percentage of tokens saved by caching");
    }
    
    // Детальная статистика
    ImGui::Separator();
    ImGui::Text("Total Requests: %d", cache_stats_.total_requests);
    ImGui::SameLine();
    ImGui::Text(" | Prompt Cache: %d", cache_stats_.prompt_cache_hits);
    ImGui::SameLine();
    ImGui::Text(" | RAG Cache: %d", cache_stats_.rag_cache_hits);
    
    ImGui::Text("Tokens Generated: %d", cache_stats_.total_tokens_generated);
    ImGui::SameLine();
    ImGui::Text(" | Saved: %d", cache_stats_.total_tokens_saved);
    
    // Время сессии
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::minutes>(now - cache_stats_.session_start);
    ImGui::Text("Session Duration: %d min", static_cast<int>(duration.count()));
    
    ImGui::Separator();
}

void ChatInterface::scroll_to_bottom() {
    ImGui::SetScrollHereY(1.0f);
    ImGui::SetScrollHereY(1.0f);
}

} // namespace ui
} // namespace llama_gui
