#include "../include/ui/openrouter_models_dialog.h"
#include "../include/core/logger.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace llama_gui::core;

namespace llama_gui {
namespace ui {

OpenRouterModelsDialog::OpenRouterModelsDialog(llama_gui::core::Settings& settings)
    : settings_(settings)
    , client_(settings.get_openrouter_api_key()) {

    // Установка таймаута из настроек
    client_.set_timeout(settings.openrouter().timeout_ms);

    // Установка кастомного URL если указан
    if (!settings.openrouter().custom_base_url.empty()) {
        client_.set_base_url(settings.openrouter().custom_base_url);
    }

    // Инициализация состояния фильтров
    free_models_only_ = settings.openrouter().free_models_only;
    std::strncpy(search_buffer_, settings.openrouter().last_search_query.c_str(), sizeof(search_buffer_) - 1);
    search_buffer_[sizeof(search_buffer_) - 1] = '\0';
}

void OpenRouterModelsDialog::open() {
    is_open_ = true;
    load_models();
}

void OpenRouterModelsDialog::close() {
    is_open_ = false;

    // Сохранение состояния фильтров
    auto& openrouter_settings = settings_.openrouter();
    openrouter_settings.free_models_only = free_models_only_;
    openrouter_settings.last_search_query = search_buffer_;
}

void OpenRouterModelsDialog::render() {
    if (!is_open_) {
        return;
    }

    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    
    // Используем обычное окно (не popup) - оно будет независимым
    if (!ImGui::Begin("OpenRouter Models", &is_open_, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }
    
    // Заголовок с описанием
    ImGui::Text("Выберите модель OpenRouter для использования в чате.");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Панель управления (поиск и фильтры)
    ImGui::BeginChild("Toolbar", ImVec2(0, ImGui::GetFrameHeightWithSpacing() * 2.5f), false);
    
    // Поиск
    ImGui::Text("Поиск:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##Search", search_buffer_, IM_ARRAYSIZE(search_buffer_), ImGuiInputTextFlags_EnterReturnsTrue)) {
        search_models(search_buffer_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Найти")) {
        search_models(search_buffer_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Сброс")) {
        search_buffer_[0] = '\0';
        search_models("");
    }
    
    // Фильтры
    ImGui::Spacing();
    ImGui::Checkbox("Только бесплатные модели", &free_models_only_);
    ImGui::SameLine();
    if (ImGui::Button("Обновить список")) {
        if (free_models_only_) {
            load_free_models();
        } else {
            load_models();
        }
    }
    ImGui::SameLine();
    if (is_loading_) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Загрузка...");
    }
    
    // Статус
    ImGui::SameLine(ImGui::GetWindowWidth() - 200);
    ImGui::Text("%zu моделей", filtered_models_.size());
    
    ImGui::EndChild();
    ImGui::Separator();
    
    // Список моделей
    render_model_list();
    
    // Кнопки управления
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 250);
    
    if (ImGui::Button("OK", ImVec2(100, 0))) {
        if (selected_model_index_ >= 0 && selected_model_index_ < static_cast<int>(filtered_models_.size())) {
            on_model_selected(filtered_models_[selected_model_index_]);
        }
        close();
    }
    ImGui::SameLine();
    if (ImGui::Button("Отмена", ImVec2(100, 0))) {
        close();
    }
    ImGui::SameLine();
    if (ImGui::Button("Применить", ImVec2(100, 0))) {
        if (selected_model_index_ >= 0 && selected_model_index_ < static_cast<int>(filtered_models_.size())) {
            on_model_selected(filtered_models_[selected_model_index_]);
        }
    }

    ImGui::End();
}

void OpenRouterModelsDialog::render_inline() {
    // Отрисовка таблицы моделей inline (внутри другого окна ImGui)
    if (filtered_models_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), 
            is_loading_ ? "Загрузка списка моделей..." : "Модели не найдены. Нажмите 'Обновить'");
        return;
    }

    // Ограничиваем высоту таблицы
    float max_height = ImGui::GetFontSize() * 12;  // Максимум 12 строк
    ImGui::BeginChild("InlineModelsList", ImVec2(0, max_height), true);

    if (ImGui::BeginTable("InlineModelsTable", 4, 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
        ImGuiTableFlags_ScrollY)) {
        
        ImGui::TableSetupColumn("Название", ImGuiTableColumnFlags_WidthStretch, 40);
        ImGui::TableSetupColumn("Провайдер", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Контекст", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("Цена", ImGuiTableColumnFlags_WidthFixed, 80);
        
        ImGui::TableHeadersRow();
        
        for (size_t i = 0; i < filtered_models_.size(); ++i) {
            const auto& model = filtered_models_[i];
            
            ImGui::PushID(static_cast<int>(i));
            
            // Колонка 0: Название + кнопка выбора
            ImGui::TableNextColumn();
            
            bool is_selected = (model.id == settings_.openrouter().selected_model);
            if (is_selected) {
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 1.0f, 0.0f, 1.0f));
            }
            
            if (ImGui::Selectable(model.name.c_str(), is_selected, ImGuiSelectableFlags_SpanAllColumns)) {
                // Выбор модели
                settings_.openrouter().selected_model = model.id;
                settings_.openrouter().enabled = true;
                
                // Добавление в недавние
                auto& recent = settings_.openrouter().recent_models;
                auto it = std::find(recent.begin(), recent.end(), model.id);
                if (it != recent.end()) {
                    recent.erase(it);
                }
                recent.insert(recent.begin(), model.id);
                if (recent.size() > 10) {
                    recent.pop_back();
                }
            }
            
            if (is_selected) {
                ImGui::PopStyleColor();
            }
            
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("Дважды кликните для выбора");
            }
            
            if (ImGui::IsMouseDoubleClicked(0) && ImGui::IsItemHovered()) {
                settings_.openrouter().selected_model = model.id;
                settings_.openrouter().enabled = true;
            }
            
            // Колонка 1: Провайдер
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(model.provider.c_str());
            
            // Колонка 2: Контекст
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(format_context_size(model.context_length).c_str());
            
            // Колонка 3: Цена
            ImGui::TableNextColumn();
            if (model.is_free) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Бесплатно");
            } else {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(4);
                oss << "$" << model.prompt_price_usd_per_million << "/1M";
                ImGui::TextUnformatted(oss.str().c_str());
            }
            
            ImGui::PopID();
        }
        
        ImGui::EndTable();
    }

    ImGui::EndChild();
    
    // Информация о количестве моделей
    ImGui::Text("%zu моделей (показано)", filtered_models_.size());
}

void OpenRouterModelsDialog::load_models() {
    if (is_loading_) {
        return;
    }
    
    is_loading_ = true;
    models_.clear();
    filtered_models_.clear();
    selected_model_index_ = -1;
    
    client_.get_models_async([this](const core::OpenRouterModelsResponse& response) {
        on_models_received(response);
    });
}

void OpenRouterModelsDialog::load_free_models() {
    if (is_loading_) {
        return;
    }
    
    is_loading_ = true;
    models_.clear();
    filtered_models_.clear();
    selected_model_index_ = -1;
    free_models_only_ = true;
    
    client_.get_free_models_async([this](const core::OpenRouterModelsResponse& response) {
        on_models_received(response);
    });
}

void OpenRouterModelsDialog::search_models(const std::string& query, bool free_only) {
    if (is_loading_) {
        return;
    }

    is_loading_ = true;
    models_.clear();
    filtered_models_.clear();
    selected_model_index_ = -1;
    free_models_only_ = free_only;

    client_.search_models_async(query, free_only, [this](const core::OpenRouterModelsResponse& response) {
        on_models_received(response);
    });
}

void OpenRouterModelsDialog::on_models_received(const llama_gui::core::OpenRouterModelsResponse& response) {
    is_loading_ = false;
    
    if (!response.success) {
        LOG_ERROR("OpenRouter: Ошибка загрузки моделей: " + response.error);
        return;
    }
    
    models_ = response.models;
    apply_filters();
    
    LOG_INFO("OpenRouter: Загружено " + std::to_string(models_.size()) + " моделей");
}

void OpenRouterModelsDialog::apply_filters() {
    std::string query = search_buffer_;
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);
    
    filtered_models_.clear();
    
    for (const auto& model : models_) {
        // Фильтр по бесплатности
        if (free_models_only_ && !model.is_free) {
            continue;
        }
        
        // Поиск по имени
        if (!query.empty()) {
            std::string name_lower = model.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            
            std::string id_lower = model.id;
            std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);
            
            if (name_lower.find(query) == std::string::npos && 
                id_lower.find(query) == std::string::npos) {
                continue;
            }
        }
        
        filtered_models_.push_back(model);
    }
    
    // Сортировка по имени
    std::sort(filtered_models_.begin(), filtered_models_.end());
}

void OpenRouterModelsDialog::render_model_list() {
    if (filtered_models_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
            is_loading_ ? "Загрузка списка моделей..." : "Модели не найдены");
        return;
    }
    
    // Таблица моделей
    ImGui::BeginChild("ModelsList", ImVec2(0, -ImGui::GetFrameHeightWithSpacing() * 4), false);
    
    if (ImGui::BeginTable("ModelsTable", 5, 
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
        ImGuiTableFlags_Hideable | ImGuiTableFlags_ScrollY)) {
        
        ImGui::TableSetupColumn("Название", ImGuiTableColumnFlags_WidthStretch, 30);
        ImGui::TableSetupColumn("Провайдер", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Контекст", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Цена", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Бесплатно", ImGuiTableColumnFlags_WidthFixed, 70);
        
        ImGui::TableHeadersRow();
        
        for (size_t i = 0; i < filtered_models_.size(); ++i) {
            const auto& model = filtered_models_[i];
            
            ImGui::PushID(static_cast<int>(i));
            
            if (ImGui::Selectable(
                model.name.c_str(),
                selected_model_index_ == static_cast<int>(i),
                ImGuiSelectableFlags_SpanAllColumns,
                ImVec2(0, ImGui::GetTextLineHeightWithSpacing() + 4)
            )) {
                selected_model_index_ = static_cast<int>(i);
            }
            
            if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
                on_model_selected(model);
                close();
            }
            
            // Колонка 0: Название
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(model.name.c_str());
            
            // Колонка 1: Провайдер
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(model.provider.c_str());
            
            // Колонка 2: Контекст
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(format_context_size(model.context_length).c_str());
            
            // Колонка 3: Цена
            ImGui::TableNextColumn();
            if (model.is_free) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Бесплатно");
            } else {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(4);
                oss << "$" << model.prompt_price_usd_per_million << "/1M tokens";
                ImGui::TextUnformatted(oss.str().c_str());
            }
            
            // Колонка 4: Бесплатно
            ImGui::TableNextColumn();
            if (model.is_free) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓");
            } else {
                ImGui::TextDisabled("✗");
            }
            
            ImGui::PopID();
        }
        
        ImGui::EndTable();
    }
    
    ImGui::EndChild();
}

void OpenRouterModelsDialog::on_model_selected(const core::OpenRouterModel& model) {
    auto& openrouter_settings = settings_.openrouter();
    openrouter_settings.selected_model = model.id;
    openrouter_settings.enabled = true;
    
    // Добавление в недавние модели
    auto it = std::find(openrouter_settings.recent_models.begin(), 
                        openrouter_settings.recent_models.end(), 
                        model.id);
    if (it != openrouter_settings.recent_models.end()) {
        openrouter_settings.recent_models.erase(it);
    }
    openrouter_settings.recent_models.insert(openrouter_settings.recent_models.begin(), model.id);
    if (openrouter_settings.recent_models.size() > 10) {
        openrouter_settings.recent_models.pop_back();
    }
    
    LOG_INFO("OpenRouter: Выбрана модель " + model.id);
}

std::string OpenRouterModelsDialog::format_price(double price) const {
    if (price == 0.0) {
        return "Бесплатно";
    }
    
    std::ostringstream oss;
    oss << "$" << std::fixed << std::setprecision(6) << price;
    return oss.str();
}

std::string OpenRouterModelsDialog::format_context_size(int64_t context) const {
    if (context <= 0) {
        return "N/A";
    }
    
    if (context >= 1000000) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(1) << (context / 1000000.0) << "M";
        return oss.str();
    }
    
    if (context >= 1000) {
        std::ostringstream oss;
        oss << (context / 1000) << "K";
        return oss.str();
    }
    
    return std::to_string(context);
}

} // namespace ui
} // namespace llama_gui
