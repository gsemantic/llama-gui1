// ============================================================================
// ИЗМЕНЕНИЯ В synthesize_final_answer_openrouter() - ВЫБОР МОДЕЛИ ДЛЯ REDUCE
// Найти строку: params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
// Заменить на:
// ============================================================================

std::string RagManager::synthesize_final_answer_openrouter(
    const std::string& query,
    const std::vector<std::string>& summaries,
    int target_context_size)
{
    std::cout << "[SYNTHESIS] Using OpenRouter for final synthesis from " << summaries.size()
              << " summaries" << std::endl;

    // ... код увеличения таймаута (см. reduce_timeout_patch.cpp) ...

    // Собираем все резюме в один контекст
    std::string context;
    context.reserve(30000);

    for (size_t i = 0; i < summaries.size(); ++i) {
        context += "\n=== Источник " + std::to_string(i+1) + " ===\n";
        context += summaries[i];
    }

    // ... код проверки размера контекста ...

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

    // === ВЫБОР МОДЕЛИ ДЛЯ REDUCE-ЭТАПА ===
    DeepAnalysisSettings default_settings;
    std::string model_id = select_model_for_stage(RagAnalysisStage::Reduce, default_settings);
    
    std::cout << "[MODEL SELECT] Reduce stage using model: " << model_id << std::endl;

    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = model_id;  // Используем выбранную модель вместо openrouter_model_id_
    params.max_tokens = std::min(target_context_size / 2, 1024);
    params.temperature = 0.5f;  // Немного выше для связного текста
    params.top_p = 0.95f;
    params.stream = false;  // Синхронный ответ

    // Добавляем сообщения
    params.messages.push_back({"system", system_prompt});
    params.messages.push_back({"user", user_message});

    // ... остальной код без изменений ...
