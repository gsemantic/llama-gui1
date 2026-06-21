#include "../include/core/rag_manager.h"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <chrono>
#include <iomanip>
#include <future>
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;

namespace llama_gui {
namespace core {

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================================

namespace {

// Форматирование времени для логирования
std::string format_duration(int64_t ms) {
    std::ostringstream oss;
    if (ms < 1000) {
        oss << ms << "ms";
    } else if (ms < 60000) {
        oss << std::fixed << std::setprecision(1) << (ms / 1000.0) << "s";
    } else {
        oss << std::fixed << std::setprecision(1) << (ms / 60000.0) << "min";
    }
    return oss.str();
}

// Подсчёт токенов (приблизительно)
int estimate_tokens(const std::string& text) {
    // Грубая оценка: 1 токен ≈ 4 символа для русского/английского
    return static_cast<int>(text.size() / 4);
}

// Разбиение чанков на батчи
std::vector<std::vector<RagChunk>> create_batches(
    std::vector<RagChunk>& chunks,
    int batch_size)
{
    std::vector<std::vector<RagChunk>> batches;
    
    for (size_t i = 0; i < chunks.size(); i += batch_size) {
        std::vector<RagChunk> batch;
        size_t end = std::min(i + static_cast<size_t>(batch_size), chunks.size());
        batch.reserve(end - i);
        
        for (size_t j = i; j < end; ++j) {
            batch.push_back(chunks[j]);
        }
        
        batches.push_back(std::move(batch));
    }
    
    return batches;
}

// Разбиение строк на батчи (для иерархического режима)
std::vector<std::vector<std::string>> create_batches_for_strings(
    const std::vector<std::string>& items,
    int batch_size)
{
    std::vector<std::vector<std::string>> batches;
    
    for (size_t i = 0; i < items.size(); i += batch_size) {
        std::vector<std::string> batch;
        size_t end = std::min(i + static_cast<size_t>(batch_size), items.size());
        batch.reserve(end - i);
        
        for (size_t j = i; j < end; ++j) {
            batch.push_back(items[j]);
        }
        
        batches.push_back(std::move(batch));
    }
    
    return batches;
}

// Очистка резюме от артефактов
std::string trim_summary(const std::string& text) {
    std::string result = text;
    
    // Удаляем возможные маркеры конца генерации
    const std::vector<std::string> markers = {
        "\n\n=== ", "\n\n---", "\n\n**", "###", "```",
        "\n\nРезюме:", "\n\nОтвет:", "\n\nВывод:"
    };
    
    for (const auto& marker : markers) {
        size_t pos = result.find(marker);
        if (pos != std::string::npos) {
            result = result.substr(0, pos);
        }
    }
    
    // Trim whitespace
    size_t start = result.find_first_not_of(" \t\n\r");
    size_t end = result.find_last_not_of(" \t\n\r");
    
    if (start == std::string::npos) {
        return "";
    }
    
    return result.substr(start, end - start + 1);
}

} // anonymous namespace

// ============================================================================
// ОСНОВНОЙ МЕТОД ГЛУБОКОГО АНАЛИЗА
// ============================================================================

std::string RagManager::process_deep_analysis(
    const std::string& query,
    std::vector<RagChunk>& all_chunks,
    const DeepAnalysisSettings& settings)
{
    auto start_time = std::chrono::steady_clock::now();

    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "[DEEP ANALYSIS] Starting deep document analysis" << std::endl;
    std::cout << "[DEEP ANALYSIS] Query: \"" << query.substr(0, 100);
    if (query.size() > 100) std::cout << "...";
    std::cout << "\"" << std::endl;
    std::cout << "[DEEP ANALYSIS] Total chunks: " << all_chunks.size() << std::endl;
    
    // Проверка инициализации llama_interface
    if (!llama_interface_) {
        std::cerr << "[DEEP ANALYSIS] Error: LlamaInterface not initialized" << std::endl;
        std::cerr << "[DEEP ANALYSIS] Make sure the server is running at http://localhost:8081" << std::endl;
        return "Ошибка: Сервер для анализа не запущен. Запустите сервер llama.cpp.";
    }
    
    // Проверка доступности сервера
    try {
        if (!llama_interface_->is_server_healthy()) {
            std::cerr << "[DEEP ANALYSIS] Error: Server is not reachable" << std::endl;
            std::cerr << "[DEEP ANALYSIS] Make sure the server is running at http://localhost:8081" << std::endl;
            return "Ошибка: Сервер недоступен. Запустите сервер llama.cpp.";
        }
    } catch (const std::exception& e) {
        std::cerr << "[DEEP ANALYSIS] Error checking server: " << e.what() << std::endl;
        return "Ошибка проверки сервера: " + std::string(e.what());
    }
    
    std::cout << "[DEEP ANALYSIS] Mode: ";

    switch (settings.mode) {
        case DeepAnalysisMode::MapReduce:
            std::cout << "MapReduce";
            break;
        case DeepAnalysisMode::Iterative:
            std::cout << "Iterative";
            break;
        case DeepAnalysisMode::Hierarchical:
            std::cout << "Hierarchical";
            break;
        default:
            std::cout << "Disabled";
            return "";
    }

    std::cout << ", chunks_per_batch: " << settings.chunks_per_batch
              << ", max_iterations: " << settings.max_iterations
              << ", target_context_size: " << settings.target_context_size
              << std::endl;
    std::cout << std::string(80, '=') << "\n" << std::endl;
    
    // Авто-увеличение контекста сервера если включено
    if (settings.auto_adjust_context_size) {
        std::cout << "[DEEP ANALYSIS] Auto-adjusting server context size to " 
                  << settings.target_context_size << "..." << std::endl;
        if (!auto_adjust_server_context_size(settings.target_context_size)) {
            std::cerr << "[DEEP ANALYSIS] Warning: Failed to adjust server context size" << std::endl;
        }
    }
    
    std::string result;
    
    // Выбираем режим обработки
    switch (settings.mode) {
        case DeepAnalysisMode::MapReduce:
            result = process_deep_analysis_mapreduce(query, all_chunks, settings);
            break;
            
        case DeepAnalysisMode::Iterative:
            result = process_deep_analysis_iterative(query, all_chunks, settings);
            break;
            
        case DeepAnalysisMode::Hierarchical:
            result = process_deep_analysis_hierarchical(query, all_chunks, settings);
            break;
            
        default:
            std::cerr << "[DEEP ANALYSIS] Error: Unknown deep analysis mode" << std::endl;
            return "";
    }
    
    // Логирование завершения
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    std::cout << "\n" << std::string(80, '=') << std::endl;
    std::cout << "[DEEP ANALYSIS] Completed in " << format_duration(duration) << std::endl;
    std::cout << "[DEEP ANALYSIS] Result size: " << result.size() << " chars"
              << " (~" << estimate_tokens(result) << " tokens)" << std::endl;
    std::cout << std::string(80, '=') << "\n" << std::endl;
    
    return result;
}

// ============================================================================
// MAP-REDUCE РЕЖИМ
// ============================================================================

std::string RagManager::process_deep_analysis_mapreduce(
    const std::string& query,
    std::vector<RagChunk>& all_chunks,
    const DeepAnalysisSettings& settings)
{
    std::cout << "\n[MAP-REDUCE] === PHASE 1: MAP (Creating batch summaries) ===" << std::endl;

    auto map_start = std::chrono::steady_clock::now();

    // Разбиваем чанки на батчи
    int batch_size = settings.chunks_per_batch;
    auto batches = create_batches(all_chunks, batch_size);

    std::cout << "[MAP-REDUCE] Split " << all_chunks.size() << " chunks into "
              << batches.size() << " batches (batch_size=" << batch_size << ")" << std::endl;

    // Определяем количество потоков для параллелизации
    const int num_threads = std::min(4, static_cast<int>(batches.size()));
    std::cout << "[MAP-REDUCE] Using " << num_threads << " threads for parallel processing" << std::endl;

    // Map-этап: создаём резюме для каждого батча ПАРАЛЛЕЛЬНО
    std::vector<std::string> batch_summaries(batches.size());
    std::vector<std::future<void>> futures;
    futures.reserve(batches.size());

    for (size_t batch_idx = 0; batch_idx < batches.size(); ++batch_idx) {
        futures.push_back(std::async(std::launch::async, [this, batch_idx, &batch_summaries, &batches, &query]() {
            const auto& batch = batches[batch_idx];
            
            // Собираем текст батча
            std::string batch_text;
            batch_text.reserve(10000);

            for (size_t i = 0; i < batch.size(); ++i) {
                batch_text += "\n--- Chunk " + std::to_string(i+1) + " (doc: " + batch[i].document_id
                              + ", idx: " + std::to_string(batch[i].chunk_index) + ") ---\n";
                batch_text += batch[i].content;
            }

            // Создаём резюме батча
            RagChunk batch_chunk;
            batch_chunk.content = batch_text;
            batch_chunk.document_id = "batch_" + std::to_string(batch_idx + 1);
            batch_chunk.chunk_index = 0;

            std::string summary = generate_chunk_summary(batch_chunk, query);

            if (!summary.empty()) {
                batch_summaries[batch_idx] = std::move(summary);
            }
        }));
    }

    // Ждём завершения всех задач
    for (auto& future : futures) {
        future.get();
    }

    // Фильтруем пустые резюме и проверяем лимит
    std::vector<std::string> filtered_summaries;
    filtered_summaries.reserve(batch_summaries.size());
    
    for (size_t i = 0; i < batch_summaries.size(); ++i) {
        if (!batch_summaries[i].empty()) {
            filtered_summaries.push_back(std::move(batch_summaries[i]));
            
            if (static_cast<int>(filtered_summaries.size()) >= settings.max_iterations) {
                std::cout << "[MAP-REDUCE] Reached max_iterations limit ("
                          << settings.max_iterations << ")" << std::endl;
                break;
            }
        } else {
            std::cerr << "[MAP-REDUCE] Warning: Failed to generate summary for batch " << (i + 1) << std::endl;
        }
    }

    batch_summaries = std::move(filtered_summaries);

    auto map_end = std::chrono::steady_clock::now();
    auto map_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        map_end - map_start).count();

    std::cout << "\n[MAP-REDUCE] MAP phase completed in " << format_duration(map_duration) << std::endl;
    std::cout << "[MAP-REDUCE] Generated " << batch_summaries.size() << " batch summaries" << std::endl;

    // Reduce-этап: синтез финального ответа
    std::cout << "\n[MAP-REDUCE] === PHASE 2: REDUCE (Synthesizing final answer) ===" << std::endl;

    auto reduce_start = std::chrono::steady_clock::now();

    std::string final_answer = synthesize_final_answer(
        query,
        batch_summaries,
        settings.target_context_size);

    auto reduce_end = std::chrono::steady_clock::now();
    auto reduce_duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        reduce_end - reduce_start).count();

    std::cout << "\n[MAP-REDUCE] REDUCE phase completed in " << format_duration(reduce_duration) << std::endl;
    std::cout << "[MAP-REDUCE] Final answer: " << final_answer.size() << " chars"
              << " (~" << estimate_tokens(final_answer) << " tokens)" << std::endl;

    return final_answer;
}

// ============================================================================
// ИТЕРАТИВНЫЙ РЕЖИМ
// ============================================================================

std::string RagManager::process_deep_analysis_iterative(
    const std::string& query,
    std::vector<RagChunk>& all_chunks,
    const DeepAnalysisSettings& settings)
{
    std::cout << "\n[ITERATIVE] === Starting iterative chunk processing ===" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Накопительное резюме
    std::string accumulated_summary;
    accumulated_summary.reserve(20000);
    
    int processed_count = 0;
    
    for (size_t i = 0; i < all_chunks.size(); ++i) {
        // Проверка на максимальное количество итераций
        if (processed_count >= settings.max_iterations) {
            std::cout << "[ITERATIVE] Reached max_iterations limit (" 
                      << settings.max_iterations << ") at chunk " << i << std::endl;
            break;
        }
        
        std::cout << "\n[ITERATIVE] Processing chunk " << (i+1) << "/" 
                  << all_chunks.size() << " (doc: " << all_chunks[i].document_id 
                  << ", idx: " << all_chunks[i].chunk_index << ")..." << std::endl;
        
        // Генерируем резюме для текущего чанка
        std::string chunk_summary = generate_chunk_summary(all_chunks[i], query);
        
        if (!chunk_summary.empty()) {
            // Добавляем к накопительному резюме
            if (!accumulated_summary.empty()) {
                accumulated_summary += "\n\n";
            }
            accumulated_summary += "=== Chunk " + std::to_string(i+1) + " ===\n";
            accumulated_summary += chunk_summary;
            
            processed_count++;
            
            // Прогрессивное суммирование: если резюме становится слишком большим,
            // сжимаем его
            if (settings.enable_progressive_summary && 
                estimate_tokens(accumulated_summary) > settings.target_context_size / 2) {
                
                std::cout << "[ITERATIVE] Accumulated summary is large, compressing..." << std::endl;
                
                RagChunk summary_chunk;
                summary_chunk.content = accumulated_summary;
                summary_chunk.document_id = "accumulated";
                summary_chunk.chunk_index = 0;
                
                std::string compressed = generate_chunk_summary(summary_chunk, query);
                
                if (!compressed.empty()) {
                    accumulated_summary = compressed;
                    std::cout << "[ITERATIVE] Compressed to " << accumulated_summary.size() 
                              << " chars (~" << estimate_tokens(accumulated_summary) << " tokens)" << std::endl;
                }
            }
            
            std::cout << "[ITERATIVE] Current accumulated summary: " 
                      << accumulated_summary.size() << " chars" << std::endl;
        } else {
            std::cerr << "[ITERATIVE] Warning: Failed to generate summary for chunk " << i << std::endl;
        }
    }
    
    // Финальный синтез
    std::cout << "\n[ITERATIVE] === Final synthesis ===" << std::endl;
    
    std::vector<std::string> summaries = {accumulated_summary};
    std::string final_answer = synthesize_final_answer(
        query,
        summaries,
        settings.target_context_size);
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    std::cout << "\n[ITERATIVE] Completed in " << format_duration(duration) << std::endl;
    std::cout << "[ITERATIVE] Processed " << processed_count << " chunks" << std::endl;
    std::cout << "[ITERATIVE] Final answer: " << final_answer.size() << " chars" << std::endl;
    
    return final_answer;
}

// ============================================================================
// ИЕРАРХИЧЕСКИЙ РЕЖИМ
// ============================================================================

std::string RagManager::process_deep_analysis_hierarchical(
    const std::string& query,
    std::vector<RagChunk>& all_chunks,
    const DeepAnalysisSettings& settings)
{
    std::cout << "\n[HIERARCHICAL] === Building summary tree ===" << std::endl;
    
    auto start_time = std::chrono::steady_clock::now();
    
    // Копируем чанки для работы
    std::vector<std::string> current_level;
    current_level.reserve(all_chunks.size());
    
    // Уровень 0: исходные чанки
    for (const auto& chunk : all_chunks) {
        current_level.push_back(chunk.content);
    }
    
    std::cout << "[HIERARCHICAL] Level 0: " << current_level.size() << " leaf chunks" << std::endl;
    
    int level = 0;
    
    // Итеративно строим дерево резюме
    while (current_level.size() > 1) {
        level++;
        std::cout << "\n[HIERARCHICAL] === Building level " << level << " ===" << std::endl;
        
        // Разбиваем на группы для суммаризации
        int group_size = std::max(2, settings.chunks_per_batch / 2);
        auto batches = create_batches_for_strings(current_level, group_size);
        
        std::vector<std::string> next_level;
        next_level.reserve(batches.size());
        
        int group_num = 0;
        for (const auto& batch : batches) {
            group_num++;
            
            // Собираем текст группы
            std::string group_text;
            group_text.reserve(15000);
            
            for (size_t i = 0; i < batch.size(); ++i) {
                group_text += "\n--- Item " + std::to_string(i+1) + " ---\n";
                group_text += batch[i];
            }
            
            // Создаём резюме группы
            RagChunk group_chunk;
            group_chunk.content = group_text;
            group_chunk.document_id = "level_" + std::to_string(level) + "_group_" + std::to_string(group_num);
            group_chunk.chunk_index = 0;
            
            std::string summary = generate_chunk_summary(group_chunk, query);
            
            if (!summary.empty()) {
                next_level.push_back(summary);
                std::cout << "[HIERARCHICAL] Group " << group_num << " → " 
                          << summary.size() << " chars" << std::endl;
            }
        }
        
        if (next_level.empty()) {
            std::cerr << "[HIERARCHICAL] Error: Failed to generate any summaries at level " 
                      << level << std::endl;
            break;
        }
        
        current_level = std::move(next_level);
        std::cout << "[HIERARCHICAL] Level " << level << ": " << current_level.size() << " summaries" << std::endl;
        
        // Проверка на зацикливание
        if (level > 20) {
            std::cerr << "[HIERARCHICAL] Warning: Too many levels, stopping" << std::endl;
            break;
        }
    }
    
    // Финальный синтез из корня дерева
    std::cout << "\n[HIERARCHICAL] === Final synthesis from root ===" << std::endl;
    
    std::string final_answer;
    
    if (current_level.size() == 1) {
        // Остался один элемент - это и есть финальное резюме
        final_answer = current_level[0];
    } else {
        // Несколько элементов - синтезируем
        std::vector<std::string> summaries = current_level;
        final_answer = synthesize_final_answer(query, summaries, settings.target_context_size);
    }
    
    auto end_time = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time).count();
    
    std::cout << "\n[HIERARCHICAL] Completed in " << format_duration(duration) << std::endl;
    std::cout << "[HIERARCHICAL] Tree depth: " << level << " levels" << std::endl;
    std::cout << "[HIERARCHICAL] Final answer: " << final_answer.size() << " chars" << std::endl;
    
    return final_answer;
}

// ============================================================================
// ГЕНЕРАЦИЯ РЕЗЮМЕ ДЛЯ ЧАНКА (MAP-ЭТАП)
// ============================================================================

std::string RagManager::generate_chunk_summary(
    const RagChunk& chunk,
    const std::string& query)
{
    // === ПРОВЕРКА: используем ли OpenRouter? ===
    if (use_openrouter_for_rag_ && openrouter_client_) {
        return generate_chunk_summary_openrouter(chunk, query);
    }
    
    // Старый код для локального сервера
    std::cout << "[SUMMARY] Generating summary for chunk " << chunk.chunk_index
              << " (doc: " << chunk.document_id << ")" << std::endl;

    // Формируем системный промпт для суммаризации
    std::string system_prompt =
        "Ты - ассистент для анализа документов. Твоя задача - создавать краткие резюме текстов.\n"
        "Создавай резюме (3-5 предложений), которое содержит ключевую информацию и фокусируется на аспектах, релевантных вопросу пользователя.\n"
        "Сохраняй важные факты, цифры, имена. Игнорируй второстепенные детали.\n"
        "Пиши только резюме, без вводных фраз.";

    // Формируем пользовательский запрос
    std::string user_message =
        "Текст для анализа:\n" + chunk.content + "\n\n"
        "Вопрос пользователя: " + query + "\n\n"
        "Создай краткое резюме текста, релевантное вопросу.";

    // Отправляем запрос на сервер через LlamaInterface
    if (!llama_interface_) {
        std::cerr << "[SUMMARY] Error: LlamaInterface not initialized" << std::endl;
        return "";
    }

    std::cout << "[SUMMARY] Sending summary request to server..." << std::endl;

    // Создаём запрос
    ChatCompletionRequest request;
    request.max_tokens = 512;
    request.temperature = 0.3f;  // Низкая температура для фактологического резюме
    request.top_p = 0.9f;
    request.repeat_penalty = 1.0f;
    request.stream = false;  // Синхронный ответ

    // Добавляем сообщения
    ChatMessage system_msg(MessageRole::System, system_prompt);
    request.messages.push_back(system_msg);

    ChatMessage user_msg(MessageRole::User, user_message);
    request.messages.push_back(user_msg);

    // Выполняем запрос асинхронно и ждём результат
    try {
        auto future_response = llama_interface_->create_chat_completion_async(request);

        // Ждём ответ с таймаутом (90 секунд для больших батчей)
        auto status = future_response.wait_for(std::chrono::seconds(90));

        if (status == std::future_status::ready) {
            auto response = future_response.get();

            if (!response.choices.empty() && !response.choices[0].message.content.empty()) {
                std::string summary = response.choices[0].message.content;

                // Очищаем резюме от возможных артефактов
                summary = trim_summary(summary);

                std::cout << "[SUMMARY] Generated summary: " << summary.size() << " chars"
                          << " (~" << estimate_tokens(summary) << " tokens)" << std::endl;

                return summary;
            } else {
                std::cerr << "[SUMMARY] Error: Empty response from server" << std::endl;
                return "[Пустое резюме]";
            }
        } else {
            std::cerr << "[SUMMARY] Error: Request timeout (90s)" << std::endl;
            std::cerr << "[SUMMARY] Try reducing batch size or increasing server context" << std::endl;
            return "[Таймаут генерации резюме]";
        }
    } catch (const std::exception& e) {
        std::cerr << "[SUMMARY] Error: Request failed: " << e.what() << std::endl;
        return "[Ошибка: " + std::string(e.what()) + "]";
    }
}

// ============================================================================
// ГЕНЕРАЦИЯ РЕЗЮМЕ ЧЕРЕЗ OPENROUTER (MAP-ЭТАП)
// ============================================================================

std::string RagManager::generate_chunk_summary_openrouter(
    const RagChunk& chunk,
    const std::string& query)
{
    std::cout << "[SUMMARY] Using OpenRouter for summary (chunk " << chunk.chunk_index
              << ", doc: " << chunk.document_id << ")" << std::endl;

    // Формируем системный промпт для суммаризации
    std::string system_prompt =
        "Ты - ассистент для анализа документов. Твоя задача - создавать краткие резюме текстов.\n"
        "Создавай резюме (3-5 предложений), которое содержит ключевую информацию и фокусируется на аспектах, релевантных вопросу пользователя.\n"
        "Сохраняй важные факты, цифры, имена. Игнорируй второстепенные детали.\n"
        "Пиши только резюме, без вводных фраз.";

    // Формируем пользовательский запрос
    std::string user_message =
        "Текст для анализа:\n" + chunk.content + "\n\n"
        "Вопрос пользователя: " + query + "\n\n"
        "Создай краткое резюме текста, релевантное вопросу.";

    std::cout << "[SUMMARY] Sending summary request to OpenRouter..." << std::endl;

    // === УВЕЛИЧЕННЫЙ ТАЙМАУТ: 120 секунд для Map-этапа ===
    int original_timeout = openrouter_client_->get_timeout();
    openrouter_client_->set_timeout(120000);  // 120 секунд

    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
    params.max_tokens = 512;
    params.temperature = 0.3f;  // Низкая температура для фактологического резюме
    params.top_p = 0.9f;
    params.stream = false;  // Синхронный ответ

    // Добавляем сообщения
    params.messages.push_back({"system", system_prompt});
    params.messages.push_back({"user", user_message});

    try {
        // Выполняем запрос
        auto response = openrouter_client_->complete(params);

        // Восстанавливаем оригинальный таймаут
        openrouter_client_->set_timeout(original_timeout);

        if (response.success && !response.content.empty()) {
            std::string summary = response.content;

            // Очищаем резюме от возможных артефактов
            summary = trim_summary(summary);

            std::cout << "[SUMMARY] OpenRouter summary: " << summary.size() << " chars"
                      << " (~" << estimate_tokens(summary) << " tokens)"
                      << " (model: " << response.model << ")" << std::endl;

            return summary;
        } else {
            std::cerr << "[SUMMARY] OpenRouter error: " << response.error << std::endl;
            return "[Ошибка OpenRouter: " + response.error + "]";
        }
    } catch (const std::exception& e) {
        // Восстанавливаем таймаут при ошибке
        openrouter_client_->set_timeout(original_timeout);
        std::cerr << "[SUMMARY] OpenRouter request failed: " << e.what() << std::endl;
        return "[Ошибка: " + std::string(e.what()) + "]";
    }
}

// ============================================================================
// СИНТЕЗ ФИНАЛЬНОГО ОТВЕТА (REDUCE-ЭТАП)
// ============================================================================

std::string RagManager::synthesize_final_answer(
    const std::string& query,
    const std::vector<std::string>& summaries,
    int target_context_size)
{
    // === ПРОВЕРКА: используем ли OpenRouter? ===
    if (use_openrouter_for_rag_ && openrouter_client_) {
        return synthesize_final_answer_openrouter(query, summaries, target_context_size);
    }
    
    // Старый код для локального сервера
    std::cout << "[SYNTHESIS] Synthesizing final answer from " << summaries.size()
              << " summaries" << std::endl;

    // Собираем все резюме в один контекст
    std::string context;
    context.reserve(30000);

    for (size_t i = 0; i < summaries.size(); ++i) {
        context += "\n=== Источник " + std::to_string(i+1) + " ===\n";
        context += summaries[i];
    }

    // Проверяем размер контекста
    int context_tokens = estimate_tokens(context);
    std::cout << "[SYNTHESIS] Total context: " << context.size() << " chars"
              << " (~" << context_tokens << " tokens)" << std::endl;

    // Если контекст слишком большой, обрезаем
    if (context_tokens > target_context_size * 3 / 4) {
        std::cout << "[SYNTHESIS] Context too large, truncating..." << std::endl;
        size_t max_chars = target_context_size * 3;  // ~3 chars per token
        if (context.size() > max_chars) {
            context = context.substr(0, max_chars);
            // Находим последнюю полную UTF-8 последовательность
            while (!context.empty() &&
                   (static_cast<unsigned char>(context.back()) & 0xC0) == 0x80) {
                context.pop_back();
            }
            context += "\n[...обрезано из-за ограничения размера...]";
        }
    }

    // Формируем системный промпт для синтеза
    std::string system_prompt =
        "Ты - ассистент для синтеза информации из нескольких источников. "
        "Твоя задача - создать полный, связный ответ на вопрос пользователя на основе предоставленных резюме.";

    // Формируем пользовательский запрос
    std::string user_message =
        "Вопрос пользователя: " + query + "\n\n"
        "Промежуточные резюме из документов:\n" + context + "\n\n"
        "ИНСТРУКЦИЯ:\n"
        "1. Внимательно изучи все промежуточные резюме выше.\n"
        "2. Создай полный, связный ответ на вопрос пользователя.\n"
        "3. Твой ответ должен:\n"
        "   - Интегрировать информацию из всех источников\n"
        "   - Быть логически структурированным\n"
        "   - Содержать конкретные факты и детали\n"
        "   - Избегать повторений\n"
        "4. Если источники противоречат друг другу, укажи на это.\n"
        "5. Если информации недостаточно, честно скажи об этом.\n\n"
        "ОТВЕТ:";

    // Отправляем запрос на сервер
    if (!llama_interface_) {
        std::cerr << "[SYNTHESIS] Error: LlamaInterface not initialized" << std::endl;
        return "";
    }

    std::cout << "[SYNTHESIS] Sending synthesis request to server..." << std::endl;

    // Создаём запрос
    ChatCompletionRequest request;

    // Вычисляем max_tokens на основе target_context_size
    request.max_tokens = std::min(target_context_size / 2, 1024);
    request.temperature = 0.5f;  // Немного выше для связного текста
    request.top_p = 0.95f;
    request.repeat_penalty = 1.1f;
    request.stream = false;  // Синхронный ответ

    // Добавляем сообщения
    ChatMessage system_msg(MessageRole::System, system_prompt);
    request.messages.push_back(system_msg);

    ChatMessage user_msg(MessageRole::User, user_message);
    request.messages.push_back(user_msg);

    // Выполняем запрос асинхронно и ждём результат
    try {
        auto future_response = llama_interface_->create_chat_completion_async(request);

        // Ждём ответ с таймаутом (60 секунд для большого синтеза)
        auto status = future_response.wait_for(std::chrono::seconds(60));

        if (status == std::future_status::ready) {
            auto response = future_response.get();

            if (!response.choices.empty() && !response.choices[0].message.content.empty()) {
                std::string final_answer = response.choices[0].message.content;

                // Очищаем ответ
                final_answer = trim_summary(final_answer);

                std::cout << "[SYNTHESIS] Final answer: " << final_answer.size() << " chars"
                          << " (~" << estimate_tokens(final_answer) << " tokens)" << std::endl;

                return final_answer;
            } else {
                std::cerr << "[SYNTHESIS] Error: Empty response from server" << std::endl;
            }
        } else {
            std::cerr << "[SYNTHESIS] Error: Request timeout (60s)" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "[SYNTHESIS] Error: Request failed: " << e.what() << std::endl;
    }

    return "";
}

// ============================================================================
// СИНТЕЗ ФИНАЛЬНОГО ОТВЕТА ЧЕРЕЗ OPENROUTER (REDUCE-ЭТАП)
// ============================================================================

std::string RagManager::synthesize_final_answer_openrouter(
    const std::string& query,
    const std::vector<std::string>& summaries,
    int target_context_size)
{
    std::cout << "[SYNTHESIS] Using OpenRouter for final synthesis from " << summaries.size()
              << " summaries" << std::endl;

    // Собираем все резюме в один контекст
    std::string context;
    context.reserve(30000);

    for (size_t i = 0; i < summaries.size(); ++i) {
        context += "\n=== Источник " + std::to_string(i+1) + " ===\n";
        context += summaries[i];
    }

    // Проверяем размер контекста
    int context_tokens = estimate_tokens(context);
    std::cout << "[SYNTHESIS] Total context: " << context.size() << " chars"
              << " (~" << context_tokens << " tokens)" << std::endl;

    // Если контекст слишком большой, обрезаем
    if (context_tokens > target_context_size * 3 / 4) {
        std::cout << "[SYNTHESIS] Context too large, truncating..." << std::endl;
        size_t max_chars = target_context_size * 3;  // ~3 chars per token
        if (context.size() > max_chars) {
            context = context.substr(0, max_chars);
            // Находим последнюю полную UTF-8 последовательность
            while (!context.empty() &&
                   (static_cast<unsigned char>(context.back()) & 0xC0) == 0x80) {
                context.pop_back();
            }
            context += "\n[...обрезано из-за ограничения размера...]";
        }
    }

    // Формируем системный промпт для синтеза
    std::string system_prompt =
        "Ты - ассистент для синтеза информации из нескольких источников. "
        "Твоя задача - создать полный, связный ответ на вопрос пользователя на основе предоставленных резюме.";

    // Формируем пользовательский запрос
    std::string user_message =
        "Вопрос пользователя: " + query + "\n\n"
        "Промежуточные резюме из документов:\n" + context + "\n\n"
        "ИНСТРУКЦИЯ:\n"
        "1. Внимательно изучи все промежуточные резюме выше.\n"
        "2. Создай полный, связный ответ на вопрос пользователя.\n"
        "3. Твой ответ должен:\n"
        "   - Интегрировать информацию из всех источников\n"
        "   - Быть логически структурированным\n"
        "   - Содержать конкретные факты и детали\n"
        "   - Избегать повторений\n"
        "4. Если источники противоречат друг другу, укажи на это.\n"
        "5. Если информации недостаточно, честно скажи об этом.\n\n"
        "ОТВЕТ:";

    std::cout << "[SYNTHESIS] Sending synthesis request to OpenRouter..." << std::endl;

    // === УВЕЛИЧЕННЫЙ ТАЙМАУТ: 180 секунд для Reduce-этапа ===
    int original_timeout = openrouter_client_->get_timeout();
    openrouter_client_->set_timeout(180000);  // 180 секунд

    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
    params.max_tokens = std::min(target_context_size / 2, 1024);
    params.temperature = 0.5f;  // Немного выше для связного текста
    params.top_p = 0.95f;
    params.stream = false;  // Синхронный ответ

    // Добавляем сообщения
    params.messages.push_back({"system", system_prompt});
    params.messages.push_back({"user", user_message});

    try {
        // Выполняем запрос
        auto response = openrouter_client_->complete(params);

        // Восстанавливаем оригинальный таймаут
        openrouter_client_->set_timeout(original_timeout);

        if (response.success && !response.content.empty()) {
            std::string final_answer = response.content;

            // Очищаем ответ
            final_answer = trim_summary(final_answer);

            std::cout << "[SYNTHESIS] OpenRouter final answer: " << final_answer.size() << " chars"
                      << " (~" << estimate_tokens(final_answer) << " tokens)"
                      << " (model: " << response.model << ")" << std::endl;

            return final_answer;
        } else {
            std::cerr << "[SYNTHESIS] OpenRouter error: " << response.error << std::endl;
            return "[Ошибка OpenRouter: " + response.error + "]";
        }
    } catch (const std::exception& e) {
        // Восстанавливаем таймаут при ошибке
        openrouter_client_->set_timeout(original_timeout);
        std::cerr << "[SYNTHESIS] OpenRouter request failed: " << e.what() << std::endl;
        return "[Ошибка: " + std::string(e.what()) + "]";
    }
}

// ============================================================================
// АВТО-УВЕЛИЧЕНИЕ КОНТЕКСТА СЕРВЕРА
// ============================================================================

bool RagManager::auto_adjust_server_context_size(int target_context_size) {
    if (!llama_interface_) {
        std::cerr << "[CONTEXT ADJUST] Error: LlamaInterface not initialized" << std::endl;
        return false;
    }
    
    std::cout << "[CONTEXT ADJUST] Requesting server to adjust context size to " 
              << target_context_size << "..." << std::endl;
    
    // Проверяем текущий контекст сервера
    try {
        json server_info_json = llama_interface_->get_server_info();
        
        if (!server_info_json.is_null()) {
            int current_ctx = server_info_json.value("n_ctx", 0);
            
            if (current_ctx > 0) {
                std::cout << "[CONTEXT ADJUST] Current server context: " 
                          << current_ctx << " tokens" << std::endl;
                
                if (current_ctx >= target_context_size) {
                    std::cout << "[CONTEXT ADJUST] Server context already sufficient" << std::endl;
                    return true;
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[CONTEXT ADJUST] Warning: Failed to get server info: " 
                  << e.what() << std::endl;
        // Продолжаем без проверки
    }
    
    // Примечание: llama.cpp сервер не поддерживает динамическое изменение контекста
    // через API. Контекст устанавливается при запуске сервера.
    // Этот метод только логирует предупреждение.
    std::cerr << "[CONTEXT ADJUST] Server does not support dynamic context adjustment" << std::endl;
    std::cerr << "[CONTEXT ADJUST] Context size must be set when starting the server" << std::endl;
    std::cerr << "[CONTEXT ADJUST] Will work with existing context size" << std::endl;
    
    return false;
}

// ============================================================================
// ПРОВЕРКА ДОСТУПНОСТИ OPENROUTER API (НЕ тратит лимиты!)
// ============================================================================

bool RagManager::check_openrouter_availability() {
    if (!openrouter_client_) {
        std::cerr << "[OPENROUTER CHECK] Client not initialized" << std::endl;
        return false;
    }

    std::cout << "[OPENROUTER CHECK] Checking API availability..." << std::endl;

    // Проверка через GET /api/v1/models - НЕ тратит лимиты генерации!
    bool available = openrouter_client_->is_api_available();

    if (available) {
        std::cout << "[OPENROUTER CHECK] ✅ API is available" << std::endl;
    } else {
        std::cerr << "[OPENROUTER CHECK] ❌ API is NOT available" << std::endl;
    }

    return available;
}

// ============================================================================
// ПРОВЕРКА ЛИМИТОВ OPENROUTER (НЕ тратит лимиты!)
// ============================================================================

bool RagManager::check_openrouter_rate_limit(int required_requests) {
    if (!openrouter_client_) {
        return false;
    }

    std::cout << "[RATE LIMIT CHECK] Checking OpenRouter rate limits..." << std::endl;

    // Получаем информацию о лимитах через GET /api/v1/auth/key
    OpenRouterRateLimit rate_limit = openrouter_client_->get_rate_limit();

    std::cout << "[RATE LIMIT CHECK] Limit: " << rate_limit.limit
              << ", Used: " << rate_limit.total_requests
              << ", Remaining: " << rate_limit.remaining_requests
              << " (Free tier: " << (rate_limit.is_free_tier ? "yes" : "no") << ")"
              << std::endl;

    // Проверяем, достаточно ли запросов
    if (rate_limit.remaining_requests < required_requests) {
        std::cerr << "[RATE LIMIT CHECK] ❌ Insufficient requests: need "
                  << required_requests << ", have " << rate_limit.remaining_requests
                  << std::endl;
        return false;
    }

    // Дополнительная проверка: если осталось мало запросов (< 10)
    if (rate_limit.remaining_requests < 10) {
        std::cout << "[RATE LIMIT CHECK] ⚠️  Warning: Low requests remaining ("
                  << rate_limit.remaining_requests << ")" << std::endl;
    } else {
        std::cout << "[RATE LIMIT CHECK] ✅ Rate limit check passed" << std::endl;
    }

    return true;
}

// ============================================================================
// FALLBACK НА ЛОКАЛЬНЫЙ СЕРВЕР ПРИ ОШИБКАХ OPENROUTER
// ============================================================================

std::string RagManager::generate_chunk_summary_with_fallback(
    const RagChunk& chunk,
    const std::string& query)
{
    // Если OpenRouter не включен, сразу используем локальный сервер
    if (!use_openrouter_for_rag_ || !openrouter_client_) {
        return generate_chunk_summary_local(chunk, query);
    }

    // === ПРОВЕРКА ДОСТУПНОСТИ API ===
    if (!check_openrouter_availability()) {
        std::cerr << "[FALLBACK] OpenRouter unavailable, switching to local server" << std::endl;
        return generate_chunk_summary_local(chunk, query);
    }

    // === ПРОВЕРКА ЛИМИТОВ ===
    if (!check_openrouter_rate_limit(10)) {
        std::cerr << "[FALLBACK] Rate limit exceeded, switching to local server" << std::endl;
        return generate_chunk_summary_local(chunk, query);
    }

    // === ПОПЫТКА OPENROUTER С RETRY ===
    int retry_count = 0;
    const int max_retries = 1;  // 1 повтор при таймауте

    while (retry_count <= max_retries) {
        try {
            std::cout << "[FALLBACK] Attempt " << (retry_count + 1)
                      << " to OpenRouter (chunk " << chunk.chunk_index << ")" << std::endl;

            // Формируем запрос к OpenRouter
            std::string system_prompt =
                "Ты - ассистент для анализа документов. Твоя задача - создавать краткие резюме текстов.\n"
                "Создавай резюме (3-5 предложений), которое содержит ключевую информацию и фокусируется на аспектах, релевантных вопросу пользователя.\n"
                "Сохраняй важные факты, цифры, имена. Игнорируй второстепенные детали.\n"
                "Пиши только резюме, без вводных фраз.";

            std::string user_message =
                "Текст для анализа:\n" + chunk.content + "\n\n"
                "Вопрос пользователя: " + query + "\n\n"
                "Создай краткое резюме текста, релевантное вопросу.";

            OpenRouterRequestParams params;
            params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
            params.max_tokens = 512;
            params.temperature = 0.3f;
            params.top_p = 0.9f;
            params.stream = false;

            params.messages.push_back({"system", system_prompt});
            params.messages.push_back({"user", user_message});

            // Выполняем запрос
            auto response = openrouter_client_->complete(params);

            if (response.success && !response.content.empty()) {
                std::string summary = trim_summary(response.content);

                // ✅ ЛОГИРОВАНИЕ ФАКТИЧЕСКОЙ МОДЕЛИ
                std::cout << "[SUMMARY] OpenRouter summary: " << summary.size() << " chars"
                          << " (~" << estimate_tokens(summary) << " tokens)"
                          << " (model: " << response.model << ")" << std::endl;

                return summary;
            }

            // Обработка ошибок API
            if (response.error.find("Rate Limit") != std::string::npos ||
                response.error.find("429") != std::string::npos) {
                std::cerr << "[FALLBACK] Rate limit exceeded, switching to local server" << std::endl;
                return generate_chunk_summary_local(chunk, query);
            }

            if (response.error.find("400") != std::string::npos ||
                response.error.find("401") != std::string::npos ||
                response.error.find("403") != std::string::npos ||
                response.error.find("500") != std::string::npos) {
                std::cerr << "[FALLBACK] API error (" << response.error.substr(0, 50) << "), switching to local server" << std::endl;
                return generate_chunk_summary_local(chunk, query);
            }

            // Таймаут - пробуем retry
            if (response.error.find("таймаут") != std::string::npos ||
                response.error.find("timeout") != std::string::npos ||
                response.error.find("CURL") != std::string::npos) {

                retry_count++;
                if (retry_count <= max_retries) {
                    std::cout << "[FALLBACK] Timeout, retrying (" << retry_count << "/" << max_retries << ")..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(2));
                    continue;
                }
            }

            // Другие ошибки - сразу fallback
            std::cerr << "[FALLBACK] OpenRouter error: " << response.error << std::endl;
            break;

        } catch (const std::exception& e) {
            std::cerr << "[FALLBACK] Exception: " << e.what() << std::endl;
            retry_count++;
            if (retry_count > max_retries) {
                break;
            }
        }
    }

    // === FALLBACK НА ЛОКАЛЬНЫЙ СЕРВЕР ===
    std::cout << "[FALLBACK] Switching to local server after " << retry_count << " failed attempts" << std::endl;
    return generate_chunk_summary_local(chunk, query);
}

std::string RagManager::generate_chunk_summary_local(
    const RagChunk& chunk,
    const std::string& query)
{
    std::cout << "[SUMMARY] Using LOCAL server for summary (chunk " << chunk.chunk_index
              << ", doc: " << chunk.document_id << ")" << std::endl;

    std::string system_prompt =
        "Ты - ассистент для анализа документов. Твоя задача - создавать краткие резюме текстов.\n"
        "Создавай резюме (3-5 предложений), которое содержит ключевую информацию и фокусируется на аспектах, релевантных вопросу пользователя.\n"
        "Сохраняй важные факты, цифры, имена. Игнорируй второстепенные детали.\n"
        "Пиши только резюме, без вводных фраз.";

    std::string user_message =
        "Текст для анализа:\n" + chunk.content + "\n\n"
        "Вопрос пользователя: " + query + "\n\n"
        "Создай краткое резюме текста, релевантное вопросу.";

    if (!llama_interface_) {
        std::cerr << "[SUMMARY] Error: LlamaInterface not initialized" << std::endl;
        return "";
    }

    std::cout << "[SUMMARY] Sending summary request to local server..." << std::endl;

    ChatCompletionRequest request;
    request.max_tokens = 512;
    request.temperature = 0.3f;
    request.top_p = 0.9f;
    request.repeat_penalty = 1.0f;
    request.stream = false;

    ChatMessage system_msg(MessageRole::System, system_prompt);
    request.messages.push_back(system_msg);

    ChatMessage user_msg(MessageRole::User, user_message);
    request.messages.push_back(user_msg);

    try {
        auto future_response = llama_interface_->create_chat_completion_async(request);
        auto status = future_response.wait_for(std::chrono::seconds(90));

        if (status == std::future_status::ready) {
            auto response = future_response.get();

            if (!response.choices.empty() && !response.choices[0].message.content.empty()) {
                std::string summary = response.choices[0].message.content;
                summary = trim_summary(summary);

                std::cout << "[SUMMARY] Local server summary: " << summary.size() << " chars"
                          << " (~" << estimate_tokens(summary) << " tokens)" << std::endl;

                return summary;
            } else {
                std::cerr << "[SUMMARY] Error: Empty response from local server" << std::endl;
                return "[Пустое резюме]";
            }
        } else {
            std::cerr << "[SUMMARY] Error: Request timeout (90s)" << std::endl;
            return "[Таймаут генерации резюме]";
        }
    } catch (const std::exception& e) {
        std::cerr << "[SUMMARY] Error: Request failed: " << e.what() << std::endl;
        return "[Ошибка: " + std::string(e.what()) + "]";
    }
}

} // namespace core
} // namespace llama_gui
