#include "../include/core/rag_manager.h"
#include "../include/core/openrouter_client.h"
#include <iostream>
#include <algorithm>
#include <vector>

namespace llama_gui {
namespace core {

// ============================================================================
// БАЗА ДАННЫХ БЕСПЛАТНЫХ МОДЕЛЕЙ OPENROUTER
// ============================================================================

// Актуальный список бесплатных моделей: https://openrouter.ai/models?max_price=0

static const std::vector<FreeModelInfo> FREE_MODELS = {
    // === БЫСТРЫЕ МОДЕЛИ ДЛЯ MAP-ЭТАПА ===
    
    {
        "google/gemma-2-2b-it:free",
        "Gemma 2 2B (Free)",
        8192,
        5,  // Очень быстрая
        2,  // Базовое качество
        "map"  // Лучше всего для суммаризации
    },
    {
        "microsoft/phi-3-mini-128k-instruct:free",
        "Phi-3 Mini 128K (Free)",
        131072,  // Огромный контекст!
        4,
        3,
        "map"  // Хороша для больших чанков
    },
    
    // === БАЛАНС СКОРОСТЬ/КАЧЕСТВО ===
    
    {
        "google/gemma-2-9b-it:free",
        "Gemma 2 9B (Free)",
        8192,
        4,  // Быстрая
        4,  // Хорошее качество
        "both"  // Универсальная
    },
    {
        "meta-llama/llama-3-8b-instruct:free",
        "Llama-3 8B Instruct (Free)",
        8192,
        4,
        4,
        "both"
    },
    {
        "meta-llama/llama-3-1-8b-instruct:free",
        "Llama-3.1 8B Instruct (Free)",
        131072,
        4,
        4,
        "both"
    },
    
    // === КАЧЕСТВЕННЫЕ МОДЕЛИ ДЛЯ REDUCE-ЭТАПА ===
    
    {
        "nousresearch/hermes-2-pro-mistral-7b:free",
        "Hermes 2 Pro Mistral 7B (Free)",
        8192,
        3,  // Средняя скорость
        5,  // Отличное качество
        "reduce"
    },
    {
        "huggingface/zephyr-7b-beta:free",
        "Zephyr 7B Beta (Free)",
        4096,
        3,
        3,
        "reduce"
    },
    {
        "openchat/openchat-7b:free",
        "OpenChat 7B (Free)",
        8192,
        3,
        3,
        "reduce"
    },
    {
        "laseropen/mistral-7b-instruct:free",
        "Mistral 7B Instruct (Free)",
        8192,
        3,
        4,
        "reduce"
    }
};

// ============================================================================
// МЕТОДЫ ВЫБОРА МОДЕЛЕЙ
// ============================================================================

std::string RagManager::select_model_for_stage(
    RagAnalysisStage stage,
    const DeepAnalysisSettings& settings)
{
    // 1. Проверяем ручную настройку
    std::string manual_model = (stage == RagAnalysisStage::Map)
        ? settings.map_model
        : settings.reduce_model;
    
    if (!manual_model.empty()) {
        std::cout << "[MODEL SELECT] Using manual model for " 
                  << (stage == RagAnalysisStage::Map ? "Map" : "Reduce")
                  << ": " << manual_model << std::endl;
        return manual_model;
    }
    
    // 2. Автоматический выбор среди бесплатных
    if (settings.auto_select_models) {
        std::cout << "[MODEL SELECT] Auto-select enabled (FREE ONLY), profile: " 
                  << settings.performance_profile << std::endl;
        return get_free_model_for_profile(stage, settings.performance_profile);
    }
    
    // 3. Умолчание - баланс среди бесплатных
    std::string default_model = (stage == RagAnalysisStage::Map)
        ? "google/gemma-2-9b-it:free"
        : "meta-llama/llama-3-8b-instruct:free";
    
    std::cout << "[MODEL SELECT] Using default: " << default_model << std::endl;
    return default_model;
}

std::string RagManager::get_free_model_for_profile(
    RagAnalysisStage stage,
    const std::string& profile)
{
    // Фильтруем модели по назначению
    std::vector<const FreeModelInfo*> candidates;
    
    std::string stage_str = (stage == RagAnalysisStage::Map) ? "map" : "reduce";
    
    for (const auto& model : FREE_MODELS) {
        if (model.best_for == "both" || model.best_for == stage_str) {
            candidates.push_back(&model);
        }
    }
    
    if (candidates.empty()) {
        // Fallback на любые бесплатные
        for (const auto& model : FREE_MODELS) {
            candidates.push_back(&model);
        }
    }
    
    // Выбираем по профилю
    const FreeModelInfo* selected = nullptr;
    
    if (profile == "fast") {
        // Самая быстрая среди кандидатов
        auto it = std::max_element(candidates.begin(), candidates.end(),
            [](const auto* a, const auto* b) {
                return a->speed_rating < b->speed_rating;
            });
        selected = *it;
    }
    else if (profile == "quality") {
        // Самое высокое качество среди кандидатов
        auto it = std::max_element(candidates.begin(), candidates.end(),
            [](const auto* a, const auto* b) {
                return a->quality_rating < b->quality_rating;
            });
        selected = *it;
    }
    else if (profile == "economy") {
        // Минимальное количество токенов (самая маленькая модель)
        auto it = std::min_element(candidates.begin(), candidates.end(),
            [](const auto* a, const auto* b) {
                return a->context_length > b->context_length;  // Меньше контекст = меньше токенов
            });
        selected = *it;
    }
    else {
        // profile == "balanced" (по умолчанию)
        if (stage == RagAnalysisStage::Map) {
            // Для суммаризации: Gemma 2 9B - хороший баланс
            for (const auto* model : candidates) {
                if (model->id == "google/gemma-2-9b-it:free") {
                    selected = model;
                    break;
                }
            }
            // Fallback
            if (!selected) {
                selected = candidates[0];
            }
        } else {
            // Для синтеза: Llama-3 8B - лучшее качество
            for (const auto* model : candidates) {
                if (model->id == "meta-llama/llama-3-8b-instruct:free") {
                    selected = model;
                    break;
                }
            }
            // Fallback
            if (!selected) {
                selected = candidates[0];
            }
        }
    }
    
    if (selected) {
        std::cout << "[MODEL SELECT] " << (stage == RagAnalysisStage::Map ? "Map" : "Reduce")
                  << " stage → " << selected->id 
                  << " (Free tier, quality=" << selected->quality_rating 
                  << ", speed=" << selected->speed_rating << ")" << std::endl;
        return selected->id;
    }
    
    // Крайний fallback
    return "google/gemma-2-9b-it:free";
}

// ============================================================================
// УСТАНОВКА OPENROUTER КЛИЕНТА
// ============================================================================

void RagManager::set_openrouter_client(std::shared_ptr<OpenRouterClient> client) {
    openrouter_client_ = client;
    use_openrouter_for_rag_ = (client != nullptr);
    std::cout << "[RAG] OpenRouter client " 
              << (use_openrouter_for_rag_ ? "connected" : "disconnected") 
              << std::endl;
}

void RagManager::set_openrouter_model(const std::string& model_id) {
    openrouter_model_id_ = model_id;
    std::cout << "[RAG] OpenRouter model set to: " << model_id << std::endl;
}

} // namespace core
} // namespace llama_gui
