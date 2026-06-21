// ============================================================================
// ИЗМЕНЕНИЯ В generate_chunk_summary_openrouter() - ВЫБОР МОДЕЛИ ДЛЯ MAP
// Найти строку: params.model = openrouter_model_id_.empty() ? "openrouter/free" : openrouter_model_id_;
// Заменить на:
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

    // === ВЫБОР МОДЕЛИ ДЛЯ MAP-ЭТАПА ===
    DeepAnalysisSettings default_settings;
    std::string model_id = select_model_for_stage(RagAnalysisStage::Map, default_settings);
    
    std::cout << "[MODEL SELECT] Map stage using model: " << model_id << std::endl;

    // Создаём запрос к OpenRouter
    OpenRouterRequestParams params;
    params.model = model_id;  // Используем выбранную модель вместо openrouter_model_id_
    params.max_tokens = 512;
    params.temperature = 0.3f;  // Низкая температура для фактологического резюме
    params.top_p = 0.9f;
    params.stream = false;  // Синхронный ответ

    // Добавляем сообщения
    params.messages.push_back({"system", system_prompt});
    params.messages.push_back({"user", user_message});

    // ... остальной код без изменений ...
