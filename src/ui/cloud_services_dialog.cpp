#include "../include/ui/cloud_services_dialog.h"
#include "../include/core/logger.h"
#include "../include/core/env_file_handler.h"
#include "../include/ui/localization_manager.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

using namespace llama_gui::core;

namespace llama_gui {
namespace ui {

CloudServicesDialog::CloudServicesDialog(llama_gui::core::Settings& settings)
    : settings_(settings)
    , openrouter_client_(std::make_unique<llama_gui::core::OpenRouterClient>(settings.get_openrouter_api_key()))
    , kilocode_client_(std::make_unique<llama_gui::core::KiloCodeClient>(settings.get_kilocode_api_key()))
    , universal_client_(std::make_unique<llama_gui::core::UniversalOpenAIClient>(settings.get_universal_openai_api_key())) {

    // Инициализация Universal клиента
    universal_client_->set_timeout(settings.universal_openai().timeout_ms);
    if (!settings.universal_openai().custom_base_url.empty()) {
        universal_client_->set_base_url(settings.universal_openai().custom_base_url);
    }
    if (!settings.universal_openai().custom_endpoint.empty()) {
        universal_client_->set_endpoint(settings.universal_openai().custom_endpoint);
    }

    // Установка таймаута из настроек
    openrouter_client_->set_timeout(settings.openrouter().timeout_ms);

    // Установка кастомного URL если указан
    if (!settings.openrouter().custom_base_url.empty()) {
        openrouter_client_->set_base_url(settings.openrouter().custom_base_url);
    }

    // Инициализация состояния фильтров
    free_models_only_ = settings.openrouter().free_models_only;
    std::strncpy(search_buffer_, settings.openrouter().last_search_query.c_str(), sizeof(search_buffer_) - 1);
    search_buffer_[sizeof(search_buffer_) - 1] = '\0';

    // Инициализация буфера API ключа
    std::string api_key = settings.get_openrouter_api_key();
    std::strncpy(api_key_buffer_, api_key.c_str(), sizeof(api_key_buffer_) - 1);
    api_key_buffer_[sizeof(api_key_buffer_) - 1] = '\0';

    // Инициализация буфера кастомного URL
    std::strncpy(custom_url_buffer_, settings.openrouter().custom_base_url.c_str(), sizeof(custom_url_buffer_) - 1);
    custom_url_buffer_[sizeof(custom_url_buffer_) - 1] = '\0';

    // Инициализация KiloCode
    kilocode_use_tor_ = settings.kilocode().use_tor;
    kilocode_client_->use_proxy(kilocode_use_tor_);
    kilocode_client_->set_proxy_url(settings.kilocode().tor_proxy_url);
    kilocode_client_->set_timeout(settings.kilocode().timeout_ms);
    std::string kilocode_api_key = settings.get_kilocode_api_key();
    std::strncpy(kilocode_api_key_buffer_, kilocode_api_key.c_str(), sizeof(kilocode_api_key_buffer_) - 1);
    kilocode_api_key_buffer_[sizeof(kilocode_api_key_buffer_) - 1] = '\0';
    std::strncpy(kilocode_tor_proxy_buffer_, settings.kilocode().tor_proxy_url.c_str(), sizeof(kilocode_tor_proxy_buffer_) - 1);
    kilocode_tor_proxy_buffer_[sizeof(kilocode_tor_proxy_buffer_) - 1] = '\0';
    std::strncpy(kilocode_search_buffer_, settings.kilocode().last_search_query.c_str(), sizeof(kilocode_search_buffer_) - 1);
    kilocode_search_buffer_[sizeof(kilocode_search_buffer_) - 1] = '\0';

    // Инициализация Universal
    std::string universal_api_key = settings.get_universal_openai_api_key();
    std::strncpy(universal_api_key_buffer_, universal_api_key.c_str(), sizeof(universal_api_key_buffer_) - 1);
    universal_api_key_buffer_[sizeof(universal_api_key_buffer_) - 1] = '\0';
    std::strncpy(universal_base_url_buffer_, settings.universal_openai().custom_base_url.c_str(), sizeof(universal_base_url_buffer_) - 1);
    universal_base_url_buffer_[sizeof(universal_base_url_buffer_) - 1] = '\0';
    std::strncpy(universal_endpoint_buffer_, settings.universal_openai().custom_endpoint.c_str(), sizeof(universal_endpoint_buffer_) - 1);
    universal_endpoint_buffer_[sizeof(universal_endpoint_buffer_) - 1] = '\0';
    std::strncpy(universal_search_buffer_, settings.universal_openai().last_search_query.c_str(), sizeof(universal_search_buffer_) - 1);
    universal_search_buffer_[sizeof(universal_search_buffer_) - 1] = '\0';
    universal_free_models_only_ = settings.universal_openai().free_models_only;
}

void CloudServicesDialog::open() {
    is_open_ = true;
    show_model_list_ = false;
    show_usage_stats_ = false;
    std::cout << "[CloudServicesDialog] open() called, is_open_ = " << is_open_ << std::endl;
    std::cout << "[CloudServicesDialog] Window should appear at (200, 50)" << std::endl;
}

void CloudServicesDialog::close() {
    is_open_ = false;
    // Сохранение настроек в settings НЕ происходит автоматически
    // Пользователь должен явно нажать "OK" или "Применить"
}

void CloudServicesDialog::update_openrouter_settings() {
    // Обновление API ключа из .env файла
    std::string api_key = settings_.get_openrouter_api_key();
    openrouter_client_->set_api_key(api_key);

    // Обновление таймаута
    openrouter_client_->set_timeout(settings_.openrouter().timeout_ms);

    // Обновление кастомного URL если указан
    if (!settings_.openrouter().custom_base_url.empty()) {
        openrouter_client_->set_base_url(settings_.openrouter().custom_base_url);
    }

    // Обновление буфера API ключа
    std::strncpy(api_key_buffer_, api_key.c_str(), sizeof(api_key_buffer_) - 1);
    api_key_buffer_[sizeof(api_key_buffer_) - 1] = '\0';

    std::cout << "[CloudServicesDialog] OpenRouter settings updated, API key: "
              << (api_key.empty() ? "НЕТ" : api_key.substr(0, 8) + "...") << std::endl;
}

void CloudServicesDialog::render() {
    if (!is_open_) {
        return;
    }

    // Устанавливаем размер и позицию для независимого окна
    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(200, 50), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Cloud Services / Облачные сервисы", &is_open_,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Вкладки облачных провайдеров
    ImGui::Text("Выберите облачный сервис:");
    ImGui::Separator();
    ImGui::Spacing();

    // Вкладки облачных провайдеров
    if (ImGui::BeginTabBar("CloudProvidersTabBar")) {
        if (ImGui::TabItemButton("OpenRouter")) {
            selected_provider_ = CloudProvider::OpenRouter;
        }
        if (ImGui::TabItemButton("KiloCode")) {
            selected_provider_ = CloudProvider::KiloCode;
        }
        if (ImGui::TabItemButton("Universal")) {
            selected_provider_ = CloudProvider::Universal;
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Отрисовка выбранной вкладки
    switch (selected_provider_) {
        case CloudProvider::OpenRouter:
            render_openrouter_tab();
            break;
        case CloudProvider::KiloCode:
            render_kilocode_tab();
            break;
        case CloudProvider::Universal:
            render_universal_tab();
            break;
    }

    ImGui::End();
}

void CloudServicesDialog::render_openrouter_tab() {
    auto& openrouter_settings = settings_.openrouter();

    ImGui::Text("Настройки OpenRouter API");
    ImGui::Separator();

    // Включение OpenRouter — автоматически отключает другие провайдеры
    if (ImGui::Checkbox("Использовать OpenRouter", &openrouter_settings.enabled)) {
        if (openrouter_settings.enabled) {
            settings_.kilocode().enabled = false;
            if (openrouter_settings.selected_model.empty()) {
                load_models();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("При включении этой опции запросы будут отправляться в OpenRouter API вместо локального сервера");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // API ключ
    ImGui::Text("API Key:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##api_key", api_key_buffer_, sizeof(api_key_buffer_), ImGuiInputTextFlags_Password)) {
        openrouter_client_->set_api_key(api_key_buffer_);
        // Сохраняем ключ в .env файл
        settings_.update_openrouter_api_key(api_key_buffer_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste##api_key")) {
        if (ImGui::GetClipboardText()) {
            std::strncpy(api_key_buffer_, ImGui::GetClipboardText(), sizeof(api_key_buffer_) - 1);
            api_key_buffer_[sizeof(api_key_buffer_) - 1] = '\0';
            openrouter_client_->set_api_key(api_key_buffer_);
            // Сохраняем ключ в .env файл
            settings_.update_openrouter_api_key(api_key_buffer_);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("API ключ OpenRouter (опционально, требуется для платных моделей). Ключ хранится в файле .env");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // Кастомный URL
    ImGui::Text("Custom Base URL (optional):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##custom_url", custom_url_buffer_, sizeof(custom_url_buffer_))) {
        openrouter_settings.custom_base_url = custom_url_buffer_;
        openrouter_client_->set_base_url(custom_url_buffer_);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("Кастомный базовый URL для OpenRouter API (оставьте пустым для использования URL по умолчанию)");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // Таймаут
    ImGui::Text("Request Timeout (ms):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputInt("##timeout", &openrouter_settings.timeout_ms)) {
        openrouter_client_->set_timeout(openrouter_settings.timeout_ms);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Выбор модели
    ImGui::Text("Модель:");
    ImGui::SameLine();

    // Кнопка для открытия списка моделей
    if (ImGui::Button("Выбрать модель...")) {
        show_model_list_ = true;
        load_models();
    }
    ImGui::SameLine();

    // Отображение текущей выбранной модели
    if (!openrouter_settings.selected_model.empty()) {
        ImGui::Text("Выбрано: %s", openrouter_settings.selected_model.c_str());
    } else {
        ImGui::TextDisabled("Модель не выбрана");
    }

    ImGui::Spacing();

    // Недавние модели (combo)
    auto& recent_models = openrouter_settings.recent_models;
    if (!recent_models.empty()) {
        ImGui::Text("Недавние модели:");
        ImGui::SameLine();
        
        std::vector<const char*> model_items;
        std::vector<std::string> model_storage;
        
        for (const auto& m : recent_models) {
            if (!m.empty()) {
                model_storage.push_back(m);
                model_items.push_back(model_storage.back().c_str());
            }
        }

        if (!model_items.empty()) {
            ImGui::SetNextItemWidth(300);
            if (ImGui::BeginCombo("##recent_models", "")) {
                for (size_t i = 0; i < model_items.size(); i++) {
                    if (ImGui::Selectable(model_items[i], false)) {
                        openrouter_settings.selected_model = model_storage[i];
                        openrouter_settings.enabled = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Фильтры и список моделей (inline)
    if (ImGui::CollapsingHeader("Список моделей", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        // Поиск
        ImGui::Text("Поиск:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputText("##Search", search_buffer_, sizeof(search_buffer_), ImGuiInputTextFlags_EnterReturnsTrue)) {
            search_models(search_buffer_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Найти##search")) {
            search_models(search_buffer_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Сброс##reset")) {
            search_buffer_[0] = '\0';
            search_models("");
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // Фильтр бесплатных моделей
        if (ImGui::Checkbox("Только бесплатные", &free_models_only_)) {
            if (free_models_only_) {
                load_free_models();
            } else {
                load_models();
            }
        }

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

        ImGui::SameLine(ImGui::GetWindowWidth() - 150);
        ImGui::Text("%zu моделей", filtered_models_.size());

        ImGui::Spacing();

        // Список моделей inline
        render_model_list();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Статистика использования
    if (ImGui::CollapsingHeader("Статистика использования")) {
        ImGui::Spacing();

        if (openrouter_settings.enabled && !api_key_buffer_[0]) {
            if (ImGui::Button("Обновить статистику")) {
                refresh_usage_stats();
            }
            ImGui::SameLine();

            if (openrouter_settings.usage_remaining > 0 || openrouter_settings.usage_total_requests > 0) {
                ImGui::Text("Запросов сегодня: %d / %d",
                    openrouter_settings.usage_total_requests,
                    openrouter_settings.usage_limit);
                ImGui::Text("Осталось: %d", openrouter_settings.usage_remaining);
            }
        } else {
            ImGui::TextDisabled("Включите OpenRouter и укажите API ключ для просмотра статистики");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Кнопки управления
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 350);
    if (ImGui::Button("OK", ImVec2(100, 0))) {
        // Сохранение настроек OpenRouter
        auto& openrouter_settings = settings_.openrouter();
        openrouter_settings.custom_base_url = custom_url_buffer_;
        openrouter_settings.free_models_only = free_models_only_;
        openrouter_settings.last_search_query = search_buffer_;
        openrouter_client_->set_api_key(api_key_buffer_);
        
        // Сохраняем ключ в .env файл
        settings_.update_openrouter_api_key(api_key_buffer_);

        std::cout << "[CloudServicesDialog] OpenRouter settings saved. API key: "
                  << (api_key_buffer_[0] ? "УСТАНОВЛЕН" : "НЕТ") << std::endl;

        close();
    }
    ImGui::SameLine();
    if (ImGui::Button("Применить", ImVec2(100, 0))) {
        // Сохранение настроек без закрытия окна
        auto& openrouter_settings = settings_.openrouter();
        openrouter_settings.custom_base_url = custom_url_buffer_;
        openrouter_settings.free_models_only = free_models_only_;
        openrouter_settings.last_search_query = search_buffer_;
        openrouter_client_->set_api_key(api_key_buffer_);
        
        // Сохраняем ключ в .env файл
        settings_.update_openrouter_api_key(api_key_buffer_);

        std::cout << "[CloudServicesDialog] OpenRouter settings applied. API key: "
                  << (api_key_buffer_[0] ? "УСТАНОВЛЕН" : "НЕТ") << std::endl;
    }
    ImGui::SameLine();
    if (ImGui::Button("Отмена", ImVec2(100, 0))) {
        close();
    }
}

void CloudServicesDialog::render_model_list() {
    if (filtered_models_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
            is_loading_ ? "Загрузка списка моделей..." : "Модели не найдены. Нажмите 'Обновить'");
        return;
    }

    float max_height = ImGui::GetFontSize() * 12;
    ImGui::BeginChild("ModelsList", ImVec2(0, max_height), true);

    if (ImGui::BeginTable("ModelsTable", 5,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Sortable)) {
        
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Provider", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Price", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Free", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < filtered_models_.size(); i++) {
            const auto& model = filtered_models_[i];
            ImGui::TableNextRow();

            // Выделение выбранной строки
            if (static_cast<int>(i) == selected_model_index_) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_Header));
            }

            // Название модели
            if (ImGui::TableNextColumn()) {
                if (ImGui::Selectable(model.name.c_str(), selected_model_index_ == static_cast<int>(i),
                    ImGuiSelectableFlags_SpanAllColumns)) {
                    selected_model_index_ = static_cast<int>(i);
                }
            }

            // Provider
            if (ImGui::TableNextColumn()) {
                ImGui::Text("%s", model.provider.c_str());
            }

            // Context size
            if (ImGui::TableNextColumn()) {
                ImGui::Text("%s", format_context_size(model.context_length).c_str());
            }

            // Price
            if (ImGui::TableNextColumn()) {
                std::string price_str = format_price(model.prompt_price_usd_per_million);
                if (model.completion_price_usd_per_million > 0) {
                    price_str += " / " + format_price(model.completion_price_usd_per_million);
                }
                ImGui::Text("%s", price_str.c_str());
            }

            // Free
            if (ImGui::TableNextColumn()) {
                if (model.is_free) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Yes");
                } else {
                    ImGui::TextDisabled("No");
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    // Кнопка выбора
    ImGui::Spacing();
    if (selected_model_index_ >= 0 && selected_model_index_ < static_cast<int>(filtered_models_.size())) {
        if (ImGui::Button("Выбрать модель")) {
            on_model_selected(filtered_models_[selected_model_index_]);
        }
    }
}

void CloudServicesDialog::load_models() {
    is_loading_ = true;
    models_.clear();
    filtered_models_.clear();

    openrouter_client_->get_models_async(
        [this](const OpenRouterModelsResponse& response) {
            is_loading_ = false;
            if (response.error.empty()) {
                models_ = response.models;
                apply_filters();
            } else {
                std::cerr << "[CloudServicesDialog] Failed to load models: " << response.error << std::endl;
            }
        }
    );
}

void CloudServicesDialog::load_free_models() {
    is_loading_ = true;
    models_.clear();
    filtered_models_.clear();

    openrouter_client_->get_free_models_async(
        [this](const OpenRouterModelsResponse& response) {
            is_loading_ = false;
            if (response.error.empty()) {
                models_ = response.models;
                apply_filters();
            } else {
                std::cerr << "[CloudServicesDialog] Failed to load free models: " << response.error << std::endl;
            }
        }
    );
}

void CloudServicesDialog::search_models(const std::string& query, bool free_only) {
    is_loading_ = true;
    models_.clear();
    filtered_models_.clear();

    openrouter_client_->search_models_async(
        query,
        free_only,
        [this](const OpenRouterModelsResponse& response) {
            is_loading_ = false;
            if (response.error.empty()) {
                models_ = response.models;
                apply_filters();
            } else {
                std::cerr << "[CloudServicesDialog] Failed to search models: " << response.error << std::endl;
            }
        }
    );
}

void CloudServicesDialog::apply_filters() {
    filtered_models_.clear();

    std::string search_query = search_buffer_;
    std::transform(search_query.begin(), search_query.end(), search_query.begin(), ::tolower);

    for (const auto& model : models_) {
        // Фильтр по бесплатным
        if (free_models_only_ && !model.is_free) {
            continue;
        }

        // Поиск по названию
        if (!search_query.empty()) {
            std::string model_name_lower = model.name;
            std::transform(model_name_lower.begin(), model_name_lower.end(), 
                          model_name_lower.begin(), ::tolower);
            
            std::string model_id_lower = model.id;
            std::transform(model_id_lower.begin(), model_id_lower.end(), 
                          model_id_lower.begin(), ::tolower);

            if (model_name_lower.find(search_query) == std::string::npos &&
                model_id_lower.find(search_query) == std::string::npos) {
                continue;
            }
        }

        filtered_models_.push_back(model);
    }

    // Сортировка: бесплатные первыми, затем по названию
    std::sort(filtered_models_.begin(), filtered_models_.end(),
        [](const OpenRouterModel& a, const OpenRouterModel& b) {
            if (a.is_free != b.is_free) {
                return a.is_free;
            }
            return a.name < b.name;
        });

    selected_model_index_ = -1;
}

void CloudServicesDialog::on_model_selected(const OpenRouterModel& model) {
    auto& openrouter_settings = settings_.openrouter();
    openrouter_settings.selected_model = model.id;
    openrouter_settings.enabled = true;

    // Добавление в недавние
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

    std::cout << "[CloudServicesDialog] Selected OpenRouter model: " << model.name << std::endl;
}

std::string CloudServicesDialog::format_price(double price) const {
    if (price == 0.0) {
        return "Free";
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(6) << price << " / 1K tokens";
    return oss.str();
}

std::string CloudServicesDialog::format_context_size(int64_t context) const {
    if (context >= 1000000) {
        return std::to_string(context / 1000000) + "M";
    } else if (context >= 1000) {
        return std::to_string(context / 1000) + "K";
    }
    return std::to_string(context);
}

void CloudServicesDialog::on_models_received(const OpenRouterModelsResponse& response) {
    is_loading_ = false;
    if (response.error.empty()) {
        models_ = response.models;
        apply_filters();
    } else {
        std::cerr << "[CloudServicesDialog] Failed to load models: " << response.error << std::endl;
    }
}

void CloudServicesDialog::refresh_usage_stats() {
    auto& openrouter_settings = settings_.openrouter();

    std::string api_key = settings_.get_openrouter_api_key();
    if (!api_key.empty()) {
        llama_gui::core::OpenRouterClient limit_client(api_key);
        auto limit = limit_client.get_rate_limit();

        openrouter_settings.usage_total_requests = limit.total_requests;
        openrouter_settings.usage_remaining = limit.remaining_requests;
        openrouter_settings.usage_limit = limit.limit;
    }
}

// ============================================================================
// Вкладка KiloCode
// ============================================================================

void CloudServicesDialog::render_kilocode_tab() {
    auto& kilocode_settings = settings_.kilocode();

    ImGui::Text("Настройки KiloCode API");
    ImGui::Separator();

    // Включение KiloCode — автоматически отключает другие провайдеры
    if (ImGui::Checkbox("Использовать KiloCode", &kilocode_settings.enabled)) {
        if (kilocode_settings.enabled) {
            settings_.openrouter().enabled = false;
            if (kilocode_settings.selected_model.empty()) {
                load_kilocode_models();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("При включении запросы будут отправляться в KiloCode API через Tor (SOCKS5 прокси)");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // API ключ
    ImGui::Text("API Key:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##kilocode_api_key", kilocode_api_key_buffer_, sizeof(kilocode_api_key_buffer_), ImGuiInputTextFlags_Password)) {
        kilocode_client_->set_api_key(kilocode_api_key_buffer_);
        // Сохраняем ключ в .env файл
        settings_.update_kilocode_api_key(kilocode_api_key_buffer_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste##kilocode_api_key")) {
        if (ImGui::GetClipboardText()) {
            std::strncpy(kilocode_api_key_buffer_, ImGui::GetClipboardText(), sizeof(kilocode_api_key_buffer_) - 1);
            kilocode_api_key_buffer_[sizeof(kilocode_api_key_buffer_) - 1] = '\0';
            kilocode_client_->set_api_key(kilocode_api_key_buffer_);
            // Сохраняем ключ в .env файл
            settings_.update_kilocode_api_key(kilocode_api_key_buffer_);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("API ключ KiloCode (получите на app.kilo.ai). Ключ хранится в файле .env");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // Использование Tor
    if (ImGui::Checkbox("Использовать Tor (SOCKS5)", &kilocode_use_tor_)) {
        kilocode_settings.use_tor = kilocode_use_tor_;
        kilocode_client_->use_proxy(kilocode_use_tor_);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("Все запросы к KiloCode API будут идти через Tor прокси для обхода блокировок");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // URL прокси
    ImGui::Text("SOCKS5 Proxy URL:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(300);
    if (ImGui::InputText("##kilocode_tor_proxy", kilocode_tor_proxy_buffer_, sizeof(kilocode_tor_proxy_buffer_))) {
        kilocode_settings.tor_proxy_url = kilocode_tor_proxy_buffer_;
        kilocode_client_->set_proxy_url(kilocode_tor_proxy_buffer_);
    }

    ImGui::Spacing();

    // Таймаут
    ImGui::Text("Request Timeout (ms):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputInt("##kilocode_timeout", &kilocode_settings.timeout_ms)) {
        kilocode_client_->set_timeout(kilocode_settings.timeout_ms);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Выбор модели
    ImGui::Text("Модель:");
    ImGui::SameLine();

    if (ImGui::Button("Выбрать модель...##kilocode")) {
        load_kilocode_free_models();
    }
    ImGui::SameLine();

    if (!kilocode_settings.selected_model.empty()) {
        ImGui::Text("Выбрано: %s", kilocode_settings.selected_model.c_str());
    } else {
        ImGui::TextDisabled("Модель не выбрана");
    }

    ImGui::Spacing();

    // Недавние модели
    auto& recent_models = kilocode_settings.recent_models;
    if (!recent_models.empty()) {
        ImGui::Text("Недавние модели:");
        ImGui::SameLine();

        std::vector<const char*> model_items;
        std::vector<std::string> model_storage;

        for (const auto& m : recent_models) {
            if (!m.empty()) {
                model_storage.push_back(m);
                model_items.push_back(model_storage.back().c_str());
            }
        }

        if (!model_items.empty()) {
            ImGui::SetNextItemWidth(300);
            if (ImGui::BeginCombo("##kilocode_recent_models", "")) {
                for (size_t i = 0; i < model_items.size(); i++) {
                    if (ImGui::Selectable(model_items[i], false)) {
                        kilocode_settings.selected_model = model_storage[i];
                        kilocode_settings.enabled = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Список моделей
    if (ImGui::CollapsingHeader("Список бесплатных моделей", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        // Поиск
        ImGui::Text("Поиск:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputText("##kilocode_search", kilocode_search_buffer_, sizeof(kilocode_search_buffer_), ImGuiInputTextFlags_EnterReturnsTrue)) {
            kilocode_settings.last_search_query = kilocode_search_buffer_;
            search_kilocode_models(kilocode_settings.last_search_query, true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Найти##kilocode_search")) {
            kilocode_settings.last_search_query = kilocode_search_buffer_;
            search_kilocode_models(kilocode_settings.last_search_query, true);
        }
        ImGui::SameLine();
        if (ImGui::Button("Сброс##kilocode_reset")) {
            kilocode_settings.last_search_query = "";
            kilocode_search_buffer_[0] = '\0';
            load_kilocode_free_models();
        }

        ImGui::SameLine();
        if (is_loading_) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Загрузка...");
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 150);
        ImGui::Text("%zu моделей", kilocode_filtered_models_.size());

        ImGui::Spacing();
        render_kilocode_model_list();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Кнопки управления
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 350);
    if (ImGui::Button("OK##kilocode", ImVec2(100, 0))) {
        kilocode_settings.tor_proxy_url = kilocode_tor_proxy_buffer_;
        kilocode_settings.use_tor = kilocode_use_tor_;
        kilocode_client_->set_api_key(kilocode_api_key_buffer_);
        kilocode_client_->use_proxy(kilocode_use_tor_);
        kilocode_client_->set_proxy_url(kilocode_tor_proxy_buffer_);
        
        // Сохраняем ключ в .env файл
        settings_.update_kilocode_api_key(kilocode_api_key_buffer_);
        
        close();
    }
    ImGui::SameLine();
    if (ImGui::Button("Применить##kilocode", ImVec2(100, 0))) {
        kilocode_settings.tor_proxy_url = kilocode_tor_proxy_buffer_;
        kilocode_settings.use_tor = kilocode_use_tor_;
        kilocode_client_->set_api_key(kilocode_api_key_buffer_);
        kilocode_client_->use_proxy(kilocode_use_tor_);
        kilocode_client_->set_proxy_url(kilocode_tor_proxy_buffer_);
        
        // Сохраняем ключ в .env файл
        settings_.update_kilocode_api_key(kilocode_api_key_buffer_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Отмена##kilocode", ImVec2(100, 0))) {
        close();
    }
}

void CloudServicesDialog::load_kilocode_models() {
    is_loading_ = true;
    kilocode_models_.clear();
    kilocode_filtered_models_.clear();

    kilocode_client_->get_models_async(
        [this](const KiloCodeModelsResponse& response) {
            is_loading_ = false;
            if (response.error.empty()) {
                kilocode_models_ = response.models;
                apply_kilocode_filters();
            } else {
                std::cerr << "[CloudServicesDialog] Failed to load KiloCode models: " << response.error << std::endl;
            }
        }
    );
}

void CloudServicesDialog::load_kilocode_free_models() {
    is_loading_ = true;
    kilocode_models_.clear();
    kilocode_filtered_models_.clear();

    kilocode_client_->get_free_models_async(
        [this](const KiloCodeModelsResponse& response) {
            is_loading_ = false;
            if (response.error.empty()) {
                kilocode_models_ = response.models;
                apply_kilocode_filters();
            } else {
                std::cerr << "[CloudServicesDialog] Failed to load KiloCode free models: " << response.error << std::endl;
            }
        }
    );
}

void CloudServicesDialog::search_kilocode_models(const std::string& query, bool free_only) {
    is_loading_ = true;
    kilocode_models_.clear();
    kilocode_filtered_models_.clear();

    kilocode_client_->search_models_async(
        query,
        free_only,
        [this](const KiloCodeModelsResponse& response) {
            is_loading_ = false;
            if (response.error.empty()) {
                kilocode_models_ = response.models;
                apply_kilocode_filters();
            } else {
                std::cerr << "[CloudServicesDialog] Failed to search KiloCode models: " << response.error << std::endl;
            }
        }
    );
}

void CloudServicesDialog::apply_kilocode_filters() {
    kilocode_filtered_models_.clear();

    std::string search_query = kilocode_search_buffer_;
    std::transform(search_query.begin(), search_query.end(), search_query.begin(), ::tolower);

    for (const auto& model : kilocode_models_) {
        if (!search_query.empty()) {
            std::string name_lower = model.name;
            std::transform(name_lower.begin(), name_lower.end(), name_lower.begin(), ::tolower);
            std::string id_lower = model.id;
            std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);

            if (name_lower.find(search_query) == std::string::npos &&
                id_lower.find(search_query) == std::string::npos) {
                continue;
            }
        }

        kilocode_filtered_models_.push_back(model);
    }

    std::sort(kilocode_filtered_models_.begin(), kilocode_filtered_models_.end(),
        [](const KiloCodeModel& a, const KiloCodeModel& b) {
            if (a.is_free != b.is_free) return a.is_free;
            return a.name < b.name;
        });

    kilocode_selected_model_index_ = -1;
}

void CloudServicesDialog::render_kilocode_model_list() {
    if (kilocode_filtered_models_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
            is_loading_ ? "Загрузка списка моделей..." : "Модели не найдены. Нажмите 'Выбрать модель'");
        return;
    }

    float max_height = ImGui::GetFontSize() * 12;
    ImGui::BeginChild("KiloCodeModelsList", ImVec2(0, max_height), true);

    if (ImGui::BeginTable("KiloCodeModelsTable", 4,
        ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders |
        ImGuiTableFlags_ScrollY)) {

        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Context", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Max Output", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Free", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < kilocode_filtered_models_.size(); i++) {
            const auto& model = kilocode_filtered_models_[i];
            ImGui::TableNextRow();

            if (static_cast<int>(i) == kilocode_selected_model_index_) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, ImGui::GetColorU32(ImGuiCol_Header));
            }

            if (ImGui::TableNextColumn()) {
                if (ImGui::Selectable(model.name.c_str(), kilocode_selected_model_index_ == static_cast<int>(i),
                    ImGuiSelectableFlags_SpanAllColumns)) {
                    kilocode_selected_model_index_ = static_cast<int>(i);
                }
            }

            if (ImGui::TableNextColumn()) {
                ImGui::Text("%s", format_context_size(model.context_length).c_str());
            }

            if (ImGui::TableNextColumn()) {
                ImGui::Text("%s", format_context_size(model.max_output_tokens).c_str());
            }

            if (ImGui::TableNextColumn()) {
                if (model.is_free) {
                    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Yes");
                } else {
                    ImGui::TextDisabled("No");
                }
            }
        }

        ImGui::EndTable();
    }

    ImGui::EndChild();

    ImGui::Spacing();
    if (kilocode_selected_model_index_ >= 0 && kilocode_selected_model_index_ < static_cast<int>(kilocode_filtered_models_.size())) {
        if (ImGui::Button("Выбрать модель##kilocode")) {
            on_kilocode_model_selected(kilocode_filtered_models_[kilocode_selected_model_index_]);
        }
    }
}

void CloudServicesDialog::on_kilocode_model_selected(const KiloCodeModel& model) {
    auto& kilocode_settings = settings_.kilocode();
    kilocode_settings.selected_model = model.id;
    kilocode_settings.enabled = true;

    auto it = std::find(kilocode_settings.recent_models.begin(),
                        kilocode_settings.recent_models.end(),
                        model.id);
    if (it != kilocode_settings.recent_models.end()) {
        kilocode_settings.recent_models.erase(it);
    }
    kilocode_settings.recent_models.insert(kilocode_settings.recent_models.begin(), model.id);
    if (kilocode_settings.recent_models.size() > 10) {
        kilocode_settings.recent_models.pop_back();
    }

    std::cout << "[CloudServicesDialog] Selected KiloCode model: " << model.name << std::endl;
}

// ============================================================================
// Вкладка Universal
// ============================================================================

void CloudServicesDialog::render_universal_tab() {
    auto& universal_settings = settings_.universal_openai();

    ImGui::Text("Настройки универсального OpenAI API");
    ImGui::Separator();

    // Включение Universal API — автоматически отключает другие провайдеры
    if (ImGui::Checkbox("Использовать универсальный API", &universal_settings.enabled)) {
        if (universal_settings.enabled) {
            settings_.openrouter().enabled = false;
            settings_.kilocode().enabled = false;
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("При включении запросы будут отправляться в произвольный OpenAI-совместимый API вместо локального сервера");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // API ключ
    ImGui::Text("API Key:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##universal_api_key", universal_api_key_buffer_, sizeof(universal_api_key_buffer_), ImGuiInputTextFlags_Password)) {
        universal_client_->set_api_key(universal_api_key_buffer_);
        settings_.update_universal_openai_api_key(universal_api_key_buffer_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste##universal_api_key")) {
        if (ImGui::GetClipboardText()) {
            std::strncpy(universal_api_key_buffer_, ImGui::GetClipboardText(), sizeof(universal_api_key_buffer_) - 1);
            universal_api_key_buffer_[sizeof(universal_api_key_buffer_) - 1] = '\0';
            universal_client_->set_api_key(universal_api_key_buffer_);
            settings_.update_universal_openai_api_key(universal_api_key_buffer_);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("API ключ (опционально). Ключ хранится в файле .env");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // Кастомный базовый URL
    ImGui::Text("Custom Base URL (optional):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##universal_base_url", universal_base_url_buffer_, sizeof(universal_base_url_buffer_))) {
        universal_settings.custom_base_url = universal_base_url_buffer_;
        universal_client_->set_base_url(universal_base_url_buffer_);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("Кастомный базовый URL (например, 'https://api.openai.com/v1')");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // Кастомный endpoint
    ImGui::Text("Custom Endpoint (optional):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##universal_endpoint", universal_endpoint_buffer_, sizeof(universal_endpoint_buffer_))) {
        universal_settings.custom_endpoint = universal_endpoint_buffer_;
        universal_client_->set_endpoint(universal_endpoint_buffer_);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("Кастомный endpoint (например, 'chat/completions')");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // Таймаут
    ImGui::Text("Request Timeout (ms):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    if (ImGui::InputInt("##universal_timeout", &universal_settings.timeout_ms)) {
        universal_client_->set_timeout(universal_settings.timeout_ms);
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Выбор модели
    ImGui::Text("Модель:");
    ImGui::SameLine();

    // Кнопка для открытия списка моделей
    if (ImGui::Button("Выбрать модель...")) {
        load_universal_models();
    }
    ImGui::SameLine();

    // Отображение текущей выбранной модели
    if (!universal_settings.selected_model.empty()) {
        ImGui::Text("Выбрано: %s", universal_settings.selected_model.c_str());
    } else {
        ImGui::TextDisabled("Модель не выбрана");
    }

    ImGui::Spacing();

    // Недавние модели (combo)
    auto& recent_models = universal_settings.recent_models;
    if (!recent_models.empty()) {
        ImGui::Text("Недавние модели:");
        ImGui::SameLine();

        std::vector<const char*> model_items;
        std::vector<std::string> model_storage;

        for (const auto& m : recent_models) {
            if (!m.empty()) {
                model_storage.push_back(m);
                model_items.push_back(model_storage.back().c_str());
            }
        }

        if (!model_items.empty()) {
            ImGui::SetNextItemWidth(300);
            if (ImGui::BeginCombo("##universal_recent_models", "")) {
                for (size_t i = 0; i < model_items.size(); i++) {
                    if (ImGui::Selectable(model_items[i], false)) {
                        universal_settings.selected_model = model_storage[i];
                        universal_settings.enabled = true;
                    }
                }
                ImGui::EndCombo();
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Фильтры и список моделей (inline)
    if (ImGui::CollapsingHeader("Список моделей", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        // Поиск
        ImGui::Text("Поиск:");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(300);
        if (ImGui::InputText("##UniversalSearch", universal_search_buffer_, sizeof(universal_search_buffer_), ImGuiInputTextFlags_EnterReturnsTrue)) {
            search_universal_models(universal_search_buffer_, universal_free_models_only_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Найти##search")) {
            search_universal_models(universal_search_buffer_, universal_free_models_only_);
        }
        ImGui::SameLine();
        if (ImGui::Button("Сброс##reset")) {
            universal_search_buffer_[0] = '\0';
            search_universal_models("", universal_free_models_only_);
        }

        ImGui::SameLine();
        ImGui::Spacing();
        ImGui::SameLine();

        // Фильтр бесплатных моделей
        if (ImGui::Checkbox("Только бесплатные модели", &universal_free_models_only_)) {
            search_universal_models(universal_search_buffer_, universal_free_models_only_);
        }

        ImGui::SameLine();
        if (ImGui::Button("Обновить список")) {
            load_universal_models();
        }

        ImGui::SameLine();
        if (is_loading_) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Загрузка...");
        }

        ImGui::SameLine(ImGui::GetWindowWidth() - 150);
        ImGui::Text("%zu моделей", universal_filtered_models_.size());

        ImGui::Spacing();

        // Список моделей inline
        render_universal_model_list();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Информация о провайдере
    if (ImGui::CollapsingHeader("Информация о провайдере")) {
        ImGui::Spacing();

        ImGui::Text("Universal API — произвольный OpenAI-совместимый endpoint");
        ImGui::TextDisabled("Используйте для:");
        ImGui::BulletText("OpenAI (https://api.openai.com/v1)");
        ImGui::BulletText("OpenRouter (https://openrouter.ai/api/v1)");
        ImGui::BulletText("KiloCode (https://api.kilo.ai/api/gateway)");
        ImGui::BulletText("Локальные инстансы с совместимой API");
        ImGui::BulletText("Другие OpenAI-совместимые сервисы");
    }
}

void CloudServicesDialog::render_universal_model_list() {
    ImGui::Spacing();

    if (is_loading_) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "Загрузка списка моделей...");
        return;
    }

    if (!universal_models_.empty()) {
        // Таблица моделей
        if (ImGui::BeginTable("UniversalModelTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
            ImGui::TableSetupColumn("Model ID", ImGuiTableColumnFlags_None, 0);
            ImGui::TableSetupColumn("Owned By", ImGuiTableColumnFlags_None, 0);
            ImGui::TableSetupColumn("Context Length", ImGuiTableColumnFlags_None, 0);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_None, 0);

            ImGui::TableHeadersRow();

            for (size_t i = 0; i < universal_models_.size(); i++) {
                const auto& model = universal_models_[i];

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%s", model.id.c_str());

                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", model.owned_by.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(model.context_length));

                ImGui::TableNextColumn();
                char button_label[64];
                snprintf(button_label, sizeof(button_label), "Выбрать##%d", i);
                if (ImGui::Button(button_label)) {
                    on_universal_model_selected(model);
                }
            }

            ImGui::EndTable();
        }
    } else if (!loading_error_.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Ошибка загрузки: %s", loading_error_.c_str());
    } else {
        ImGui::TextDisabled("Список моделей пуст");
    }
}

void CloudServicesDialog::load_universal_models() {
    is_loading_ = true;
    loading_error_.clear();
    universal_models_.clear();
    universal_filtered_models_.clear();

    update_universal_settings();

    universal_client_->get_models_async([this](const llama_gui::core::UniversalOpenAIModelsResponse& response) {
        is_loading_ = false;

        if (response.success) {
            universal_models_ = response.models;
            apply_universal_filters();
        } else {
            loading_error_ = response.error;
        }
    });
}

void CloudServicesDialog::load_universal_free_models() {
    // Universal API не имеет специального endpoint для бесплатных моделей
    // Используем обычный get_models
    load_universal_models();
}

void CloudServicesDialog::search_universal_models(const std::string& query, bool free_only) {
    is_loading_ = true;
    universal_models_.clear();
    universal_filtered_models_.clear();

    update_universal_settings();

    universal_client_->get_models_async([this, query](const llama_gui::core::UniversalOpenAIModelsResponse& response) {
        if (response.success) {
            std::vector<llama_gui::core::UniversalOpenAIModel> filtered;

            std::string query_lower = query;
            std::transform(query_lower.begin(), query_lower.end(), query_lower.begin(), ::tolower);

            for (const auto& model : response.models) {
                if (query.empty()) {
                    filtered.push_back(model);
                    continue;
                }

                std::string id_lower = model.id;
                std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);
                std::string owned_by_lower = model.owned_by;
                std::transform(owned_by_lower.begin(), owned_by_lower.end(), owned_by_lower.begin(), ::tolower);

                if (id_lower.find(query_lower) != std::string::npos ||
                    owned_by_lower.find(query_lower) != std::string::npos) {
                    filtered.push_back(model);
                }
            }

            universal_models_ = filtered;
            apply_universal_filters();
        }
    });
}

void CloudServicesDialog::apply_universal_filters() {
    universal_filtered_models_.clear();

    std::string search_query = universal_search_buffer_;
    std::transform(search_query.begin(), search_query.end(), search_query.begin(), ::tolower);

    for (const auto& model : universal_models_) {
        // Фильтр по бесплатным (Universal API не предоставляет информацию о цене)
        if (universal_free_models_only_) {
            continue;
        }

        // Поиск по названию
        if (!search_query.empty()) {
            std::string id_lower = model.id;
            std::transform(id_lower.begin(), id_lower.end(), id_lower.begin(), ::tolower);

            std::string owned_by_lower = model.owned_by;
            std::transform(owned_by_lower.begin(), owned_by_lower.end(), owned_by_lower.begin(), ::tolower);

            if (id_lower.find(search_query) == std::string::npos &&
                owned_by_lower.find(search_query) == std::string::npos) {
                continue;
            }
        }

        universal_filtered_models_.push_back(model);
    }

    // Сортировка по ID
    std::sort(universal_filtered_models_.begin(), universal_filtered_models_.end(),
        [](const llama_gui::core::UniversalOpenAIModel& a, const llama_gui::core::UniversalOpenAIModel& b) {
            return a.id < b.id;
        });

    universal_selected_model_index_ = -1;
}

void CloudServicesDialog::on_universal_model_selected(const llama_gui::core::UniversalOpenAIModel& model) {
    auto& universal_settings = settings_.universal_openai();
    universal_settings.selected_model = model.id;
    universal_settings.enabled = true;

    // Добавление в недавние
    auto it = std::find(universal_settings.recent_models.begin(),
                        universal_settings.recent_models.end(),
                        model.id);
    if (it != universal_settings.recent_models.end()) {
        universal_settings.recent_models.erase(it);
    }
    universal_settings.recent_models.insert(universal_settings.recent_models.begin(), model.id);
    if (universal_settings.recent_models.size() > 10) {
        universal_settings.recent_models.pop_back();
    }

    std::cout << "[CloudServicesDialog] Selected Universal model: " << model.id << std::endl;
}

void CloudServicesDialog::update_universal_settings() {
    // Обновление API ключа из .env файла
    std::string api_key = settings_.get_universal_openai_api_key();
    universal_client_->set_api_key(api_key);

    // Обновление таймаута
    universal_client_->set_timeout(settings_.universal_openai().timeout_ms);

    // Обновление кастомного URL если указан
    if (!settings_.universal_openai().custom_base_url.empty()) {
        universal_client_->set_base_url(settings_.universal_openai().custom_base_url);
    }

    // Обновление кастомного endpoint
    if (!settings_.universal_openai().custom_endpoint.empty()) {
        universal_client_->set_endpoint(settings_.universal_openai().custom_endpoint);
    }

    // Обновление буфера API ключа
    std::strncpy(universal_api_key_buffer_, api_key.c_str(), sizeof(universal_api_key_buffer_) - 1);
    universal_api_key_buffer_[sizeof(universal_api_key_buffer_) - 1] = '\0';
}

} // namespace ui
} // namespace llama_gui
