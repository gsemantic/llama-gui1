// ============================================================================
// ИЗМЕНЕНИЯ В synthesize_final_answer_openrouter()
// Добавить после строки: std::cout << "[SYNTHESIS] Using OpenRouter for final synthesis from " << summaries.size() << " summaries" << std::endl;
// ============================================================================

std::string RagManager::synthesize_final_answer_openrouter(
    const std::string& query,
    const std::vector<std::string>& summaries,
    int target_context_size)
{
    std::cout << "[SYNTHESIS] Using OpenRouter for final synthesis from " << summaries.size()
              << " summaries" << std::endl;

    // === УВЕЛИЧЕНИЕ ТАЙМАУТА ДЛЯ REDUCE-ЭТАПА (180 секунд) ===
    int original_timeout = 0;
    if (openrouter_client_) {
        original_timeout = 30000;  // Сохраняем оригинальный таймаут (предполагается)
        openrouter_client_->set_timeout(180000);  // 180 секунд для Reduce
        std::cout << "[SYNTHESIS] Timeout set to 180s for REDUCE phase" << std::endl;
    }

    // Восстановление таймаута при выходе (RAII)
    class TimeoutRestorer {
    public:
        TimeoutRestorer(OpenRouterClient* client, int timeout) 
            : client_(client), timeout_(timeout) {}
        ~TimeoutRestorer() {
            if (client_ && timeout_ > 0) {
                client_->set_timeout(timeout_);
                std::cout << "[SYNTHESIS] Timeout restored to " << timeout_ << "ms" << std::endl;
            }
        }
    private:
        OpenRouterClient* client_;
        int timeout_;
    };
    TimeoutRestorer restorer(openrouter_client_.get(), original_timeout);

    // ... остальной код метода без изменений ...
