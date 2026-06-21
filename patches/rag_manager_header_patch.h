// ============================================================================
// ИЗМЕНЕНИЯ В include/core/rag_manager.h
// Добавить в public секцию класса RagManager (после строки ~160)
// ============================================================================

    /**
     * @brief Проверка доступности OpenRouter API (НЕ тратит лимиты!)
     * @return true если API доступен
     */
    bool check_openrouter_availability();

    /**
     * @brief Проверка лимитов OpenRouter перед MapReduce (НЕ тратит лимиты!)
     * @param required_requests Требуемое количество запросов
     * @return true если лимиты позволяют
     */
    bool check_openrouter_rate_limit(int required_requests);

    /**
     * @brief Генерация резюме с fallback на локальный сервер
     * @param chunk Чанк для суммаризации
     * @param query Исходный запрос
     * @return Текст резюме
     */
    std::string generate_chunk_summary_with_fallback(const RagChunk& chunk, const std::string& query);

    /**
     * @brief Генерация резюме через локальный сервер
     * @param chunk Чанк для суммаризации
     * @param query Исходный запрос
     * @return Текст резюме
     */
    std::string generate_chunk_summary_local(const RagChunk& chunk, const std::string& query);
