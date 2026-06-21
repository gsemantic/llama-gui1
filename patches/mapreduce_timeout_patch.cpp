// ============================================================================
// ИЗМЕНЕНИЯ В process_deep_analysis_mapreduce()
// Добавить после строки: std::cout << "\n[MAP-REDUCE] === PHASE 1: MAP (Creating batch summaries) ===" << std::endl;
// ============================================================================

std::string RagManager::process_deep_analysis_mapreduce(
    const std::string& query,
    std::vector<RagChunk>& all_chunks,
    const DeepAnalysisSettings& settings)
{
    std::cout << "\n[MAP-REDUCE] === PHASE 1: MAP (Creating batch summaries) ===" << std::endl;

    // === УВЕЛИЧЕНИЕ ТАЙМАУТА ДЛЯ MAP-ЭТАПА (120 секунд) ===
    int original_timeout = 0;
    if (openrouter_client_) {
        original_timeout = 30000;  // Сохраняем оригинальный таймаут (предполагается)
        openrouter_client_->set_timeout(120000);  // 120 секунд для Map
        std::cout << "[MAP-REDUCE] Timeout set to 120s for MAP phase" << std::endl;
    }

    // Восстановление таймаута при выходе (RAII)
    class TimeoutRestorer {
    public:
        TimeoutRestorer(OpenRouterClient* client, int timeout) 
            : client_(client), timeout_(timeout) {}
        ~TimeoutRestorer() {
            if (client_ && timeout_ > 0) {
                client_->set_timeout(timeout_);
                std::cout << "[MAP-REDUCE] Timeout restored to " << timeout_ << "ms" << std::endl;
            }
        }
    private:
        OpenRouterClient* client_;
        int timeout_;
    };
    TimeoutRestorer restorer(openrouter_client_.get(), original_timeout);

    // ... остальной код метода без изменений ...
