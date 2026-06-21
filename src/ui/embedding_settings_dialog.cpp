#include "../include/ui/embedding_settings_dialog.h"
#include "../include/core/logger.h"
#include <imgui.h>
#include <cstring>
#include <iostream>

using namespace llama_gui::core;

namespace llama_gui {
namespace ui {

EmbeddingSettingsDialog::EmbeddingSettingsDialog(
    llama_gui::core::RagSettings& settings,
    std::shared_ptr<llama_gui::core::EmbeddingGenerator> embedding_generator)
    : settings_(settings)
    , embedding_generator_(embedding_generator) {

    // Инициализация буферов из настроек
    std::strncpy(api_key_buffer_, settings_.cloud_embedding_api_key.c_str(), sizeof(api_key_buffer_) - 1);
    api_key_buffer_[sizeof(api_key_buffer_) - 1] = '\0';
    
    std::strncpy(endpoint_url_buffer_, "", sizeof(endpoint_url_buffer_) - 1);
    
    std::strncpy(model_name_buffer_, settings_.cloud_embedding_model.c_str(), sizeof(model_name_buffer_) - 1);
    model_name_buffer_[sizeof(model_name_buffer_) - 1] = '\0';
    
    // Инициализация индексов
    selected_mode_index_ = static_cast<int>(settings_.embedding_mode);
    selected_provider_index_ = static_cast<int>(settings_.cloud_embedding_provider);
}

void EmbeddingSettingsDialog::open() {
    is_open_ = true;
    // Сбрасываем позицию и размер окна при каждом открытии,
    // чтобы окно не было невидимым из-за сохранённых настроек ImGui
    ImGui::SetNextWindowSize(ImVec2(700, 500), ImGuiCond_Always);
    ImGui::SetNextWindowPos(
        ImVec2(
            ImGui::GetIO().DisplaySize.x / 2.0f - 350.0f,
            ImGui::GetIO().DisplaySize.y / 2.0f - 250.0f
        ),
        ImGuiCond_Always
    );
    std::cout << "[EmbeddingSettingsDialog] Opened" << std::endl;
}

void EmbeddingSettingsDialog::close() {
    is_open_ = false;
}

void EmbeddingSettingsDialog::apply_settings() {
    // Применяем режим эмбеддинга
    settings_.embedding_mode = static_cast<EmbeddingMode>(selected_mode_index_);
    
    // Применяем провайдер
    settings_.cloud_embedding_provider = static_cast<CloudEmbeddingProvider>(selected_provider_index_);
    
    // Применяем API ключ
    settings_.cloud_embedding_api_key = api_key_buffer_;
    
    // Применяем модель
    settings_.cloud_embedding_model = model_name_buffer_;
    
    // Настраиваем генератор
    if (embedding_generator_) {
        embedding_generator_->set_mode(settings_.embedding_mode);
        embedding_generator_->set_cloud_priority(settings_.cloud_embedding_priority);
        embedding_generator_->enable_cache(settings_.enable_embedding_cache);
        embedding_generator_->set_cache_size(settings_.embedding_cache_size);
        
        // Конфигурируем облачный API
        CloudEmbeddingConfig config;
        config.provider = settings_.cloud_embedding_provider;
        config.api_key = settings_.cloud_embedding_api_key;
        config.model_name = settings_.cloud_embedding_model;
        config.timeout_ms = 30000;
        config.max_retries = 2;
        
        embedding_generator_->configure_cloud_provider(config);
    }
    
    std::cout << "[EmbeddingSettingsDialog] Settings applied" << std::endl;
    std::cout << "  Mode: " << static_cast<int>(settings_.embedding_mode) << std::endl;
    std::cout << "  Provider: " << static_cast<int>(settings_.cloud_embedding_provider) << std::endl;
    std::cout << "  Model: " << settings_.cloud_embedding_model << std::endl;
}

void EmbeddingSettingsDialog::check_cloud_availability() {
    if (!embedding_generator_) return;
    
    is_loading_ = true;
    cloud_available_ = embedding_generator_->check_cloud_availability();
    is_loading_ = false;
    
    std::cout << "[EmbeddingSettingsDialog] Cloud availability: " 
              << (cloud_available_ ? "✅ Available" : "❌ Unavailable") << std::endl;
}

void EmbeddingSettingsDialog::render() {
    if (!is_open_) {
        return;
    }

    if (!ImGui::Begin("Embedding Settings / Настройки эмбеддинга", &is_open_,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Вкладки
    if (ImGui::BeginTabBar("EmbeddingSettingsTabBar")) {
        
        // === Вкладка: General ===
        if (ImGui::BeginTabItem("General / Общие")) {
            current_tab_ = Tab::General;
            render_general_tab();
            ImGui::EndTabItem();
        }
        
        // === Вкладка: Cloud ===
        if (ImGui::BeginTabItem("Cloud API / Облако")) {
            current_tab_ = Tab::Cloud;
            render_cloud_tab();
            ImGui::EndTabItem();
        }
        
        // === Вкладка: Local ===
        if (ImGui::BeginTabItem("Local / Локально")) {
            current_tab_ = Tab::Local;
            render_local_tab();
            ImGui::EndTabItem();
        }
        
        // === Вкладка: Cache ===
        if (ImGui::BeginTabItem("Cache / Кэш")) {
            current_tab_ = Tab::Cache;
            render_cache_tab();
            ImGui::EndTabItem();
        }
        
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    
    // Кнопки управления
    float button_width = 120.0f;
    float spacing = 10.0f;
    float total_width = 3.0f * button_width + 2.0f * spacing;
    float available_width = ImGui::GetContentRegionAvail().x;
    float start_x = ImGui::GetCursorPosX() + (available_width - total_width) / 2.0f;
    
    ImGui::SetCursorPosX(start_x);
    
    if (ImGui::Button("Apply/Применить", ImVec2(button_width, 0))) {
        apply_settings();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Check API/Проверить", ImVec2(button_width, 0))) {
        check_cloud_availability();
    }
    
    ImGui::SameLine();
    
    if (ImGui::Button("Close/Закрыть", ImVec2(button_width, 0))) {
        close();
    }
    
    // Статус
    ImGui::Spacing();
    if (embedding_generator_) {
        auto stats = embedding_generator_->get_stats();
        ImGui::Text("Status: %s | Requests: %zu | Cache hits: %zu",
                    embedding_generator_->is_loaded() ? "✅ Loaded" : "❌ Not loaded",
                    stats.total_requests,
                    stats.cache_hits);
    }

    ImGui::End();
}

void EmbeddingSettingsDialog::render_general_tab() {
    ImGui::Text("Embedding Mode / Режим эмбеддинга:");
    ImGui::Separator();
    ImGui::Spacing();
    
    const char* modes[] = {
        "Local Only (Только локально)",
        "Cloud Only (Только облако)",
        "Hybrid (Гибридный)",
        "Auto (Авто)"
    };
    
    if (ImGui::Combo("##ModeCombo", &selected_mode_index_, modes, 4)) {
        on_mode_changed(selected_mode_index_);
    }
    
    ImGui::TextWrapped("Mode description:");
    switch (static_cast<EmbeddingMode>(selected_mode_index_)) {
        case EmbeddingMode::LocalOnly:
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                "Use only local GGUF model. No internet required, but slower.");
            break;
        case EmbeddingMode::CloudOnly:
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                "Use only cloud API. Faster, better quality, requires internet.");
            break;
        case EmbeddingMode::Hybrid:
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                "Use cloud with local fallback. Best of both worlds.");
            break;
        case EmbeddingMode::Auto:
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
                "Automatically select based on availability.");
            break;
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Приоритет облака
    ImGui::Checkbox("Cloud Priority / Приоритет облака", &settings_.cloud_embedding_priority);
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Информация о совместимости
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "⚠ Compatibility Note:");
    ImGui::TextWrapped(
        "For embeddings to be compatible between cloud and local models, "
        "you MUST use the SAME model (e.g., all-MiniLM-L6-v2) in both modes. "
        "Different models produce vectors in different spaces!");
}

void EmbeddingSettingsDialog::render_cloud_tab() {
    ImGui::Text("Cloud Provider / Облачный провайдер:");
    ImGui::Separator();
    ImGui::Spacing();
    
    const char* providers[] = {
        "HuggingFace (Free)",
        "OpenRouter",
        "Custom API"
    };
    
    if (ImGui::Combo("##ProviderCombo", &selected_provider_index_, providers, 3)) {
        on_provider_changed(selected_provider_index_);
    }
    
    ImGui::Spacing();
    
    // API Key
    ImGui::Text("API Key:");
    ImGui::SetNextItemWidth(400.0f);
    ImGui::InputText("##APIKey", api_key_buffer_, sizeof(api_key_buffer_));
    
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
        "HuggingFace: Free tier available. Get token from huggingface.co/settings/tokens");
    
    ImGui::Spacing();
    
    // Model Name
    ImGui::Text("Model Name:");
    ImGui::SetNextItemWidth(400.0f);
    ImGui::InputText("##ModelName", model_name_buffer_, sizeof(model_name_buffer_));
    
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
        "Recommended: BGE-M3 (1024 dimensions, multilingual, high quality)");
    
    ImGui::Spacing();
    
    // Custom Endpoint (для Custom provider)
    if (selected_provider_index_ == 2) {  // Custom
        ImGui::Text("Custom Endpoint URL:");
        ImGui::SetNextItemWidth(400.0f);
        ImGui::InputText("##EndpointURL", endpoint_url_buffer_, sizeof(endpoint_url_buffer_));
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Статус доступности
    ImGui::Text("Cloud API Status:");
    if (is_loading_) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Checking...");
    } else if (cloud_available_) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✅ Available");
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "❌ Unavailable (Click 'Check API' to test)");
    }
}

void EmbeddingSettingsDialog::render_local_tab() {
    ImGui::Text("Local Model Settings / Настройки локальной модели:");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Text("Model Path:");
    ImGui::SetNextItemWidth(500.0f);
    ImGui::InputText("##ModelPath", settings_.embedding_model_path.data(), 
                     settings_.embedding_model_path.size(),
                     ImGuiInputTextFlags_ReadOnly);
    
    ImGui::Spacing();
    
    if (embedding_generator_) {
        ImGui::Text("Current Status:");
        ImGui::Text("  - Loaded: %s", embedding_generator_->is_local_loaded() ? "✅ Yes" : "❌ No");
        ImGui::Text("  - Dimension: %d", embedding_generator_->get_embedding_dimension());
        ImGui::Text("  - Model: %s", embedding_generator_->get_model_name().c_str());
    }
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
        "Download model from: https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2");
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
        "Convert to GGUF using: python convert-hf-to-gguf.py");
}

void EmbeddingSettingsDialog::render_cache_tab() {
    ImGui::Text("Embedding Cache Settings / Настройки кэша эмбеддингов:");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::Checkbox("Enable Cache / Включить кэш", &settings_.enable_embedding_cache);
    
    ImGui::Spacing();
    
    ImGui::Text("Cache Size (entries):");
    ImGui::SetNextItemWidth(200.0f);
    ImGui::SliderInt("##CacheSize", &settings_.embedding_cache_size, 100, 2000);
    
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), 
        "Larger cache = fewer API calls, but more memory usage");
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // Статистика кэша
    if (embedding_generator_) {
        auto stats = embedding_generator_->get_stats();
        
        ImGui::Text("Cache Statistics:");
        ImGui::Text("  - Current entries: %zu", embedding_generator_->get_cache_size());
        ImGui::Text("  - Total requests: %zu", stats.total_requests);
        ImGui::Text("  - Cache hits: %zu (%.1f%%)", 
                    stats.cache_hits,
                    stats.total_requests > 0 ? 
                        100.0f * stats.cache_hits / stats.total_requests : 0.0f);
    }
    
    ImGui::Spacing();
    
    if (ImGui::Button("Clear Cache / Очистить кэш")) {
        if (embedding_generator_) {
            embedding_generator_->clear_cache();
        }
    }
}

std::string EmbeddingSettingsDialog::get_mode_name(EmbeddingMode mode) const {
    switch (mode) {
        case EmbeddingMode::LocalOnly: return "Local Only";
        case EmbeddingMode::CloudOnly: return "Cloud Only";
        case EmbeddingMode::Hybrid: return "Hybrid";
        case EmbeddingMode::Auto: return "Auto";
        default: return "Unknown";
    }
}

std::string EmbeddingSettingsDialog::get_provider_name(CloudEmbeddingProvider provider) const {
    switch (provider) {
        case CloudEmbeddingProvider::HuggingFace: return "HuggingFace";
        case CloudEmbeddingProvider::OpenRouter: return "OpenRouter";
        case CloudEmbeddingProvider::Custom: return "Custom";
        default: return "Unknown";
    }
}

void EmbeddingSettingsDialog::on_mode_changed(int index) {
    std::cout << "[EmbeddingSettingsDialog] Mode changed to: " << index << std::endl;
}

void EmbeddingSettingsDialog::on_provider_changed(int index) {
    std::cout << "[EmbeddingSettingsDialog] Provider changed to: " << index << std::endl;
}

} // namespace ui
} // namespace llama_gui
