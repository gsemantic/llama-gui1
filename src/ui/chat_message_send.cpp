#include "../include/ui/chat_interface.h"
#include "../include/core/state_manager.h"
#include "../include/core/model_manager.h"
#include "../include/core/logger.h"
#include "../include/ui/localization_manager.h"
#include "../include/core/openrouter_client.h"
#include "../include/core/kilocode_client.h"
#include "imgui.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <algorithm>
#include <thread>
#include <future>

namespace llama_gui {
namespace ui {

void ChatInterface::send_message() {
    // Проверка: если включен KiloCode - используем его (приоритет над OpenRouter)
    if (settings_.kilocode().enabled && !settings_.kilocode().selected_model.empty()) {
        send_message_via_kilocode();
        return;
    }

    // Проверка: если включен OpenRouter - используем его
    if (settings_.openrouter().enabled && !settings_.openrouter().selected_model.empty()) {
        send_message_via_openrouter();
        return;
    }

    // OPTIMIZATION: Work directly with input_buffer_ to avoid unnecessary copies
    // Clean in-place by modifying input_buffer_ directly
    std::string cleaned_content;
    cleaned_content.reserve(strlen(input_buffer_)); // Pre-allocate memory

    for (size_t i = 0; i < strlen(input_buffer_); i++) {
        char c = input_buffer_[i];
        unsigned char uc = static_cast<unsigned char>(c);

        // Разрешаем все печатные символы, включая UTF-8
        if (uc >= 32 || uc == '\n' || uc == '\r' || uc == '\t') {
            cleaned_content += c;
        }
        // Пропускаем нулевые символы и другие проблемные
    }

    // OPTIMIZATION: Use swap instead of assignment to avoid copy
    std::string message_content;
    message_content.swap(cleaned_content);

    // Additional safety check: ensure no control characters remain at the beginning
    size_t start_pos = 0;
    while (start_pos < message_content.length()) {
        unsigned char first_char = static_cast<unsigned char>(message_content[start_pos]);
        if (first_char < 32 || first_char == 127) {
            start_pos++;
        } else {
            break;
        }
    }
    if (start_pos > 0) {
        message_content = message_content.substr(start_pos);
    }

    // Final validation: if message is empty after cleaning, return early
    if (message_content.empty()) {
        std::cerr << "WARNING: Message content is empty after cleaning" << std::endl;
        return;
    }

    // Fix UTF-8 encoding issues
    try {
        bool needs_conversion = false;
        for (char c : message_content) {
            if (static_cast<unsigned char>(c) > 127) {
                needs_conversion = true;
                break;
            }
        }

        if (needs_conversion) {
            std::string validated = validate_and_fix_utf8(message_content);

            if (validated.empty() || validated.find_first_not_of(' ') == std::string::npos) {
                // Try to detect and convert from Windows-1251 (common for Cyrillic)
                std::string temp_content;
                temp_content.reserve(strlen(input_buffer_));

                for (char c : input_buffer_) {
                    unsigned char uc = static_cast<unsigned char>(c);
                    if (uc >= 0xC0 && uc <= 0xFF) {
                        static const unsigned char win1251_to_utf8[][2] = {
                            {0xD0, 0x90}, {0xD0, 0x91}, {0xD0, 0x92}, {0xD0, 0x93}, {0xD0, 0x94}, {0xD0, 0x95}, {0xD0, 0x96}, {0xD0, 0x97},
                            {0xD0, 0x98}, {0xD0, 0x99}, {0xD0, 0x9A}, {0xD0, 0x9B}, {0xD0, 0x9C}, {0xD0, 0x9D}, {0xD0, 0x9E}, {0xD0, 0x9F},
                            {0xD0, 0xA0}, {0xD0, 0xA1}, {0xD0, 0xA2}, {0xD0, 0xA3}, {0xD0, 0xA4}, {0xD0, 0xA5}, {0xD0, 0xA6}, {0xD0, 0xA7},
                            {0xD0, 0xA8}, {0xD0, 0xA9}, {0xD0, 0xAA}, {0xD0, 0xAB}, {0xD0, 0xAC}, {0xD0, 0xAD}, {0xD0, 0xAE}, {0xD0, 0xAF},
                            {0xD0, 0xB0}, {0xD0, 0xB1}, {0xD0, 0xB2}, {0xD0, 0xB3}, {0xD0, 0xB4}, {0xD0, 0xB5}, {0xD0, 0xB6}, {0xD0, 0xB7},
                            {0xD0, 0xB8}, {0xD0, 0xB9}, {0xD0, 0xBA}, {0xD0, 0xBB}, {0xD0, 0xBC}, {0xD0, 0xBD}, {0xD0, 0xBE}, {0xD0, 0xBF},
                            {0xD1, 0x80}, {0xD1, 0x81}, {0xD1, 0x82}, {0xD1, 0x83}, {0xD1, 0x84}, {0xD1, 0x85}, {0xD1, 0x86}, {0xD1, 0x87},
                            {0xD1, 0x88}, {0xD1, 0x89}, {0xD1, 0x8A}, {0xD1, 0x8B}, {0xD1, 0x8C}, {0xD1, 0x8D}, {0xD1, 0x8E}, {0xD1, 0x8F}
                        };

                        if (uc >= 0xC0 && uc <= 0xFF) {
                            size_t idx = uc - 0xC0;
                            if (idx < sizeof(win1251_to_utf8)/sizeof(win1251_to_utf8[0])) {
                                temp_content += static_cast<char>(win1251_to_utf8[idx][0]);
                                temp_content += static_cast<char>(win1251_to_utf8[idx][1]);
                            } else {
                                temp_content += '?';
                            }
                        } else {
                            temp_content += c;
                        }
                    } else {
                        temp_content += c;
                    }
                }
                message_content.swap(temp_content);
            } else {
                message_content.swap(validated);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "UTF-8 conversion error: " << e.what() << std::endl;
    }

    // Efficient trim
    size_t start = message_content.find_first_not_of(" \n\r\t");
    size_t end = message_content.find_last_not_of(" \n\r\t");

    if (start == std::string::npos) {
        return;
    }

    if (start > 0 || end < message_content.length() - 1) {
        message_content = message_content.substr(start, end - start + 1);
    }

    if (message_content.empty()) {
        return;
    }

    // ПРОВЕРКА: Загружена ли модель?
    if (!model_manager_ || !model_manager_->is_model_loaded()) {
        std::cout << "MainWindow: Модель не загружена, показываем диалог загрузки" << std::endl;

        if (model_load_request_callback_) {
            model_load_request_callback_(message_content);
        } else {
            std::cerr << "Ошибка: Модель не загружена. Пожалуйста, выберите модель в настройках." << std::endl;
        }
        return;
    }

    // ПРОВЕРКА: Идет ли загрузка модели?
    if (is_model_loading()) {
        std::cout << "MainWindow: Модель загружается, ждем завершения..." << std::endl;
        pending_query_for_loading_ = message_content;
        return;
    }

    // Get active conversation or create new one
    std::vector<llama_gui::core::Conversation*> all_conversations = state_manager_.get_all_conversations();
    llama_gui::core::Conversation* active_conv = nullptr;

    for (llama_gui::core::Conversation* conv : all_conversations) {
        if (conv->is_active) {
            active_conv = conv;
            break;
        }
    }

    if (!active_conv) {
        std::string new_conv_id = state_manager_.create_conversation("New Chat");
        active_conv = state_manager_.get_conversation(new_conv_id);
        if (!active_conv) {
            std::cerr << "Failed to create new conversation after retry" << std::endl;
            return;
        }
        state_manager_.set_active_conversation(new_conv_id);
    }

    // Проверка чередования ролей: если последнее сообщение от assistant с ошибкой, удаляем его
    if (!active_conv->messages.empty()) {
        const auto& last_msg = active_conv->messages.back();
        if (last_msg.role == "assistant" && 
            (last_msg.content.find("Error:") == 0 || last_msg.content.find("[Generation stopped") != std::string::npos)) {
            // Удаляем сообщение об ошибке или прерывании
            active_conv->messages.pop_back();
            invalidate_cache_for_conversation(active_conv->id);
        }
    }

    // Проверка: если последнее сообщение от user, добавляем assistant ответ-заглушку
    if (!active_conv->messages.empty() && active_conv->messages.back().role == "user") {
        // Последнее сообщение от пользователя, но нет ответа от ассистента
        // Это может произойти после ошибки сервера — добавим пустой ответ
        llama_gui::core::Message placeholder_msg("assistant", "[No response generated]");
        state_manager_.add_message(active_conv->id, placeholder_msg);
        invalidate_cache_for_conversation(active_conv->id);
    }

    // Проверка кэш-истории
    bool use_cache = (settings_.rag().rag_mode == llama_gui::core::RagMode::CacheOnly ||
                      settings_.rag().rag_mode == llama_gui::core::RagMode::Both);

    if (use_cache && rag_enabled_ && rag_manager_ && settings_.rag().enable_rag && settings_.rag().enable_caching) {
        std::string cached_response = rag_manager_->find_cached_response(message_content);

        if (!cached_response.empty()) {
            llama_gui::core::Message user_msg("user", message_content);
            state_manager_.add_message(active_conv->id, user_msg);

            llama_gui::core::Message assistant_msg("assistant", cached_response);
            state_manager_.add_message(active_conv->id, assistant_msg);

            if (active_conv->messages.size() == 2 &&
                (active_conv->title == "New Chat" || active_conv->title.empty())) {
                std::string title = generate_conversation_title(message_content);
                active_conv->title = title;
            }

            cache_current_interaction(message_content, cached_response);
            invalidate_cache_for_conversation(active_conv->id);
            clear_input();
            
            // Статистика: RAG cache hit
            cache_stats_.total_requests++;
            cache_stats_.rag_cache_hits++;
            cache_stats_.total_tokens_saved += static_cast<int>(cached_response.length() / 4);
            
            return;
        }
    }
    
    // Prompt Caching - проверка идентичных запросов
    {
        std::string cached_response;
        if (check_prompt_cache(message_content, cached_response)) {
            // Найдено в prompt cache!
            llama_gui::core::Message user_msg("user", message_content);
            state_manager_.add_message(active_conv->id, user_msg);

            llama_gui::core::Message assistant_msg("assistant", cached_response);
            state_manager_.add_message(active_conv->id, assistant_msg);

            if (active_conv->messages.size() == 2 &&
                (active_conv->title == "New Chat" || active_conv->title.empty())) {
                std::string title = generate_conversation_title(message_content);
                active_conv->title = title;
            }

            invalidate_cache_for_conversation(active_conv->id);
            clear_input();
            
            // Статистика уже обновлена в check_prompt_cache, но нужно добавить saved tokens
            cache_stats_.total_tokens_saved += static_cast<int>(cached_response.length() / 4);
            
            return;
        }
    }

    // RAG обработка - АСИНХРОННАЯ
    std::string final_message_content = message_content;
    bool has_rag_context = false;
    bool use_documents = (settings_.rag().rag_mode == llama_gui::core::RagMode::DocumentsOnly ||
                          settings_.rag().rag_mode == llama_gui::core::RagMode::Both);

    // Проверяем, есть ли уже готовый результат от асинхронной RAG-обработки
    process_rag_queue();  // Обрабатываем результат если готов
    
    if (use_documents && rag_enabled_ && rag_manager_ && settings_.rag().enable_rag) {
        // Отправляем запрос на асинхронную обработку
        submit_rag_request(message_content);
        processing_rag_document_ = true;

        // Ждем результат в главном цикле (через process_rag_queue)
        // Для простоты пока используем синхронный режим
        // TODO: Полная асинхронность требует рефакторинга send_message
        std::cout << "[RAG ASYNC] Note: Using synchronous mode for now" << std::endl;

        try {
            std::cout << "[RAG LOCAL] Calling process_with_rag..." << std::endl;
            llama_gui::core::Logger::instance().info("RAG: BEFORE process_with_rag");
            llama_gui::core::Logger::instance().flush_file_log();
            
            std::string rag_prompt = process_with_rag(message_content);
            
            std::cout << "[RAG LOCAL] process_with_rag returned, length: " << rag_prompt.size() << std::endl;
            llama_gui::core::Logger::instance().info("RAG: AFTER process_with_rag, length=" + std::to_string(rag_prompt.size()));
            llama_gui::core::Logger::instance().flush_file_log();
            
            if (llama_gui::core::Logger::instance().is_debug_mode()) {
                std::cout << "[DEBUG] RAG: rag_prompt != message_content: " << (rag_prompt != message_content ? "YES" : "NO") << std::endl;
                std::cout << "[DEBUG] RAG: message_content length: " << message_content.size() << ", rag_prompt length: " << rag_prompt.size() << std::endl;
            }
            if (rag_prompt != message_content) {
                final_message_content = rag_prompt;
                has_rag_context = true;
                if (llama_gui::core::Logger::instance().is_debug_mode()) {
                    std::cout << "[DEBUG] RAG: has_rag_context = TRUE" << std::endl;
                }
            } else {
                processing_rag_document_ = false;
                if (llama_gui::core::Logger::instance().is_debug_mode()) {
                    std::cout << "[DEBUG] RAG: has_rag_context = FALSE (no change)" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "[RAG LOCAL ERROR] Exception in process_with_rag: " << e.what() << std::endl;
            std::cerr << "[RAG LOCAL ERROR] Falling back to original message" << std::endl;
            processing_rag_document_ = false;
            // Продолжаем с оригинальным сообщением
        } catch (...) {
            std::cerr << "[RAG LOCAL ERROR] Unknown exception in process_with_rag" << std::endl;
            std::cerr << "[RAG LOCAL ERROR] Falling back to original message" << std::endl;
            processing_rag_document_ = false;
            // Продолжаем с оригинальным сообщением
        }
    }

    // === ДОБАВЛЕНИЕ СОДЕРЖИМОГО ПРИКРЕПЛЁННЫХ ФАЙЛОВ (если RAG не нашёл контекст или выключен) ===
    bool has_attachments = !attachments_.empty() && !attachment_contents_.empty();
    std::string attachments_context;
    
    if (has_attachments && !has_rag_context) {
        // RAG выключен или не нашёл документов - добавляем содержимое файлов напрямую
        attachments_context = build_attachments_context();
        if (!attachments_context.empty()) {
            final_message_content = message_content + attachments_context;
            std::cout << "[ATTACHMENT] Adding " << attachments_context.size() 
                      << " bytes of file content to message (RAG not used)" << std::endl;
        }
    } else if (has_attachments && has_rag_context) {
        std::cout << "[ATTACHMENT] RAG context already provided, skipping direct file content" << std::endl;
    }
    // ==================================================================================================

    // Add user message
    llama_gui::core::Message user_msg("user", message_content);
    state_manager_.add_message(active_conv->id, user_msg);

    // Переименование
    if (active_conv->messages.size() == 1 &&
        (active_conv->title == "New Chat" || active_conv->title.empty())) {
        std::string title = generate_conversation_title(message_content);
        active_conv->title = title;
    }

    invalidate_cache_for_conversation(active_conv->id);
    clear_input();

    // Prepare messages for API call
    std::vector<llama_gui::core::ChatMessage> messages;
    messages.reserve(12);

    // Добавляем системный промпт для RAG (если включен)
    // Это помогает модели правильно использовать контекст
    if (has_rag_context) {
        std::string rag_system_prompt = "Ты - полезный ассистент с доступом к внешним документам. "
                                        "Используй предоставленный контекст для ответа на вопрос. "
                                        "Если контекст не содержит нужной информации, скажи об этом. "
                                        "Отвечай кратко и по делу.";
        messages.push_back(llama_gui::core::ChatMessage(llama_gui::core::MessageRole::System, rag_system_prompt));
        if (llama_gui::core::Logger::instance().is_debug_mode()) {
            std::cout << "[DEBUG] RAG: Added system prompt for RAG context" << std::endl;
        }
    }

    const size_t max_messages = 10;
    size_t start_idx = (active_conv->messages.size() > max_messages) ?
                       (active_conv->messages.size() - max_messages) : 0;

    for (size_t i = start_idx; i < active_conv->messages.size(); ++i) {
        const auto& msg = active_conv->messages[i];
        if (msg.content.find("Error:") == std::string::npos) {
            llama_gui::core::MessageRole role_enum = llama_gui::core::MessageRole::User;
            if (msg.role == "assistant") {
                role_enum = llama_gui::core::MessageRole::Assistant;
            } else if (msg.role == "system") {
                role_enum = llama_gui::core::MessageRole::System;
            }

            std::string content_to_send = msg.content;
            // Модифицируем последнее user-сообщение, добавляя RAG-контекст ИЛИ содержимое файлов
            bool is_last_user_message = (i == active_conv->messages.size() - 1 && msg.role == "user");
            
            if (has_rag_context && is_last_user_message) {
                content_to_send = final_message_content;
                if (llama_gui::core::Logger::instance().is_debug_mode()) {
                    std::cout << "[DEBUG] MSG: Applying RAG context to last user message, new length: " << content_to_send.size() << std::endl;
                }
            } else if (has_attachments && !has_rag_context && is_last_user_message) {
                // RAG выключен, но есть вложения - добавляем содержимое файлов
                content_to_send = final_message_content;
                std::cout << "[ATTACHMENT] Sending message with attached file content, total size: " 
                          << content_to_send.size() << " bytes" << std::endl;
            }

            if (llama_gui::core::Logger::instance().is_debug_mode()) {
                std::cout << "[DEBUG] MSG[" << i << "]: role=" << msg.role << ", content_length=" << content_to_send.size() << std::endl;
            }
            messages.push_back(llama_gui::core::ChatMessage(role_enum, content_to_send));
        }
    }

    // Create chat completion request
    llama_gui::core::ChatCompletionRequest request;

    std::string model_path = settings_.get_model_path();
    size_t last_slash = model_path.find_last_of("/\\");
    std::string filename = (last_slash == std::string::npos) ? model_path : model_path.substr(last_slash + 1);
    size_t last_dot = filename.find_last_of(".");
    request.model = (last_dot == std::string::npos) ? filename : filename.substr(0, last_dot);

    request.messages = messages;
    
    // === ЗАЩИТА ОТ ПЕРЕПОЛНЕНИЯ КОНТЕКСТНОГО ОКНА ПРИ RAG ===
    // Вычисляем примерный размер промпта в токенах
    size_t total_prompt_chars = 0;
    for (const auto& msg : messages) {
        total_prompt_chars += msg.content.size();
    }
    // Грубая оценка: 1 токен ≈ 4 символа (для русского/английского)
    int estimated_prompt_tokens = static_cast<int>(total_prompt_chars / 4);
    
    // Получаем текущий размер контекста
    int ctx_size = settings_.batch().ctx_size;
    int max_tokens_setting = settings_.chat().max_tokens;
    
    // Рассчитываем безопасный max_tokens
    // Оставляем 20% запаса для KV-cache и служебных токенов
    int context_reserve = std::max(512, static_cast<int>(ctx_size * 0.20f));
    int max_allowed_tokens = ctx_size - estimated_prompt_tokens - context_reserve;
    
    // Если запрос слишком большой, уменьшаем max_tokens
    if (max_tokens_setting > max_allowed_tokens && max_allowed_tokens > 0) {
        request.max_tokens = std::max(128, max_allowed_tokens);  // Минимум 128 токенов для ответа
        std::cout << "[RAG PROTECTION] Reducing max_tokens from " << max_tokens_setting 
                  << " to " << request.max_tokens 
                  << " (prompt ~" << estimated_prompt_tokens 
                  << " tokens, ctx_size=" << ctx_size << ")" << std::endl;
    } else if (max_allowed_tokens <= 0) {
        // Критическая ситуация - промпт уже превышает ctx_size
        request.max_tokens = 256;  // Минимальный ответ
        std::cerr << "[RAG PROTECTION] WARNING: Prompt size (" << estimated_prompt_tokens 
                  << " tokens) exceeds context size (" << ctx_size << ")!" << std::endl;
        std::cerr << "[RAG PROTECTION] Setting max_tokens to minimum (256)" << std::endl;
        
        // Показываем предупреждение пользователю через GUI
        // TODO: Добавить notification в GUI
    } else {
        request.max_tokens = max_tokens_setting;
    }
    
    // Дополнительная проверка для RAG режима
    if (has_rag_context && estimated_prompt_tokens > ctx_size / 2) {
        std::cout << "[RAG WARNING] Prompt uses " << (100 * estimated_prompt_tokens / ctx_size) 
                  << "% of context window. Consider increasing ctx_size in settings." << std::endl;
    }
    // ============================================================
    
    request.temperature = settings_.chat().temperature;
    request.top_p = settings_.chat().top_p;
    request.top_k = settings_.chat().top_k;
    request.min_p = settings_.chat().min_p;
    request.repeat_penalty = settings_.chat().repeat_penalty;
    request.presence_penalty = settings_.chat().presence_penalty;
    request.frequency_penalty = settings_.chat().frequency_penalty;
    request.mirostat_mode = settings_.chat().mirostat_mode;
    request.mirostat_tau = settings_.chat().mirostat_tau;
    request.mirostat_eta = settings_.chat().mirostat_eta;
    request.stop_on_newline = settings_.chat().stop_on_newline;
    request.stream = true;

    try {
        std::string conversation_id = active_conv->id;
        std::string original_prompt = message_content;  // Сохраняем для кэширования
        std::string rag_prompt_for_cache = final_message_content;  // Сохраняем полный RAG-промпт для кэширования
        start_streaming();

        if (processing_rag_document_) {
            processing_rag_document_ = false;
        }

        llama_interface_.create_chat_completion_streaming(request,
            [this, conversation_id, original_prompt, rag_prompt_for_cache, has_rag_context, ctx_size, estimated_prompt_tokens](const std::string& chunk, bool is_final) {
                try {
                    if (!chunk.empty()) {
                        std::string clean_chunk;

                        // === ОБРАБОТКА ОШИБОК СЕРВЕРА ===
                        // Проверяем, не вернул ли сервер ошибку
                        if (chunk.find("\"error\"") != std::string::npos) {
                            try {
                                auto json_error = nlohmann::json::parse(chunk);
                                if (json_error.contains("error")) {
                                    std::string error_msg = json_error["error"].get<std::string>();
                                    
                                    // Проверяем тип ошибки
                                    if (error_msg.find("context") != std::string::npos || 
                                        error_msg.find("n_ctx") != std::string::npos ||
                                        error_msg.find("exceed") != std::string::npos ||
                                        error_msg.find("too long") != std::string::npos) {
                                        
                                        std::cerr << "[RAG ERROR] Context size exceeded: " << error_msg << std::endl;
                                        std::string user_error = 
                                            "Ошибка: Превышен размер контекста.\n\n"
                                            "Текущий размер контекста: " + std::to_string(ctx_size) + " токенов\n"
                                            "Примерный размер промпта: " + std::to_string(estimated_prompt_tokens) + " токенов\n\n"
                                            "Рекомендации:\n"
                                            "1. Увеличьте 'ctx_size' в настройках сервера (минимум 8192 для RAG)\n"
                                            "2. Уменьшите длину сообщений в диалоге\n"
                                            "3. Очистите историю чата\n"
                                            "4. Уменьшите количество документов в RAG";
                                        
                                        std::lock_guard<std::mutex> lock(streaming_mutex_);
                                        pending_responses_.push_back({user_error, conversation_id});
                                        return;
                                    }
                                }
                            } catch (const std::exception&) {
                                // Если не удалось распарсить JSON, продолжаем обычную обработку
                            }
                        }
                        // ========================================

                        if (chunk.find("\"content\"") != std::string::npos) {
                            try {
                                auto json_chunk = nlohmann::json::parse(chunk);
                                if (json_chunk.contains("choices") && !json_chunk["choices"].empty()) {
                                    auto& choice = json_chunk["choices"][0];
                                    if (choice.contains("delta") && choice["delta"].contains("content")) {
                                        auto content = choice["delta"]["content"];
                                        if (!content.is_null()) {
                                            clean_chunk = content.get<std::string>();
                                        }
                                    } else if (choice.contains("message") && choice["message"].contains("content")) {
                                        auto content = choice["message"]["content"];
                                        if (!content.is_null()) {
                                            clean_chunk = content.get<std::string>();
                                        }
                                    }
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "JSON parse error: " << e.what() << std::endl;
                            }
                        }

                        if (!clean_chunk.empty()) {
                            std::lock_guard<std::mutex> lock(streaming_mutex_);
                            current_stream_content_ += clean_chunk;
                            update_performance_metrics(current_stream_content_, false);
                        }
                    }

                    if (is_final) {
                        std::string final_content;
                        {
                            std::lock_guard<std::mutex> lock(streaming_mutex_);
                            final_content = current_stream_content_;
                        }

                        update_performance_metrics(final_content, true);
                        pending_responses_.push_back({final_content, conversation_id});

                        // Prompt Caching: сохраняем в кэш
                        // Вычисляем приблизительное количество токенов (1 токен ≈ 4 символа)
                        int estimated_tokens = static_cast<int>(final_content.length() / 4);
                        
                        // Сохраняем в обычный кэш промптов
                        update_prompt_cache(original_prompt, final_content, estimated_tokens);
                        
                        // RAG Prompt Caching: если использовался RAG-контекст, сохраняем также в кэш RAG-промптов
                        if (has_rag_context && rag_prompt_for_cache != original_prompt) {
                            update_rag_prompt_cache(rag_prompt_for_cache, final_content, estimated_tokens);
                        }

                        // RAG Cache: сохраняем запрос-ответ в чат-историю для будущего кэширования
                        // Это позволяет находить похожие вопросы и возвращать кэшированный ответ
                        std::cout << "[RAG CACHE DEBUG] cache_current_interaction check: rag_enabled_=" << (rag_enabled_ ? "true" : "false")
                                  << ", settings_.rag().enable_caching=" << (settings_.rag().enable_caching ? "true" : "false") << std::endl;
                        if (rag_enabled_ && settings_.rag().enable_caching) {
                            cache_current_interaction(original_prompt, final_content);
                            std::cout << "[RAG CACHE] Interaction cached for future semantic matching" << std::endl;
                        } else {
                            std::cout << "[RAG CACHE] NOT caching interaction (disabled)" << std::endl;
                        }

                        // Статистика
                        cache_stats_.total_requests++;
                        cache_stats_.total_tokens_generated += estimated_tokens;

                        std::cout << "Streaming completed, content length: " << final_content.length() << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Error in streaming callback: " << e.what() << std::endl;
                    std::string error_msg = "Error: Streaming failed - " + std::string(e.what());
                    pending_responses_.push_back({error_msg, conversation_id});
                }
            });

    } catch (const std::exception& e) {
        std::cerr << "Error starting streaming operation: " << e.what() << std::endl;
        stop_streaming();

        llama_gui::core::Message error_msg("assistant", "Error: Failed to start streaming operation");
        state_manager_.add_message(active_conv->id, error_msg);
    }
}

void ChatInterface::clear_input() {
    input_buffer_[0] = '\0';
    ImGui::SetKeyboardFocusHere();
}

void ChatInterface::set_input_text(const std::string& text) {
    size_t max_len = sizeof(input_buffer_) - 1;
    size_t copy_len = text.length() < max_len ? text.length() : max_len;
    memcpy(input_buffer_, text.c_str(), copy_len);
    input_buffer_[copy_len] = '\0';
    input_focused_ = true;
}

void ChatInterface::open_model_selection_dialog() {
    std::cout << "ChatInterface: Opening model selection dialog" << std::endl;
    if (model_selection_callback_) {
        model_selection_callback_();
    } else {
        std::cout << "ChatInterface: No model selection callback set" << std::endl;
    }
}

void ChatInterface::set_model_selection_callback(std::function<void()> callback) {
    model_selection_callback_ = callback;
    std::cout << "ChatInterface: Model selection callback set" << std::endl;
}

// ============================================================================
// OpenRouter Integration
// ============================================================================

void ChatInterface::send_message_via_openrouter() {
    std::string message_content = input_buffer_;

    // Очистка input
    memset(input_buffer_, 0, sizeof(input_buffer_));
    input_focused_ = true;

    if (message_content.empty()) {
        return;
    }

    // === RAG-ОБРАБОТКА ДЛЯ OPENROUTER ===
    // OpenRouter тоже может использовать RAG-контекст
    std::string final_message_content = message_content;
    bool has_rag_context = false;
    
    bool use_documents = (settings_.rag().rag_mode == llama_gui::core::RagMode::DocumentsOnly ||
                          settings_.rag().rag_mode == llama_gui::core::RagMode::Both);

    if (use_documents && rag_enabled_ && rag_manager_ && settings_.rag().enable_rag) {
        std::cout << "[OpenRouter RAG] Processing query through RAG..." << std::endl;
        std::string rag_prompt = process_with_rag(message_content);
        
        if (rag_prompt != message_content) {
            final_message_content = rag_prompt;
            has_rag_context = true;
            std::cout << "[OpenRouter RAG] RAG context added, size: " << rag_prompt.size() << " bytes" << std::endl;
        }
    }
    // ===================================

    // Получаем активную конверсацию
    std::vector<llama_gui::core::Conversation*> all_convs = state_manager_.get_all_conversations();
    llama_gui::core::Conversation* active_conv = nullptr;
    for (auto* conv : all_convs) {
        if (conv->is_active) {
            active_conv = conv;
            break;
        }
    }

    if (!active_conv) {
        // Создаём новую конверсацию
        std::string new_conv_id = state_manager_.create_conversation("New Chat");
        active_conv = state_manager_.get_conversation(new_conv_id);
        state_manager_.set_active_conversation(new_conv_id);
    }

    // Добавляем сообщение пользователя с RAG-контекстом (если есть)
    llama_gui::core::Message user_msg;
    user_msg.role = "user";
    user_msg.content = has_rag_context ? final_message_content : message_content;
    state_manager_.add_message(active_conv->id, user_msg);

    // Переименование чата из первой строки запроса
    if (active_conv->messages.size() == 1 &&
        (active_conv->title == "New Chat" || active_conv->title.empty())) {
        std::string title = generate_conversation_title(message_content);
        active_conv->title = title;
    }

    // Получаем настройки OpenRouter
    const auto& openrouter_settings = settings_.openrouter();
    const std::string& model_id = openrouter_settings.selected_model;
    const std::string& api_key = settings_.get_openrouter_api_key();

    std::cout << "[OpenRouter] Отправка запроса к модели: " << model_id << std::endl;
    std::cout << "[OpenRouter] API ключ: " << (api_key.empty() ? "НЕТ" : api_key.substr(0, 8) + "...") << std::endl;

    // Создаём клиент OpenRouter
    llama_gui::core::OpenRouterClient client(api_key);
    client.set_timeout(openrouter_settings.timeout_ms);
    
    // Формируем параметры запроса для OpenRouter
    // Примечание: OpenRouter поддерживает ограниченный набор параметров
    llama_gui::core::OpenRouterRequestParams params;
    params.model = model_id;
    params.max_tokens = settings_.chat().max_tokens;
    params.temperature = settings_.chat().temperature;
    params.top_p = settings_.chat().top_p;
    // params.top_k не передаётся - не поддерживается OpenRouter API
    params.stream = false;  // Пока без стриминга
    
    // Дополнительные параметры (поддерживаются не всеми моделями)
    if (settings_.chat().presence_penalty != 0.0f) {
        params.presence_penalty = settings_.chat().presence_penalty;
    }
    if (settings_.chat().frequency_penalty != 0.0f) {
        params.frequency_penalty = settings_.chat().frequency_penalty;
    }
    
    std::cout << "[OpenRouter] Параметры: model=" << model_id 
              << ", max_tokens=" << params.max_tokens 
              << ", temp=" << params.temperature 
              << ", top_p=" << params.top_p;
    if (params.presence_penalty != 0.0f) {
        std::cout << ", presence=" << params.presence_penalty;
    }
    if (params.frequency_penalty != 0.0f) {
        std::cout << ", frequency=" << params.frequency_penalty;
    }
    std::cout << std::endl;
    
    // Добавляем системный промпт
    if (!settings_.chat().default_system_prompt.empty()) {
        llama_gui::core::OpenRouterRequestParams::Message sys_msg;
        sys_msg.role = "system";
        sys_msg.content = settings_.chat().default_system_prompt;
        params.messages.push_back(sys_msg);
    }

    // Получаем историю диалога из active_conv (объявлена выше)
    if (active_conv) {
        for (const auto& msg : active_conv->messages) {
            llama_gui::core::OpenRouterRequestParams::Message orch_msg;
            orch_msg.role = msg.role;
            orch_msg.content = msg.content;
            params.messages.push_back(orch_msg);
        }
    }
    
    // Показываем индикатор загрузки
    streaming_active_ = true;
    
    // Отправляем запрос (синхронно, в отдельном потоке)
    std::string api_key_copy = api_key;
    std::string model_id_copy = model_id;
    std::thread([this, api_key_copy, model_id_copy, params]() mutable {
        llama_gui::core::OpenRouterClient client(api_key_copy);
        client.set_timeout(settings_.openrouter().timeout_ms);
        auto response = client.complete(params);
        
        // Получаем активную конверсацию внутри потока
        std::vector<llama_gui::core::Conversation*> all_convs = state_manager_.get_all_conversations();
        std::string conv_id;
        for (auto* conv : all_convs) {
            if (conv->is_active) {
                conv_id = conv->id;
                break;
            }
        }
        
        if (response.success) {
            std::cout << "[OpenRouter] Ответ получен (модель " << model_id_copy << "): " << response.content.size() << " символов" << std::endl;

            // Добавляем ответ ассистента
            PendingResponse pr;
            pr.content = response.content;
            pr.conversation_id = conv_id;
            pending_responses_.push_back(pr);
        } else {
            std::cerr << "[OpenRouter] Ошибка: " << response.error << std::endl;
            
            // Диагностика ошибки
            std::string error_msg = response.error;
            std::string diagnostic = "[OpenRouter] Диагностика ошибки:\n";
            
            // Проверка типа ошибки
            if (error_msg.find("408") != std::string::npos || 
                error_msg.find("timeout") != std::string::npos ||
                error_msg.find("timed out") != std::string::npos) {
                diagnostic += "  - Тип: Таймаут соединения\n";
                diagnostic += "  - Причина: Сервер OpenRouter не ответил за " + std::to_string(settings_.openrouter().timeout_ms / 1000) + "с\n";
                diagnostic += "  - Решение: Проверьте подключение к интернету или увеличьте таймаут в настройках\n";
            } else if (error_msg.find("401") != std::string::npos) {
                diagnostic += "  - Тип: Ошибка авторизации (401)\n";
                diagnostic += "  - Причина: Неверный или отсутствующий API ключ\n";
                diagnostic += "  - Решение: Проверьте API ключ в настройках облачных сервисов\n";
            } else if (error_msg.find("403") != std::string::npos) {
                diagnostic += "  - Тип: Доступ запрещён (403)\n";
                diagnostic += "  - Причина: API ключ не имеет доступа к этой модели\n";
                diagnostic += "  - Решение: Проверьте подписку или выберите другую модель\n";
            } else if (error_msg.find("429") != std::string::npos) {
                diagnostic += "  - Тип: Превышен лимит (429)\n";
                diagnostic += "  - Причина: Превышен дневной лимит запросов\n";
                diagnostic += "  - Решение: Дождитесь сброса лимита или обновите подписку\n";
            } else if (error_msg.find("500") != std::string::npos || 
                       error_msg.find("502") != std::string::npos ||
                       error_msg.find("503") != std::string::npos) {
                diagnostic += "  - Тип: Ошибка сервера OpenRouter\n";
                diagnostic += "  - Причина: Временные проблемы на стороне сервиса\n";
                diagnostic += "  - Решение: Попробуйте позже или используйте локальную модель\n";
            } else {
                diagnostic += "  - Тип: Неизвестная ошибка\n";
                diagnostic += "  - Детали: " + error_msg + "\n";
            }
            
            diagnostic += "\n[!] ВНИМАНИЕ: Ответ будет получен от локальной модели вместо облачной.\n";
            diagnostic += "    Для переключения обратно выберите модель OpenRouter в настройках облачных сервисов.\n";
            
            std::cerr << diagnostic << std::endl;
            
            // Показываем ошибку в чате
            PendingResponse pr;
            pr.content = diagnostic;
            pr.conversation_id = conv_id;
            pending_responses_.push_back(pr);
            
            // Автоматическое переключение на локальную модель (опционально)
            // settings_.openrouter().enabled = false;  // Можно включить при необходимости
        }

        streaming_active_ = false;
    }).detach();
}

// ============================================================================
// KiloCode Integration
// ============================================================================

void ChatInterface::send_message_via_kilocode() {
    std::string message_content = input_buffer_;

    // Очистка input
    memset(input_buffer_, 0, sizeof(input_buffer_));
    input_focused_ = true;

    if (message_content.empty()) {
        return;
    }

    // === RAG-ОБРАБОТКА ===
    std::string final_message_content = message_content;
    bool has_rag_context = false;

    bool use_documents = (settings_.rag().rag_mode == llama_gui::core::RagMode::DocumentsOnly ||
                          settings_.rag().rag_mode == llama_gui::core::RagMode::Both);

    if (use_documents && rag_enabled_ && rag_manager_ && settings_.rag().enable_rag) {
        std::cout << "[KiloCode RAG] Processing query through RAG..." << std::endl;
        std::string rag_prompt = process_with_rag(message_content);

        if (rag_prompt != message_content) {
            final_message_content = rag_prompt;
            has_rag_context = true;
        }
    }

    // Получаем активную конверсацию
    std::vector<llama_gui::core::Conversation*> all_convs = state_manager_.get_all_conversations();
    llama_gui::core::Conversation* active_conv = nullptr;
    for (auto* conv : all_convs) {
        if (conv->is_active) {
            active_conv = conv;
            break;
        }
    }

    if (!active_conv) {
        std::string new_conv_id = state_manager_.create_conversation("New Chat");
        active_conv = state_manager_.get_conversation(new_conv_id);
        state_manager_.set_active_conversation(new_conv_id);
    }

    // Добавляем сообщение пользователя
    llama_gui::core::Message user_msg;
    user_msg.role = "user";
    user_msg.content = has_rag_context ? final_message_content : message_content;
    state_manager_.add_message(active_conv->id, user_msg);

    // Переименование чата
    if (active_conv->messages.size() == 1 &&
        (active_conv->title == "New Chat" || active_conv->title.empty())) {
        std::string title = generate_conversation_title(message_content);
        active_conv->title = title;
    }

    // Получаем настройки KiloCode
    const auto& kilocode_settings = settings_.kilocode();
    const std::string& model_id = kilocode_settings.selected_model;
    const std::string& api_key = settings_.get_kilocode_api_key();

    std::cout << "[KiloCode] Отправка запроса к модели: " << model_id << std::endl;
    std::cout << "[KiloCode] API ключ: " << (api_key.empty() ? "НЕТ" : api_key.substr(0, 8) + "...") << std::endl;
    std::cout << "[KiloCode] Tor: " << (kilocode_settings.use_tor ? "ВКЛ" : "ВЫКЛ") << " (" << kilocode_settings.tor_proxy_url << ")" << std::endl;

    // Формируем параметры запроса
    llama_gui::core::KiloCodeRequestParams params;
    params.model = model_id;
    params.max_tokens = settings_.chat().max_tokens;
    params.temperature = settings_.chat().temperature;
    params.top_p = settings_.chat().top_p;
    params.stream = false;

    if (settings_.chat().presence_penalty != 0.0f) {
        params.presence_penalty = settings_.chat().presence_penalty;
    }
    if (settings_.chat().frequency_penalty != 0.0f) {
        params.frequency_penalty = settings_.chat().frequency_penalty;
    }

    // Системный промпт
    if (!settings_.chat().default_system_prompt.empty()) {
        llama_gui::core::KiloCodeRequestParams::Message sys_msg;
        sys_msg.role = "system";
        sys_msg.content = settings_.chat().default_system_prompt;
        params.messages.push_back(sys_msg);
    }

    // История диалога
    if (active_conv) {
        for (const auto& msg : active_conv->messages) {
            llama_gui::core::KiloCodeRequestParams::Message orch_msg;
            orch_msg.role = msg.role;
            orch_msg.content = msg.content;
            params.messages.push_back(orch_msg);
        }
    }

    // Сохраняем ID конверсации для лямбды
    std::string conv_id = active_conv->id;

    // Асинхронный запрос в отдельном потоке
    streaming_active_ = true;

    std::string api_key_copy = api_key;
    std::string model_id_copy = model_id;
    std::string tor_proxy = kilocode_settings.tor_proxy_url;
    bool use_tor = kilocode_settings.use_tor;
    int timeout = kilocode_settings.timeout_ms;

    std::thread([this, api_key_copy, model_id_copy, tor_proxy, use_tor, timeout, params, conv_id]() mutable {
        try {
            // Создаём клиент KiloCode
            llama_gui::core::KiloCodeClient client(api_key_copy);
            client.set_timeout(timeout);
            client.use_proxy(use_tor);
            client.set_proxy_url(tor_proxy);

            std::cout << "[KiloCode] Выполнение запроса..." << std::endl;
            auto response = client.complete(params);

            if (response.success) {
                std::cout << "[KiloCode] Ответ получен, длина: " << response.content.size() << std::endl;

                // Добавляем ответ в чат
                llama_gui::core::Message assistant_msg;
                assistant_msg.role = "assistant";
                assistant_msg.content = response.content;

                // Безопасно добавляем через pending_responses
                PendingResponse pr;
                pr.content = response.content;
                pr.conversation_id = conv_id;

                {
                    std::lock_guard<std::mutex> lock(streaming_mutex_);
                    pending_responses_.push_back(pr);
                }
            } else {
                std::string error_msg = response.error;
                std::cout << "[KiloCode] Ошибка: " << error_msg << std::endl;

                std::string diagnostic = "❌ Ошибка KiloCode: " + error_msg + "\n\n";
                diagnostic += "Параметры:\n";
                diagnostic += "  - Модель: " + model_id_copy + "\n";
                diagnostic += "  - Tor: " + std::string(use_tor ? "ВКЛ" : "ВЫКЛ") + "\n";
                diagnostic += "  - Прокси: " + tor_proxy + "\n";

                if (error_msg.find("CURL Error") != std::string::npos ||
                    error_msg.find("connect") != std::string::npos) {
                    diagnostic += "\n[!] Проблема с подключением. Проверьте:\n";
                    diagnostic += "  - Работает ли Tor (порт 9050)\n";
                    diagnostic += "  - Подключение к интернету\n";
                }

                PendingResponse pr;
                pr.content = diagnostic;
                pr.conversation_id = conv_id;
                {
                    std::lock_guard<std::mutex> lock(streaming_mutex_);
                    pending_responses_.push_back(pr);
                }
            }
        } catch (const std::exception& e) {
            std::string error_msg = e.what();
            std::cerr << "[KiloCode] Исключение: " << error_msg << std::endl;

            PendingResponse pr;
            pr.content = "❌ Исключение KiloCode: " + error_msg;
            pr.conversation_id = conv_id;
            {
                std::lock_guard<std::mutex> lock(streaming_mutex_);
                pending_responses_.push_back(pr);
            }
        }

        streaming_active_ = false;
    }).detach();
}

} // namespace ui
} // namespace llama_gui
