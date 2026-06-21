#include "../include/ui/universal_openai_dialog.h"
#include "../include/core/logger.h"
#include "../include/core/env_file_handler.h"
#include "../include/ui/localization_manager.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cstring>

using namespace llama_gui::core;
using namespace llama_gui::ui;

namespace llama_gui {
namespace ui {

UniversalOpenAIDialog::UniversalOpenAIDialog(Settings& settings)
    : settings_(settings)
    , client_(std::make_unique<UniversalOpenAIClient>(settings.get_universal_openai_api_key())) {

    // Инициализация API ключа из .env
    env_api_key_ = settings.get_universal_openai_api_key();
    std::strncpy(api_key_buffer_, env_api_key_.c_str(), sizeof(api_key_buffer_) - 1);
    api_key_buffer_[sizeof(api_key_buffer_) - 1] = '\0';

    // Инициализация базового URL
    std::strncpy(base_url_buffer_, settings.universal_openai().custom_base_url.c_str(), sizeof(base_url_buffer_) - 1);
    base_url_buffer_[sizeof(base_url_buffer_) - 1] = '\0';

    // Инициализация endpoint
    std::strncpy(endpoint_buffer_, settings.universal_openai().custom_endpoint.c_str(), sizeof(endpoint_buffer_) - 1);
    endpoint_buffer_[sizeof(endpoint_buffer_) - 1] = '\0';
}

void UniversalOpenAIDialog::open() {
    is_open_ = true;
    show_model_list_ = false;
    show_usage_stats_ = false;
    std::cout << "[UniversalOpenAIDialog] open() called, is_open_ = " << is_open_ << std::endl;
}

void UniversalOpenAIDialog::close() {
    is_open_ = false;
    // Сохранение настроек в settings НЕ происходит автоматически
    // Пользователь должен явно нажать "OK" или "Применить"
}

void UniversalOpenAIDialog::update_client() {
    // Обновление API ключа из .env файла
    std::string api_key = settings_.get_universal_openai_api_key();
    client_->set_api_key(api_key);

    // Обновление таймаута
    client_->set_timeout(settings_.universal_openai().timeout_ms);

    // Обновление кастомного URL если указан
    if (!settings_.universal_openai().custom_base_url.empty()) {
        client_->set_base_url(settings_.universal_openai().custom_base_url);
    }

    // Обновление кастомного endpoint
    if (!settings_.universal_openai().custom_endpoint.empty()) {
        client_->set_endpoint(settings_.universal_openai().custom_endpoint);
    }

    // Обновление буфера API ключа
    std::strncpy(api_key_buffer_, api_key.c_str(), sizeof(api_key_buffer_) - 1);
    api_key_buffer_[sizeof(api_key_buffer_) - 1] = '\0';

    std::cout << "[UniversalOpenAIDialog] Client updated, API key: "
              << (api_key.empty() ? "НЕТ" : api_key.substr(0, 8) + "...") << std::endl;
}

void UniversalOpenAIDialog::render() {
    if (!is_open_) {
        return;
    }

    // Устанавливаем размер и позицию для независимого окна
    ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(200, 50), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Universal OpenAI / Универсальный OpenAI", &is_open_,
                      ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    // Вкладки
    if (ImGui::BeginTabBar("UniversalOpenAITabBar")) {
        if (ImGui::TabItemButton("Настройки")) {
            dialog_tab_ = DialogTab::General;
        }
        if (ImGui::TabItemButton("Модели")) {
            dialog_tab_ = DialogTab::Models;
        }
        ImGui::EndTabBar();
    }

    ImGui::Separator();
    ImGui::Spacing();

    // Отрисовка выбранной вкладки
    switch (dialog_tab_) {
        case DialogTab::General:
            render_general_tab();
            break;
        case DialogTab::Models:
            render_models_tab();
            break;
    }

    ImGui::End();
}

void UniversalOpenAIDialog::render_general_tab() {
    auto& settings = settings_.universal_openai();

    ImGui::Text("Настройки универсального OpenAI API");
    ImGui::Separator();

    // Включение универсального API — автоматически отключает другие провайдеры
    if (ImGui::Checkbox("Использовать универсальный API", &settings.enabled)) {
        if (settings.enabled) {
            settings_.openrouter().enabled = false;
            settings_.kilocode().enabled = false;
            if (settings.selected_model.empty()) {
                load_models();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("При включении этой опции запросы будут отправляться в произвольный OpenAI-совместимый API вместо локального сервера");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // API ключ
    ImGui::Text("API Key:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##api_key", api_key_buffer_, sizeof(api_key_buffer_), ImGuiInputTextFlags_Password)) {
        client_->set_api_key(api_key_buffer_);
        // Сохраняем ключ в .env файл
        settings_.update_universal_openai_api_key(api_key_buffer_);
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste##api_key")) {
        if (ImGui::GetClipboardText()) {
            std::strncpy(api_key_buffer_, ImGui::GetClipboardText(), sizeof(api_key_buffer_) - 1);
            api_key_buffer_[sizeof(api_key_buffer_) - 1] = '\0';
            client_->set_api_key(api_key_buffer_);
            settings_.update_universal_openai_api_key(api_key_buffer_);
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("API ключ (опционально, требуется для платных моделей). Ключ хранится в файле .env");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // Выбор провайдера (preset)
    ImGui::Text("Провайдер (preset):");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##provider", "")) {
        if (ImGui::Selectable("Custom (Custom)", selected_provider_ == ProviderType::Custom)) {
            selected_provider_ = ProviderType::Custom;
        }
        if (ImGui::Selectable("OpenRouter", selected_provider_ == ProviderType::OpenRouter)) {
            selected_provider_ = ProviderType::OpenRouter;
        }
        if (ImGui::Selectable("KiloCode", selected_provider_ == ProviderType::KiloCode)) {
            selected_provider_ = ProviderType::KiloCode;
        }
        if (ImGui::Selectable("OpenAI", selected_provider_ == ProviderType::OpenAI)) {
            selected_provider_ = ProviderType::OpenAI;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("Быстрый выбор популярного провайдера. Перезапишет Custom настройки.");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // Кастомный базовый URL
    ImGui::Text("Custom Base URL (optional):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##custom_url", base_url_buffer_, sizeof(base_url_buffer_))) {
        settings.custom_base_url = base_url_buffer_;
        client_->set_base_url(base_url_buffer_);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::Text("Кастомный базовый URL (например, 'https://api.openai.com/v1' или 'https://openrouter.ai/api/v1')");
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    ImGui::Spacing();

    // Кастомный endpoint
    ImGui::Text("Custom Endpoint (optional):");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    if (ImGui::InputText("##custom_endpoint", endpoint_buffer_, sizeof(endpoint_buffer_))) {
        settings.custom_endpoint = endpoint_buffer_;
        client_->set_endpoint(endpoint_buffer_);
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
    if (ImGui::InputInt("##timeout", &settings.timeout_ms)) {
        client_->set_timeout(settings.timeout_ms);
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
    if (!settings.selected_model.empty()) {
        ImGui::Text("Выбрано: %s", settings.selected_model.c_str());
    } else {
        ImGui::TextDisabled("Модель не выбрана");
    }

    ImGui::Spacing();

    // Недавние модели (combo)
    auto& recent_models = settings.recent_models;
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
                        settings.selected_model = model_storage[i];
                        settings.enabled = true;
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

        ImGui::Spacing();

        // Фильтр бесплатных моделей
        ImGui::Checkbox("Только бесплатные модели", &free_models_only_);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::Text("Показывать только бесплатные модели (если провайдер поддерживает)");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::Spacing();

        // Список моделей
        render_model_list();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // Информация о провайдере
    if (ImGui::CollapsingHeader("Информация о провайдере")) {
        ImGui::Spacing();

        if (selected_provider_ == ProviderType::Custom) {
            ImGui::Text("Custom API — произвольный OpenAI-совместимый endpoint");
            ImGui::TextDisabled("Используйте для:");
            ImGui::BulletText("OpenRouter");
            ImGui::BulletText("KiloCode");
            ImGui::BulletText("OpenAI");
            ImGui::BulletText("Локальные инстансы");
            ImGui::BulletText("Другие OpenAI-совместимые сервисы");
        } else if (selected_provider_ == ProviderType::OpenRouter) {
            ImGui::Text("OpenRouter — агностичный провайдер AI моделей");
            ImGui::TextDisabled("Используйте для:");
            ImGui::BulletText("Доступ к 100+ моделей");
            ImGui::BulletText("Бесплатные модели");
            ImGui::BulletText("Единый ключ для всех провайдеров");
            ImGui::BulletText("Совместимость с OpenAI API");
        } else if (selected_provider_ == ProviderType::KiloCode) {
            ImGui::Text("KiloCode — AI через Tor (приватность)");
            ImGui::TextDisabled("Используйте для:");
            ImGui::BulletText("Приватность и анонимность");
            ImGui::BulletText("Tor прокси");
            ImGui::BulletText("Бесплатные модели");
        } else if (selected_provider_ == ProviderType::OpenAI) {
            ImGui::Text("OpenAI — оригинальный API");
            ImGui::TextDisabled("Используйте для:");
            ImGui::BulletText("Официальные модели GPT");
            ImGui::BulletText("Максимальная совместимость");
            ImGui::BulletText("Поддержка функций (functions)");
        }
    }
}

void UniversalOpenAIDialog::render_models_tab() {
    ImGui::Text("Модели");
    ImGui::Separator();

    if (ImGui::Button("Обновить список моделей")) {
        load_models();
    }
    ImGui::SameLine();

    if (ImGui::Button("Сброс фильтров")) {
        search_buffer_[0] = '\0';
        free_models_only_ = false;
        load_models();
    }

    ImGui::Spacing();

    // Список моделей
    render_model_list();
}

void UniversalOpenAIDialog::render_model_list() {
    ImGui::Spacing();

    if (loading_models_) {
        ImGui::Text("Загрузка моделей...");
        return;
    }

    if (!models_.empty()) {
        // Таблица моделей
        if (ImGui::BeginTable("ModelTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable)) {
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_None, 0);
            ImGui::TableSetupColumn("Owned By", ImGuiTableColumnFlags_None, 0);
            ImGui::TableSetupColumn("Context Length", ImGuiTableColumnFlags_None, 0);
            ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_None, 0);

            ImGui::TableHeadersRow();

            for (size_t i = 0; i < models_.size(); i++) {
                const auto& model = models_[i];

                ImGui::TableNextRow();

                ImGui::TableNextColumn();
                ImGui::Text("%s", model.id.c_str());

                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", model.owned_by.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%lld", static_cast<long long>(model.context_length));

                ImGui::TableNextColumn();
                std::string button_label = "Выбрать##" + std::to_string(i);
                if (ImGui::Button(button_label.c_str())) {
                    settings_.universal_openai().selected_model = model.id;
                    settings_.universal_openai().enabled = true;
                    
                    // Добавляем в недавние модели
                    auto& recent = settings_.universal_openai().recent_models;
                    recent.erase(std::remove(recent.begin(), recent.end(), model.id), recent.end());
                    recent.insert(recent.begin(), model.id);
                    
                    // Ограничиваем список недавних моделей до 10
                    if (recent.size() > 10) {
                        recent.resize(10);
                    }
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

void UniversalOpenAIDialog::load_models() {
    loading_models_ = true;
    loading_error_.clear();
    
    update_client();

    client_->get_models_async([this](const UniversalOpenAIModelsResponse& response) {
        loading_models_ = false;

        if (response.success) {
            models_ = response.models;
            settings_.universal_openai().last_search_query = search_buffer_;
        } else {
            loading_error_ = response.error;
        }
    });
}

void UniversalOpenAIDialog::search_models(const std::string& query) {
    update_client();

    client_->get_models_async([this, query](const UniversalOpenAIModelsResponse& response) {
        if (response.success) {
            std::vector<UniversalOpenAIModel> filtered;

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

            models_ = filtered;
            settings_.universal_openai().last_search_query = query;
        }
    });
}

} // namespace ui
} // namespace llama_gui
