#include "../include/ui/llama_bench_dialog.h"
#include "../include/bench/bench_common.h"
#include "../include/bench/profile_adapter.h"
#include "../include/core/config_manager.h"
#include "../include/core/server_manager.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <thread>

namespace llama_gui {
namespace ui {

/**
 * @brief Внутренняя реализация LlamaBenchDialog
 */
struct LlamaBenchDialog::Impl {
    bool visible = false;
    bool initialized = false;

    // Модуль llama-bench
    llama_gui::bench::LlamaBenchModule bench_module;

    // Сервер-менеджер (ссылка из MainWindow)
    std::shared_ptr<llama_gui::core::ServerManager> server_manager;
    bool server_was_running = false;  // Был ли сервер запущен до бенчмарка

    // Профили
    std::string profiles_directory;
    std::vector<std::string> available_profiles;
    std::vector<char> profile_selected;
    std::string profiles_filter;
    
    // Параметры теста
    int n_prompt = 512;
    int n_gen = 128;
    int batch_size = 2048;
    int threads = 2;
    int n_gpu_layers = 0;
    int repetitions = 3;
    std::string context_depths = "0,4096,8192";
    
    // Состояние выполнения
    bool running = false;
    int progress = 0;
    std::string current_status;
    
    // Вкладки
    int active_tab = 0;
    
    // Результаты
    bool show_results = false;
    int results_filter_idx = 0;
    
    // Экспорт
    std::string last_export_path;
    
    // UI состояния
    bool show_error_modal = false;
    std::string error_message;
};

// ============================================================================
// Конструктор/деструктор
// ============================================================================

LlamaBenchDialog::LlamaBenchDialog()
    : pimpl_(std::make_unique<Impl>())
{
}

LlamaBenchDialog::~LlamaBenchDialog() {
    shutdown();
}

// ============================================================================
// Управление видимостью
// ============================================================================

void LlamaBenchDialog::setVisible(bool visible) {
    pimpl_->visible = visible;
}

bool LlamaBenchDialog::isVisible() const {
    return pimpl_->visible;
}

void LlamaBenchDialog::toggle() {
    pimpl_->visible = !pimpl_->visible;
}

// ============================================================================
// Управление сервером
// ============================================================================

void LlamaBenchDialog::setServerManager(std::shared_ptr<llama_gui::core::ServerManager> server_manager) {
    pimpl_->server_manager = server_manager;
}

bool LlamaBenchDialog::stopServerForBenchmark() {
    if (!pimpl_->server_manager) {
        return false;
    }

    // Проверить запущен ли сервер
    if (pimpl_->server_manager->is_server_running()) {
        pimpl_->server_was_running = true;

        // Остановить сервер
        std::cout << "Llama Bench: Stopping server for benchmark..." << std::endl;
        pimpl_->server_manager->stop_server(true);  // blocking = true

        // Подождать пока сервер действительно остановится (до 5 секунд)
        int wait_count = 0;
        while (pimpl_->server_manager->is_server_running() && wait_count < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }

        if (pimpl_->server_manager->is_server_running()) {
            std::cerr << "Llama Bench: Warning - server did not stop cleanly" << std::endl;
        } else {
            std::cout << "Llama Bench: Server stopped successfully" << std::endl;
        }
        return true;
    }

    pimpl_->server_was_running = false;
    return false;
}

bool LlamaBenchDialog::restartServerAfterBenchmark() {
    if (!pimpl_->server_manager || !pimpl_->server_was_running) {
        return false;
    }
    
    // Запустить сервер снова
    pimpl_->server_manager->start_server();
    
    std::cout << "Llama Bench: Server restarted after benchmark" << std::endl;
    pimpl_->server_was_running = false;
    return true;
}

// ============================================================================
// Инициализация
// ============================================================================

bool LlamaBenchDialog::initialize(const std::string& llama_bench_path,
                                  const std::string& profiles_dir) {
    if (pimpl_->initialized) {
        return true;
    }
    
    pimpl_->profiles_directory = profiles_dir;
    
    // Найти путь к llama-bench если не указан
    std::string bench_path = llama_bench_path;
    if (bench_path.empty()) {
        bench_path = findLlamaBenchPath();
    }
    
    if (bench_path.empty()) {
        pimpl_->error_message = "llama-bench executable not found";
        pimpl_->show_error_modal = true;
        return false;
    }
    
    // Инициализировать модуль
    if (!pimpl_->bench_module.initialize(bench_path)) {
        pimpl_->error_message = "Failed to initialize llama-bench module";
        pimpl_->show_error_modal = true;
        return false;
    }
    
    // Настроить колбэки
    pimpl_->bench_module.setStatusCallback([this](const std::string& status) {
        pimpl_->current_status = status;
    });

    pimpl_->bench_module.setProgressCallback([this](int percent, const std::string& status) {
        pimpl_->progress = percent;
        pimpl_->current_status = status;
    });
    
    // Callback для перезапуска сервера после завершения бенчмарка
    pimpl_->bench_module.setBenchmarkCompleteCallback([this]() {
        restartServerAfterBenchmark();
    });

    // Загрузить профили
    updateProfileList();
    
    pimpl_->initialized = true;
    
    return true;
}

void LlamaBenchDialog::shutdown() {
    pimpl_->bench_module.shutdown();
    pimpl_->initialized = false;
}

// ============================================================================
// Сброс состояния
// ============================================================================

void LlamaBenchDialog::reset() {
    pimpl_->running = false;
    pimpl_->progress = 0;
    pimpl_->current_status.clear();
    pimpl_->active_tab = 0;
}

// ============================================================================
// Рендеринг
// ============================================================================

void LlamaBenchDialog::render() {
    if (!pimpl_->visible) {
        return;
    }
    
    ImGui::SetNextWindowSize(ImVec2(900, 600), ImGuiCond_FirstUseEver);
    
    if (!ImGui::Begin("Llama Bench - Model Comparison", &pimpl_->visible)) {
        ImGui::End();
        return;
    }
    
    // Вкладки
    const char* tabs[] = { "Run Benchmark", "Compare Profiles", "Results", "History" };
    
    ImGui::BeginTabBar("##LlamaBenchTabs");
    
    if (ImGui::BeginTabItem(tabs[0])) {
        pimpl_->active_tab = 0;
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tabs[1])) {
        pimpl_->active_tab = 1;
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tabs[2])) {
        pimpl_->active_tab = 2;
        ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem(tabs[3])) {
        pimpl_->active_tab = 3;
        ImGui::EndTabItem();
    }
    
    ImGui::EndTabBar();
    
    // Рендеринг содержимого вкладок
    switch (pimpl_->active_tab) {
        case 0:
            renderRunBenchmarkTab();
            break;
        case 1:
            renderCompareProfilesTab();
            break;
        case 2:
            renderResultsTab();
            break;
        case 3:
            renderHistoryTab();
            break;
    }
    
    // Статус бар
    renderStatusBar();
    
    ImGui::End();
    
    // Модальное окно ошибки
    if (pimpl_->show_error_modal) {
        ImGui::OpenPopup("Error");
        pimpl_->show_error_modal = false;
    }
    
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Error: %s", pimpl_->error_message.c_str());
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

// ============================================================================
// Рендеринг вкладок
// ============================================================================

void LlamaBenchDialog::renderRunBenchmarkTab() {
    ImGui::Text("Run benchmark for a single model/profile");
    ImGui::Separator();
    ImGui::Spacing();
    
    // Выбор профиля
    renderProfileSelection();
    ImGui::Spacing();
    
    // Параметры теста
    renderTestParameters();
    ImGui::Spacing();
    
    // Индикатор прогресса если выполняется
    if (pimpl_->running) {
        renderProgressIndicator();
        ImGui::Spacing();
    }
    
    // Кнопки действий
    renderActionButtons();
}

void LlamaBenchDialog::renderCompareProfilesTab() {
    ImGui::Text("Compare multiple profiles against each other");
    ImGui::Separator();
    ImGui::Spacing();

    // Важное сообщение о том, что параметры берутся из профилей
    ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f),
                       "ⓘ Параметры теста берутся ИЗ КАЖДОГО профиля");
    ImGui::SameLine();
    if (ImGui::Button("?")) {
        ImGui::OpenPopup("ProfileParamsInfo");
    }
    
    // Модальное окно с информацией
    if (ImGui::BeginPopupModal("ProfileParamsInfo", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("При сравнении профилей каждый профиль запускается\n"
                    "со СВОИМИ собственными параметрами из JSON файла.\n\n"
                    "Это позволяет сравнивать РАЗНЫЕ конфигурации\n"
                    "и определять, какая из них лучше для вашей системы.\n\n"
                    "Параметры которые берутся из профиля:\n"
                    "  - batch_size (размер батча)\n"
                    "  - ubatch_size (микро-батч)\n"
                    "  - threads (потоки CPU)\n"
                    "  - n_gpu_layers (слои GPU)\n"
                    "  - cache_type_k/v (тип KV кэша)\n"
                    "  - ctx_size (размер контекста)\n"
                    "  - n_predict (количество токенов генерации)");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::Spacing();

    ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                       "Select profiles to compare (hold Ctrl for multiple):");
    ImGui::Spacing();

    renderProfileSelection();
    ImGui::Spacing();

    // Показываем параметры только для информации (не редактируемые)
    renderTestParametersInfo();
    ImGui::Spacing();

    if (pimpl_->running) {
        renderProgressIndicator();
        ImGui::Spacing();
    }

    renderActionButtons();
}

void LlamaBenchDialog::renderResultsTab() {
    ImGui::Text("Benchmark Results");
    ImGui::Separator();
    ImGui::Spacing();
    
    renderResultsTable();
    ImGui::Spacing();
    
    // Кнопки экспорта
    if (ImGui::Button("Export JSON")) {
        onExportJson();
    }
    ImGui::SameLine();
    if (ImGui::Button("Export CSV")) {
        onExportCsv();
    }
    ImGui::SameLine();
    if (ImGui::Button("Export Markdown")) {
        onExportMarkdown();
    }
    ImGui::SameLine();
    if (ImGui::Button("Analyze with Model")) {
        onAnalyzeWithModel();
    }
}

void LlamaBenchDialog::renderHistoryTab() {
    ImGui::Text("Benchmark History");
    ImGui::Separator();
    
    const auto& results = pimpl_->bench_module.getResults();
    
    ImGui::Text("Total runs: %zu", results.getTotalResults());
    ImGui::Text("Comparisons: %zu", results.getComparisonsCount());
    
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    
    // История запусков
    if (ImGui::BeginTable("HistoryTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable)) {
        ImGui::TableSetupColumn("Date");
        ImGui::TableSetupColumn("Profile");
        ImGui::TableSetupColumn("Prompt TPS");
        ImGui::TableSetupColumn("Gen TPS");
        ImGui::TableSetupColumn("Status");
        
        ImGui::TableHeadersRow();
        
        // Показать последние 20 результатов
        const auto& all_results = results.getResults();
        int count = 0;
        
        for (auto it = all_results.rbegin(); it != all_results.rend() && count < 20; ++it, ++count) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Recent");
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", it->profile_name.c_str());
            
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.1f", it->prompt_tokens_per_sec);
            
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.1f", it->gen_tokens_per_sec);
            
            ImGui::TableSetColumnIndex(4);
            const char* status_icon = llama_gui::bench::BenchCommon::getStatusIcon(it->status);
            ImGui::Text("%s %s", status_icon, it->getStatusString().c_str());
        }
        
        ImGui::EndTable();
    }
}

// ============================================================================
// Рендеринг компонентов
// ============================================================================

void LlamaBenchDialog::renderProfileSelection() {
    ImGui::Text("Filter Profiles:");

    // Фильтр
    char filter_buf[128] = "";
    if (ImGui::InputTextWithHint("##ProfileFilter", "Search profiles...", filter_buf, IM_ARRAYSIZE(filter_buf))) {
        pimpl_->profiles_filter = filter_buf;
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        onRefreshProfiles();
    }
    
    ImGui::Spacing();
    
    // Список профилей
    if (ImGui::BeginListBox("##ProfilesList", ImVec2(-FLT_MIN, 150))) {
        for (size_t i = 0; i < pimpl_->available_profiles.size(); ++i) {
            const std::string& profile = pimpl_->available_profiles[i];
            
            // Фильтрация
            if (!pimpl_->profiles_filter.empty() && 
                profile.find(pimpl_->profiles_filter) == std::string::npos) {
                continue;
            }
            
            bool selected = pimpl_->profile_selected[i];
            
            if (ImGui::Selectable(profile.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick)) {
                if (ImGui::GetIO().KeyCtrl) {
                    // Ctrl+click для множественного выбора
                    pimpl_->profile_selected[i] = !selected;
                } else {
                    // Одиночный выбор
                    for (auto& s : pimpl_->profile_selected) {
                        s = false;
                    }
                    pimpl_->profile_selected[i] = true;
                }
                onProfileSelected(profile, pimpl_->profile_selected[i]);
            }
        }
        ImGui::EndListBox();
    }
    
    ImGui::TextDisabled("Hold Ctrl to select multiple profiles");
}

void LlamaBenchDialog::renderTestParameters() {
    ImGui::Text("Test Parameters:");
    ImGui::Separator();
    
    if (ImGui::BeginTable("ParamsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();
        
        // Prompt tokens
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Prompt Tokens");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputInt("##NPrompt", &pimpl_->n_prompt);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Number of tokens in the prompt");
        
        // Generation tokens
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Generation Tokens");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputInt("##NGen", &pimpl_->n_gen);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Number of tokens to generate");
        
        // Batch size
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Batch Size");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputInt("##BatchSize", &pimpl_->batch_size);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Processing batch size");
        
        // Threads
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Threads");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputInt("##Threads", &pimpl_->threads);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Number of CPU threads");
        
        // GPU layers
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("GPU Layers");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputInt("##GPULayers", &pimpl_->n_gpu_layers);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Number of layers to offload to GPU");
        
        // Repetitions
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Repetitions");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputInt("##Repetitions", &pimpl_->repetitions);
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Number of test repetitions");
        
        // Context depths
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("Context Depths");
        ImGui::TableSetColumnIndex(1);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##Depths", &pimpl_->context_depths[0], pimpl_->context_depths.capacity());
        ImGui::TableSetColumnIndex(2);
        ImGui::TextDisabled("Comma-separated context depths (e.g., 0,4096,8192)");
        
        ImGui::EndTable();
    }
}

void LlamaBenchDialog::renderTestParametersInfo() {
    // Найти выбранные профили и показать их параметры
    std::vector<std::string> selected;
    for (size_t i = 0; i < pimpl_->available_profiles.size(); ++i) {
        if (pimpl_->profile_selected[i]) {
            selected.push_back(pimpl_->available_profiles[i]);
        }
    }

    if (selected.empty()) {
        ImGui::TextDisabled("Select profiles to see their parameters");
        return;
    }

    ImGui::Text("Parameters from selected profiles (read-only):");
    ImGui::Separator();

    if (ImGui::BeginTable("ProfileParamsTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Profile", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Batch", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("UBatch", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Threads", ImGuiTableColumnFlags_WidthFixed, 70);
        ImGui::TableSetupColumn("GPU", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Ctx", ImGuiTableColumnFlags_WidthFixed, 80);
        ImGui::TableSetupColumn("Cache K/V", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableHeadersRow();

        for (const auto& profile_name : selected) {
            std::string profile_path = pimpl_->profiles_directory + "/" + profile_name + ".json";
            auto params = llama_gui::bench::ProfileAdapter::profileToBenchParams(profile_path);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", profile_name.c_str());

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", params.batch_size);

            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%d", params.ubatch_size);

            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%d", params.threads);

            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", params.n_gpu_layers);

            ImGui::TableSetColumnIndex(5);
            if (!params.n_depth.empty()) {
                ImGui::Text("%d", params.n_depth[0]);
            } else {
                ImGui::TextDisabled("N/A");
            }

            ImGui::TableSetColumnIndex(6);
            std::string cache_info = params.cache_type_k + "/" + params.cache_type_v;
            ImGui::Text("%s", cache_info.c_str());
        }

        ImGui::EndTable();
    }
}

void LlamaBenchDialog::renderResultsTable() {
    const auto& results = pimpl_->bench_module.getResults();
    const auto& all_results = results.getResults();
    
    if (all_results.empty()) {
        ImGui::TextDisabled("No results yet. Run a benchmark first.");
        return;
    }
    
    if (ImGui::BeginTable("ResultsTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Profile", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthFixed, 200);
        ImGui::TableSetupColumn("Prompt TPS", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Gen TPS", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("TTFT (ms)", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Duration", ImGuiTableColumnFlags_WidthFixed, 100);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 100);
        
        ImGui::TableHeadersRow();
        
        for (const auto& result : all_results) {
            ImGui::TableNextRow();
            
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", result.profile_name.c_str());
            
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", result.model_name.c_str());
            
            ImGui::TableSetColumnIndex(2);
            // Подсветка лучших значений
            auto best_prompt_it = std::max_element(all_results.begin(), all_results.end(),
                [](const auto& a, const auto& b) { return a.prompt_tokens_per_sec < b.prompt_tokens_per_sec; });
            bool is_best_prompt = (best_prompt_it != all_results.end() && &result == &(*best_prompt_it));
            if (is_best_prompt) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 128, 0, 64));
            }
            ImGui::Text("%.1f", result.prompt_tokens_per_sec);

            ImGui::TableSetColumnIndex(3);
            auto best_gen_it = std::max_element(all_results.begin(), all_results.end(),
                [](const auto& a, const auto& b) { return a.gen_tokens_per_sec < b.gen_tokens_per_sec; });
            bool is_best_gen = (best_gen_it != all_results.end() && &result == &(*best_gen_it));
            if (is_best_gen) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg, IM_COL32(0, 128, 0, 64));
            }
            ImGui::Text("%.1f", result.gen_tokens_per_sec);
            
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%.0f", result.ttft_ms);
            
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%s", formatDuration(result.duration_seconds).c_str());
            
            ImGui::TableSetColumnIndex(6);
            const char* icon = llama_gui::bench::BenchCommon::getStatusIcon(result.status);
            ImGui::Text("%s %s", icon, result.getStatusString().c_str());
        }
        
        ImGui::EndTable();
    }
}

void LlamaBenchDialog::renderProgressIndicator() {
    ImGui::Text("Status: %s", pimpl_->current_status.c_str());
    
    if (pimpl_->progress >= 0 && pimpl_->progress <= 100) {
        char progress_buf[64];
        snprintf(progress_buf, IM_ARRAYSIZE(progress_buf), "%d%%", pimpl_->progress);
        ImGui::ProgressBar(static_cast<float>(pimpl_->progress) / 100.0f, ImVec2(-FLT_MIN, 20), progress_buf);
    }
}

void LlamaBenchDialog::renderActionButtons() {
    ImGui::Separator();
    ImGui::Spacing();
    
    if (pimpl_->running) {
        // Кнопка отмены
        if (ImGui::Button("Cancel", ImVec2(150, 0))) {
            onCancelBenchmark();
        }
    } else {
        // Кнопка запуска
        if (pimpl_->active_tab == 0) {
            // Одиночный бенчмарк
            if (ImGui::Button("Start Benchmark", ImVec2(150, 0))) {
                onStartBenchmark();
            }
        } else if (pimpl_->active_tab == 1) {
            // Сравнение профилей
            if (ImGui::Button("Start Comparison", ImVec2(150, 0))) {
                onStartBenchmark();
            }
        }
    }
}

void LlamaBenchDialog::renderStatusBar() {
    ImGui::Separator();

    ImGui::BeginGroup();
    ImGui::TextDisabled("Status: %s", pimpl_->current_status.empty() ? "Ready" : pimpl_->current_status.c_str());
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::TextDisabled("Profiles: %zu", pimpl_->available_profiles.size());
    ImGui::EndGroup();

    ImGui::SameLine();
    ImGui::Separator();
    ImGui::SameLine();

    ImGui::BeginGroup();
    ImGui::TextDisabled("Results: %zu", pimpl_->bench_module.getResults().getTotalResults());
    ImGui::EndGroup();
}

// ============================================================================
// Обработчики событий
// ============================================================================

void LlamaBenchDialog::onStartBenchmark() {
    // Найти выбранные профили
    std::vector<std::string> selected;
    for (size_t i = 0; i < pimpl_->available_profiles.size(); ++i) {
        if (pimpl_->profile_selected[i]) {
            selected.push_back(pimpl_->available_profiles[i]);
        }
    }

    if (selected.empty()) {
        pimpl_->error_message = "No profiles selected";
        pimpl_->show_error_modal = true;
        return;
    }

    // Остановить сервер для бенчмарка
    stopServerForBenchmark();

    // Загрузить параметры из первого выбранного профиля
    llama_gui::bench::BenchTestParams params;
    std::string first_profile_path = pimpl_->profiles_directory + "/" + selected[0] + ".json";
    params = llama_gui::bench::ProfileAdapter::profileToBenchParams(first_profile_path);

    // Проверить что модель найдена
    if (params.model_path.empty()) {
        pimpl_->error_message = "Model path not found in profile: " + selected[0];
        pimpl_->show_error_modal = true;
        restartServerAfterBenchmark();  // Запустить сервер снова при ошибке
        return;
    }

    pimpl_->running = true;
    pimpl_->progress = 0;

    if (pimpl_->active_tab == 0 && selected.size() == 1) {
        // Одиночный бенчмарк - использовать параметры из профиля
        pimpl_->bench_module.runBenchmark(selected[0], params);
    } else {
        // Сравнение профилей - загрузить КАЖДЫЙ профиль со СВОИМИ параметрами
        std::vector<std::string> profile_paths;
        std::vector<llama_gui::bench::BenchTestParams> all_params;

        for (const auto& profile : selected) {
            std::string profile_path = pimpl_->profiles_directory + "/" + profile + ".json";
            auto prof_params = llama_gui::bench::ProfileAdapter::profileToBenchParams(profile_path);

            // НЕ переопределять параметры - использовать из профиля!
            // Каждый профиль будет запущен со своими собственными настройками

            if (!prof_params.model_path.empty()) {
                profile_paths.push_back(profile_path);
                all_params.push_back(prof_params);
            }
        }

        if (profile_paths.empty()) {
            pimpl_->error_message = "No valid profiles found";
            pimpl_->show_error_modal = true;
            pimpl_->running = false;
            restartServerAfterBenchmark();  // Запустить сервер снова при ошибке
            return;
        }

        // Запустить сравнение с первым профилем (остальные будут в модуле)
        pimpl_->bench_module.runComparison(profile_paths, all_params[0]);
    }
}

void LlamaBenchDialog::onCancelBenchmark() {
    pimpl_->bench_module.cancelCurrentRun();
    pimpl_->running = false;
}

void LlamaBenchDialog::onExportJson() {
    std::string path = pimpl_->bench_module.getResultsDirectory() + "/export_" + 
                       llama_gui::bench::BenchCommon::getTimestampForFileName() + ".json";
    
    if (pimpl_->bench_module.saveResults(path)) {
        pimpl_->current_status = "Exported to: " + path;
    } else {
        pimpl_->error_message = "Failed to export results";
        pimpl_->show_error_modal = true;
    }
}

void LlamaBenchDialog::onExportCsv() {
    std::string content = pimpl_->bench_module.getResults().exportToCsv();
    std::string path = pimpl_->bench_module.getResultsDirectory() + "/export_" + 
                       llama_gui::bench::BenchCommon::getTimestampForFileName() + ".csv";
    
    if (llama_gui::bench::BenchCommon::writeStringToFile(path, content)) {
        pimpl_->current_status = "Exported to: " + path;
    } else {
        pimpl_->error_message = "Failed to export CSV";
        pimpl_->show_error_modal = true;
    }
}

void LlamaBenchDialog::onExportMarkdown() {
    std::string content = pimpl_->bench_module.getResults().exportToMarkdown();
    std::string path = pimpl_->bench_module.getResultsDirectory() + "/export_" + 
                       llama_gui::bench::BenchCommon::getTimestampForFileName() + ".md";
    
    if (llama_gui::bench::BenchCommon::writeStringToFile(path, content)) {
        pimpl_->current_status = "Exported to: " + path;
    } else {
        pimpl_->error_message = "Failed to export Markdown";
        pimpl_->show_error_modal = true;
    }
}

void LlamaBenchDialog::onAnalyzeWithModel() {
    // TODO: Интеграция с RAG для анализа
    pimpl_->error_message = "Model analysis not yet implemented";
    pimpl_->show_error_modal = true;
}

void LlamaBenchDialog::onProfileSelected(const std::string& profile, bool selected) {
    // Обработка выбора профиля
}

void LlamaBenchDialog::onRefreshProfiles() {
    updateProfileList();
}

// ============================================================================
// Вспомогательные методы
// ============================================================================

void LlamaBenchDialog::updateProfileList() {
    pimpl_->available_profiles.clear();
    pimpl_->profile_selected.clear();
    
    // Загрузить профили из директории
    auto profiles = llama_gui::bench::BenchCommon::listFilesWithExtension(
        pimpl_->profiles_directory, ".json");
    
    for (const auto& path : profiles) {
        std::string name = llama_gui::bench::BenchCommon::extractFileNameWithoutExt(path);
        pimpl_->available_profiles.push_back(name);
        pimpl_->profile_selected.push_back(false);
    }
}

void LlamaBenchDialog::refreshResults() {
    // Обновить отображение результатов
}

std::string LlamaBenchDialog::formatDuration(double seconds) const {
    return llama_gui::bench::BenchCommon::formatDuration(seconds);
}

std::string LlamaBenchDialog::formatSpeed(double tps) const {
    return llama_gui::bench::BenchCommon::formatSpeed(tps);
}

std::string LlamaBenchDialog::formatTimeMs(double ms) const {
    return llama_gui::bench::BenchCommon::formatTimeMs(ms);
}

void LlamaBenchDialog::drawColoredText(const char* text, uint32_t color) {
    // Извлечь RGBA из цвета
    float r = ((color >> 0) & 0xFF) / 255.0f;
    float g = ((color >> 8) & 0xFF) / 255.0f;
    float b = ((color >> 16) & 0xFF) / 255.0f;
    float a = ((color >> 24) & 0xFF) / 255.0f;
    
    ImGui::TextColored(ImVec4(r, g, b, a), "%s", text);
}

std::string LlamaBenchDialog::findLlamaBenchPath() const {
    // Поиск llama-bench в стандартных местах
    
    // 1. Родительская директория проекта
    std::string parent_path = "../llama-b7472/llama-bench";
    if (llama_gui::bench::BenchCommon::fileExists(parent_path)) {
        return parent_path;
    }
    
    // 2. Абсолютный путь (для разработки)
    std::string abs_path = "/home/Alex/projects/llama-b7472-bin-ubuntu-x64/llama-b7472/llama-bench";
    if (llama_gui::bench::BenchCommon::fileExists(abs_path)) {
        return abs_path;
    }
    
    // 3. PATH
    const char* path_env = std::getenv("PATH");
    if (path_env) {
        std::istringstream iss(path_env);
        std::string path_dir;
        
        while (std::getline(iss, path_dir, ':')) {
            std::string full_path = path_dir + "/llama-bench";
            if (llama_gui::bench::BenchCommon::fileExists(full_path)) {
                return full_path;
            }
        }
    }
    
    return "";
}

} // namespace ui
} // namespace llama_gui
