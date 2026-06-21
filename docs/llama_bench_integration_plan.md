# План реализации модуля "Llama Bench Integration"

## Обзор проекта

**Цель:** Интеграция llama-bench для сравнения профилей конфигурации с возможностью анализа результатов моделью.

**Версия плана:** 1.0  
**Дата создания:** 25 марта 2026 г.  
**Статус:** Утверждён

---

## Архитектурные принципы

1. **Модульность** — весь код в отдельных папках `src/bench/` и `include/bench/`
2. **Изоляция** — новый код не влияет на существующую функциональность
3. **Опциональность** — можно включить/выключить через CMake опцию
4. **Единая точка входа** — `LlamaBenchModule` как фасад для всех операций
5. **Совместимость** — использование существующих профилей, RAG, UI стиля

---

## ФАЗА 1: Базовая инфраструктура

**Цель:** Создать базовые классы для запуска llama-bench и парсинга результатов.

**Время реализации:** 3-4 дня

### Задача 1.1: Структура проекта и базовые типы

**Описание:**
- Создать папки `src/bench/` и `include/bench/`
- Определить базовые структуры данных для результатов бенчмарка
- Создать enum для статусов выполнения

**Файлы для создания:**
```
include/bench/bench_types.h
include/bench/bench_common.h
```

**Содержание `bench_types.h`:**
```cpp
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace llama_gui {
namespace bench {

// Статус выполнения теста
enum class BenchStatus {
    Pending,
    Running,
    Completed,
    Failed,
    Cancelled
};

// Тип вывода
enum class OutputFormat {
    CSV,
    JSON,
    JSONL,
    Markdown,
    SQL
};

// Параметры одного теста
struct BenchTestParams {
    std::string model_path;
    int n_prompt = 512;        // количество токенов промпта
    int n_gen = 128;           // количество генерируемых токенов
    int n_depth = 0;           // глубина (0 = auto)
    int batch_size = 2048;
    int ubatch_size = 512;
    int threads = 2;
    int n_gpu_layers = 99;
    std::string cache_type_k = "f16";
    std::string cache_type_v = "f16";
    int repetitions = 5;
    float delay_seconds = 0.0f;
    bool flash_attn = false;
    bool embeddings = false;
    bool mmap = true;
};

// Результат одного запуска
struct BenchRunResult {
    std::string test_id;
    BenchStatus status;
    
    // Параметры теста
    BenchTestParams params;
    
    // Метрики производительности
    double prompt_tokens_per_sec = 0.0;
    double gen_tokens_per_sec = 0.0;
    double prompt_ms_total = 0.0;
    double gen_ms_total = 0.0;
    double prompt_ms_per_token = 0.0;
    double gen_ms_per_token = 0.0;
    
    // Статистика по повторениям
    int repetitions_completed = 0;
    std::vector<double> prompt_tpss;  // tokens/sec для каждого повторения
    std::vector<double> gen_tpss;
    
    // Ошибки
    std::string error_message;
    
    // Время выполнения
    std::chrono::system_clock::time_point start_time;
    std::chrono::system_clock::time_point end_time;
    double duration_seconds = 0.0;
};

// Сводные результаты сравнения профилей
struct BenchComparisonResult {
    std::string comparison_id;
    std::chrono::system_clock::time_point created_at;
    
    // Профили которые сравнивались
    std::vector<std::string> profile_names;
    
    // Результаты по каждому профилю
    std::vector<BenchRunResult> results;
    
    // Лучший профиль по метрикам
    std::string best_prompt_tps_profile;
    std::string best_gen_tps_profile;
    
    // Статистика
    double total_duration_seconds = 0.0;
    int total_tests_run = 0;
    int total_tests_failed = 0;
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Нет  
**Время:** 2-3 часа

---

### Задача 1.2: LlamaBenchRunner — запуск llama-bench

**Описание:**
- Класс для запуска исполняемого файла llama-bench
- Поддержка асинхронного выполнения
- Перехват stdout/stderr
- Обработка прерывания

**Файлы для создания:**
```
include/bench/llama_bench_runner.h
src/bench/llama_bench_runner.cpp
```

**Содержание `llama_bench_runner.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>

namespace llama_gui {
namespace bench {

/**
 * @class LlamaBenchRunner
 * @brief Запуск llama-bench с заданными параметрами
 * 
 * Поддерживает:
 * - Асинхронный запуск
 * - Прогресс выполнения
 * - Перехват вывода
 * - Отмену выполнения
 */
class LlamaBenchRunner {
public:
    using ProgressCallback = std::function<void(int current, int total, const std::string& status)>;
    using OutputCallback = std::function<void(const std::string& line)>;
    using CompletionCallback = std::function<void(const BenchRunResult&)>;

    LlamaBenchRunner(const std::string& llama_bench_path);
    ~LlamaBenchRunner();

    // Запуск теста (асинхронно)
    bool startTest(const BenchTestParams& params);
    
    // Остановка текущего теста
    void cancel();
    
    // Проверка статуса
    bool isRunning() const;
    BenchStatus getStatus() const;
    int getProgress() const;  // 0-100%
    
    // Колбэки
    void setProgressCallback(ProgressCallback cb);
    void setOutputCallback(OutputCallback cb);
    void setCompletionCallback(CompletionCallback cb);
    
    // Получить результат после завершения
    const BenchRunResult& getResult() const;
    
    // Настройки
    void setOutputFormat(OutputFormat format);
    void setVerbose(bool verbose);
    void setWorkingDirectory(const std::string& dir);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    void runTestAsync(const BenchTestParams& params);
    std::string buildCommandLine(const BenchTestParams& params);
    BenchRunResult parseOutput(const std::string& output, OutputFormat format);
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Задача 1.1  
**Время:** 4-5 часов

---

### Задача 1.3: LlamaBenchParser — парсинг вывода

**Описание:**
- Парсинг JSON/CSV/Markdown вывода llama-bench
- Извлечение метрик производительности
- Валидация данных

**Файлы для создания:**
```
include/bench/llama_bench_parser.h
src/bench/llama_bench_parser.cpp
```

**Содержание `llama_bench_parser.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace llama_gui {
namespace bench {

/**
 * @class LlamaBenchParser
 * @brief Парсинг вывода llama-bench в структурированные данные
 */
class LlamaBenchParser {
public:
    // Парсинг JSON вывода
    static std::vector<BenchRunResult> parseJson(const std::string& json_input);
    
    // Парсинг CSV вывода
    static std::vector<BenchRunResult> parseCsv(const std::string& csv_input);
    
    // Парсинг Markdown таблицы
    static std::vector<BenchRunResult> parseMarkdown(const std::string& md_input);
    
    // Авто-определение формата и парсинг
    static std::vector<BenchRunResult> parse(const std::string& input, OutputFormat hint = OutputFormat::JSON);
    
    // Валидация результата
    static bool validateResult(const BenchRunResult& result);
    
    // Извлечение метрик из JSON объекта nlohmann
    static BenchRunResult extractFromJson(const nlohmann::json& json_obj);

private:
    // Вспомогательные методы
    static std::vector<std::string> splitCsvLine(const std::string& line);
    static std::vector<std::string> splitMarkdownTable(const std::string& md);
    static double parseDouble(const std::string& str, double default_val = 0.0);
    static int parseInt(const std::string& str, int default_val = 0);
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Задача 1.1  
**Время:** 3-4 часа

---

### Задача 1.4: LlamaBenchResults — модель данных и хранение

**Описание:**
- Класс для управления коллекцией результатов
- Сериализация/десериализация в JSON
- Методы для сравнения и агрегации

**Файлы для создания:**
```
include/bench/llama_bench_results.h
src/bench/llama_bench_results.cpp
```

**Содержание `llama_bench_results.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

namespace llama_gui {
namespace bench {

/**
 * @class LlamaBenchResults
 * @brief Управление коллекцией результатов бенчмарков
 * 
 * Функции:
 * - Добавление результатов
 * - Сохранение/загрузка из файла
 * - Поиск и фильтрация
 * - Сравнение профилей
 * - Экспорт в различные форматы
 */
class LlamaBenchResults {
public:
    LlamaBenchResults();
    explicit LlamaBenchResults(const std::string& storage_path);
    ~LlamaBenchResults();

    // Добавление результатов
    void addResult(const BenchRunResult& result);
    void addComparison(const BenchComparisonResult& comparison);
    
    // Сохранение и загрузка
    bool saveToFile(const std::string& file_path);
    bool loadFromFile(const std::string& file_path);
    bool saveToHistory();  // Сохранить в history.json
    
    // Поиск и фильтрация
    std::vector<BenchRunResult> findByProfile(const std::string& profile_name) const;
    std::vector<BenchRunResult> findByDateRange(
        std::chrono::system_clock::time_point from,
        std::chrono::system_clock::time_point to) const;
    std::vector<BenchRunResult> findBestByMetric(const std::string& metric, int top_n = 5) const;
    
    // Сравнение
    BenchComparisonResult compareProfiles(const std::vector<std::string>& profile_names) const;
    
    // Экспорт
    std::string exportToJson() const;
    std::string exportToCsv() const;
    std::string exportToMarkdown() const;
    bool exportToFile(const std::string& file_path, OutputFormat format);
    
    // Статистика
    size_t getTotalResults() const;
    size_t getComparisonsCount() const;
    std::chrono::system_clock::time_point getLastRunTime() const;
    
    // Очистка
    void clear();
    void removeOlderThan(std::chrono::system_clock::time_point cutoff);

    // Сериализация
    nlohmann::json toJson() const;
    static LlamaBenchResults fromJson(const nlohmann::json& json);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    std::string storage_path_;
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Задачи 1.1, 1.3  
**Время:** 4-5 часов

---

### Задача 1.5: LlamaBenchModule — точка входа

**Описание:**
- Фасад для всех операций бенчмарка
- Координация между Runner, Parser, Results
- Интеграция с основной системой

**Файлы для создания:**
```
include/bench/llama_bench_module.h
src/bench/llama_bench_module.cpp
```

**Содержание `llama_bench_module.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include "llama_bench_results.h"
#include <string>
#include <memory>
#include <functional>

namespace llama_gui {
namespace bench {

class LlamaBenchRunner;

/**
 * @class LlamaBenchModule
 * @brief Основная точка входа для функциональности llama-bench
 * 
 * Функции:
 * - Инициализация модуля
 * - Запуск бенчмарков
 * - Управление результатами
 * - Интеграция с UI
 */
class LlamaBenchModule {
public:
    using StatusCallback = std::function<void(const std::string& status)>;
    using ProgressCallback = std::function<void(int percent, const std::string& status)>;

    LlamaBenchModule();
    ~LlamaBenchModule();

    // Инициализация
    bool initialize(const std::string& llama_bench_path,
                   const std::string& results_directory = "bench_results");
    void shutdown();
    bool isInitialized() const;
    
    // Запуск бенчмарка для профиля
    bool runBenchmark(const std::string& profile_path,
                     const BenchTestParams& params);
    
    // Запуск сравнения нескольких профилей
    bool runComparison(const std::vector<std::string>& profile_paths,
                      const BenchTestParams& params);
    
    // Управление выполнением
    void cancelCurrentRun();
    bool isRunning() const;
    int getProgress() const;
    std::string getCurrentStatus() const;
    
    // Результаты
    const LlamaBenchResults& getResults() const;
    LlamaBenchResults& getResults();
    
    // Колбэки для UI
    void setStatusCallback(StatusCallback cb);
    void setProgressCallback(ProgressCallback cb);
    
    // Настройки
    void setDefaultParams(const BenchTestParams& params);
    BenchTestParams getDefaultParams() const;
    
    // Путь к llama-bench
    std::string getLlamaBenchPath() const;
    std::string getResultsDirectory() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Задачи 1.2, 1.3, 1.4  
**Время:** 3-4 часа

---

### Задача 1.6: CMake интеграция

**Описание:**
- Добавить опцию `ENABLE_LLAMA_BENCH`
- Включить файлы bench в сборку
- Настроить копирование ресурсов

**Файлы для изменения:**
```
CMakeLists.txt
```

**Изменения в CMakeLists.txt:**
```cmake
# ============================================================================
# Llama Bench Integration - Опциональная сборка
# ============================================================================
option(ENABLE_LLAMA_BENCH "Enable Llama Bench integration" ON)
if(ENABLE_LLAMA_BENCH)
    add_definitions(-DENABLE_LLAMA_BENCH)
    
    set(BENCH_SOURCES
        src/bench/llama_bench_runner.cpp
        src/bench/llama_bench_parser.cpp
        src/bench/llama_bench_results.cpp
        src/bench/llama_bench_module.cpp
    )
    
    set(BENCH_HEADERS
        include/bench/bench_types.h
        include/bench/llama_bench_runner.h
        include/bench/llama_bench_parser.h
        include/bench/llama_bench_results.h
        include/bench/llama_bench_module.h
    )
    
    # Добавить исходники в основной executable
    target_sources(llama-gui-core PRIVATE ${BENCH_SOURCES} ${BENCH_HEADERS})
    
    # Копирование директории результатов в build
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/bench_results)
        file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/bench_results/
             DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/bench_results/)
        message(STATUS "Bench results directory copied to build directory")
    endif()
    
    message(STATUS "Llama Bench integration enabled")
else()
    message(STATUS "Llama Bench integration disabled")
endif()
```

**Зависимости:** Нет (параллельно с другими задачами)  
**Время:** 1 час

---

## ФАЗА 2: UI модуль

**Цель:** Создать пользовательский интерфейс для работы с llama-bench.

**Время реализации:** 4-5 дней

### Задача 2.1: LlamaBenchDialog — основной диалог

**Описание:**
- Отдельное окно/диалог "Llama Bench"
- Интеграция с DialogManager
- Основной layout с вкладками

**Файлы для создания:**
```
include/ui/llama_bench_dialog.h
src/ui/llama_bench_dialog.cpp
```

**Содержание `llama_bench_dialog.h`:**
```cpp
#pragma once

#include "../external/imgui/imgui.h"
#include "../bench/llama_bench_module.h"
#include <string>
#include <vector>
#include <memory>

namespace llama_gui {
namespace ui {

/**
 * @class LlamaBenchDialog
 * @brief Диалог для работы с llama-bench
 * 
 * Вкладки:
 * 1. Run Benchmark - запуск теста
 * 2. Compare Profiles - сравнение профилей
 * 3. Results - просмотр результатов
 * 4. History - история запусков
 */
class LlamaBenchDialog {
public:
    LlamaBenchDialog();
    ~LlamaBenchDialog();

    // Управление видимостью
    void setVisible(bool visible);
    bool isVisible() const;
    void toggle();
    
    // Рендеринг
    void render();
    
    // Инициализация модуля
    bool initialize(const std::string& llama_bench_path);
    void shutdown();
    
    // Сброс состояния
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    // Рендеринг вкладок
    void renderRunBenchmarkTab();
    void renderCompareProfilesTab();
    void renderResultsTab();
    void renderHistoryTab();
    
    // Рендеринг компонентов
    void renderProfileSelection();
    void renderTestParameters();
    void renderResultsTable();
    void renderProgressIndicator();
    void renderActionButtons();
    
    // Обработчики событий
    void onStartBenchmark();
    void onCancelBenchmark();
    void onExportJson();
    void onExportCsv();
    void onAnalyzeWithModel();
    void onProfileSelected(const std::string& profile, bool selected);
    
    // Вспомогательные методы
    void updateProfileList();
    void refreshResults();
    std::string formatDuration(double seconds) const;
    std::string formatSpeed(double tps) const;
};

} // namespace ui
} // namespace llama_gui
```

**Зависимости:** Фаза 1 завершена  
**Время:** 6-8 часов

---

### Задача 2.2: Компонент выбора профилей

**Описание:**
- Чекбоксы для выбора профилей
- Загрузка из папки profiles/
- Предпросмотр параметров профиля

**Файлы для создания:**
```
include/ui/bench_profile_selector.h
src/ui/bench_profile_selector.cpp
```

**Содержание `bench_profile_selector.h`:**
```cpp
#pragma once

#include "../external/imgui/imgui.h"
#include <string>
#include <vector>
#include <functional>

namespace llama_gui {
namespace ui {

struct ProfileInfo {
    std::string name;
    std::string path;
    std::string description;
    bool selected = false;
    
    // Основные параметры из профиля
    int threads = 0;
    int n_gpu_layers = 0;
    int ctx_size = 0;
    int batch_size = 0;
};

/**
 * @class BenchProfileSelector
 * @brief Компонент выбора профилей для бенчмарка
 */
class BenchProfileSelector {
public:
    using SelectionChangedCallback = std::function<void(const std::string& profile, bool selected)>;

    BenchProfileSelector();
    ~BenchProfileSelector();

    // Загрузка профилей
    bool loadFromDirectory(const std::string& profiles_dir);
    void clear();
    
    // Рендеринг
    void render();
    
    // Выбор
    std::vector<std::string> getSelectedProfiles() const;
    std::vector<ProfileInfo> getSelectedProfileInfos() const;
    int getSelectedCount() const;
    
    // Колбэки
    void setSelectionChangedCallback(SelectionChangedCallback cb);
    
    // Фильтрация
    void setFilterText(const std::string& text);
    void selectAll();
    void deselectAll();
    
    // Информация о профиле
    const ProfileInfo* getProfileInfo(const std::string& name) const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    void renderProfileItem(ProfileInfo& profile);
    void renderProfileTooltip(const ProfileInfo& profile);
};

} // namespace ui
} // namespace llama_gui
```

**Зависимости:** Задача 2.1  
**Время:** 3-4 часа

---

### Задача 2.3: Компонент параметров теста

**Описание:**
- Поля ввода для параметров теста
- Пресеты конфигураций
- Валидация значений

**Файлы для создания:**
```
include/ui/bench_test_params_editor.h
src/ui/bench_test_params_editor.cpp
```

**Содержание `bench_test_params_editor.h`:**
```cpp
#pragma once

#include "../external/imgui/imgui.h"
#include "../bench/bench_types.h"
#include <string>

namespace llama_gui {
namespace ui {

/**
 * @class BenchTestParamsEditor
 * @brief Редактор параметров теста для llama-bench
 */
class BenchTestParamsEditor {
public:
    BenchTestParamsEditor();
    ~BenchTestParamsEditor();

    // Рендеринг
    void render();
    
    // Получение/установка параметров
    llama_gui::bench::BenchTestParams getParams() const;
    void setParams(const llama_gui::bench::BenchTestParams& params);
    
    // Пресеты
    void loadPreset(const std::string& preset_name);
    void saveAsPreset(const std::string& preset_name);
    std::vector<std::string> getPresetNames() const;
    
    // Валидация
    bool validate() const;
    std::string getValidationError() const;
    
    // Сброс к значениям по умолчанию
    void resetToDefaults();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    void renderPromptTokensSection();
    void renderGenTokensSection();
    void renderBatchSizeSection();
    void renderThreadsSection();
    void renderGpuSection();
    void renderAdvancedSection();
    void renderPresetsMenu();
    
    bool validateField(const std::string& field_name, int value, int min_val, int max_val);
};

} // namespace ui
} // namespace llama_gui
```

**Зависимости:** Задача 2.1  
**Время:** 3-4 часа

---

### Задача 2.4: Таблица результатов

**Описание:**
- Отображение результатов в виде таблицы
- Сортировка по колонкам
- Подсветка лучших значений

**Файлы для создания:**
```
include/ui/bench_results_table.h
src/ui/bench_results_table.cpp
```

**Содержание `bench_results_table.h`:**
```cpp
#pragma once

#include "../external/imgui/imgui.h"
#include "../bench/bench_types.h"
#include <string>
#include <vector>
#include <functional>

namespace llama_gui {
namespace ui {

enum class SortColumn {
    ProfileName,
    PromptTPS,
    GenTPS,
    PromptMs,
    GenMs,
    Duration,
    Status
};

enum class SortOrder {
    Ascending,
    Descending
};

/**
 * @class BenchResultsTable
 * @brief Таблица для отображения результатов бенчмарка
 */
class BenchResultsTable {
public:
    using RowSelectedCallback = std::function<void(const std::string& test_id, bool selected)>;

    BenchResultsTable();
    ~BenchResultsTable();

    // Данные
    void setResults(const std::vector<llama_gui::bench::BenchRunResult>& results);
    void clear();
    
    // Рендеринг
    void render();
    
    // Сортировка
    void sortBy(SortColumn column);
    SortColumn getSortColumn() const;
    SortOrder getSortOrder() const;
    
    // Фильтрация
    void setFilterText(const std::string& text);
    void filterByStatus(llama_gui::bench::BenchStatus status);
    
    // Выбор
    std::vector<std::string> getSelectedTestIds() const;
    void setRowSelectedCallback(RowSelectedCallback cb);
    
    // Экспорт видимых данных
    std::vector<llama_gui::bench::BenchRunResult> getVisibleResults() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    void renderHeader();
    void renderRow(const llama_gui::bench::BenchRunResult& result, int row_index);
    void renderCell(const llama_gui::bench::BenchRunResult& result, SortColumn column);
    
    void sortResults();
    std::vector<llama_gui::bench::BenchRunResult> filterResults(
        const std::vector<llama_gui::bench::BenchRunResult>& input) const;
    
    std::string formatCellValue(const llama_gui::bench::BenchRunResult& result, SortColumn column) const;
    ImVec4 getCellColor(const llama_gui::bench::BenchRunResult& result, SortColumn column) const;
    const char* getStatusIcon(llama_gui::bench::BenchStatus status) const;
};

} // namespace ui
} // namespace llama_gui
```

**Зависимости:** Задача 2.1  
**Время:** 4-5 часов

---

### Задача 2.5: Индикатор прогресса

**Описание:**
- Визуализация прогресса выполнения
- Отображение текущего теста
- ETA (estimated time of arrival)

**Файлы для создания:**
```
include/ui/bench_progress_indicator.h
src/ui/bench_progress_indicator.cpp
```

**Содержание `bench_progress_indicator.h`:**
```cpp
#pragma once

#include "../external/imgui/imgui.h"
#include <string>
#include <chrono>

namespace llama_gui {
namespace ui {

/**
 * @class BenchProgressIndicator
 * @brief Индикатор прогресса выполнения бенчмарка
 */
class BenchProgressIndicator {
public:
    BenchProgressIndicator();
    ~BenchProgressIndicator();

    // Обновление состояния
    void setProgress(int current, int total);
    void setStatus(const std::string& status);
    void setCurrentTest(const std::string& test_name);
    void setStartTime(std::chrono::system_clock::time_point start);
    
    // Завершение
    void setCompleted(bool success);
    void setError(const std::string& error_message);
    
    // Рендеринг
    void render();
    void renderCompact();  // Компактный вид для основного окна
    void renderFull();     // Полный вид для диалога
    
    // Сброс
    void reset();
    
    // Получение состояния
    int getProgressPercent() const;
    std::string getETA() const;  // Оставшееся время
    std::string getElapsedTime() const;  // Прошедшее время

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    void renderProgressBar();
    void renderStatusText();
    void renderTimingInfo();
    
    std::string formatTime(double seconds) const;
    std::chrono::system_clock::time_point calculateETA() const;
};

} // namespace ui
} // namespace llama_gui
```

**Зависимости:** Задача 2.1  
**Время:** 2-3 часа

---

### Задача 2.6: Интеграция с MainWindow

**Описание:**
- Добавить пункт меню "Llama Bench"
- Кнопка на toolbar (опционально)
- Горячие клавиши

**Файлы для изменения:**
```
include/ui/main_window.h
src/ui/main_window_init_constructor.cpp  (или другой файл инициализации)
src/ui/main_window_rendering_core.cpp
src/ui/main_window_events.cpp
```

**Изменения:**
1. Добавить `#include "llama_bench_dialog.h"`
2. Добавить член `std::unique_ptr<LlamaBenchDialog> llama_bench_dialog_;`
3. Добавить инициализацию в конструкторе
4. Добавить рендеринг в `render()`
5. Добавить обработчик меню

**Зависимости:** Задача 2.1  
**Время:** 2-3 часа

---

## ФАЗА 3: Интеграция с профилями

**Цель:** Интеграция с существующей системой профилей проекта.

**Время реализации:** 2-3 дня

### Задача 3.1: Профиль-адаптер для llama-bench

**Описание:**
- Конвертация профилей проекта в параметры llama-bench
- Чтение параметров из JSON профилей
- Маппинг полей

**Файлы для создания:**
```
include/bench/profile_adapter.h
src/bench/profile_adapter.cpp
```

**Содержание `profile_adapter.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include <string>
#include <nlohmann/json.hpp>

namespace llama_gui {
namespace bench {

/**
 * @class ProfileAdapter
 * @brief Адаптер профилей проекта для llama-bench
 * 
 * Конвертирует профили из формата проекта в параметры llama-bench
 */
class ProfileAdapter {
public:
    // Конвертация профиля проекта в параметры бенчмарка
    static BenchTestParams profileToBenchParams(const std::string& profile_path);
    static BenchTestParams profileToBenchParams(const nlohmann::json& profile_json);
    
    // Извлечение конкретных параметров из JSON
    static int extractThreads(const nlohmann::json& json);
    static int extractBatchSize(const nlohmann::json& json);
    static int extractGpuLayers(const nlohmann::json& json);
    static std::string extractModelPath(const nlohmann::json& json);
    static int extractContextSize(const nlohmann::json& json);
    
    // Получить список доступных профилей
    static std::vector<std::string> getAvailableProfiles(const std::string& profiles_dir);
    
    // Загрузить профиль по имени
    static nlohmann::json loadProfile(const std::string& profile_name,
                                     const std::string& profiles_dir);
    
    // Валидация профиля для бенчмарка
    static bool validateProfileForBenchmark(const nlohmann::json& profile,
                                           std::string& error_message);

private:
    // Маппинг имён полей между форматами
    static const std::unordered_map<std::string, std::string> field_mapping_;
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Фаза 1 завершена  
**Время:** 3-4 часа

---

### Задача 3.2: Менеджер профилей для бенчмарка

**Описание:**
- Кэширование загруженных профилей
- Быстрый доступ к параметрам
- Отслеживание изменений

**Файлы для создания:**
```
include/bench/bench_profile_manager.h
src/bench/bench_profile_manager.cpp
```

**Содержание `bench_profile_manager.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include "profile_adapter.h"
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace llama_gui {
namespace bench {

struct CachedProfile {
    std::string name;
    std::string path;
    nlohmann::json json_data;
    BenchTestParams bench_params;
    time_t last_modified;
    bool is_valid;
    std::string error_message;
};

/**
 * @class BenchProfileManager
 * @brief Управление профилями для бенчмарка
 */
class BenchProfileManager {
public:
    BenchProfileManager();
    explicit BenchProfileManager(const std::string& profiles_directory);
    ~BenchProfileManager();

    // Инициализация
    bool initialize(const std::string& profiles_directory);
    void refresh();  // Перезагрузить профили
    
    // Получение профилей
    std::vector<std::string> getProfileNames() const;
    const CachedProfile* getProfile(const std::string& name) const;
    const CachedProfile* getProfileByPath(const std::string& path) const;
    
    // Параметры для бенчмарка
    BenchTestParams getBenchParams(const std::string& profile_name) const;
    std::vector<BenchTestParams> getBenchParamsForProfiles(
        const std::vector<std::string>& profile_names) const;
    
    // Валидация
    bool isProfileValid(const std::string& profile_name) const;
    std::string getProfileError(const std::string& profile_name) const;
    
    // Фильтрация
    std::vector<std::string> filterByPattern(const std::string& pattern) const;
    std::vector<std::string> getValidProfiles() const;
    
    // Информация
    size_t getProfilesCount() const;
    size_t getValidProfilesCount() const;
    std::string getProfilesDirectory() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    void loadProfile(const std::string& path);
    void validateProfile(CachedProfile& profile);
    time_t getFileModificationTime(const std::string& path) const;
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Задача 3.1  
**Время:** 3-4 часа

---

### Задача 3.3: Синхронизация с изменениями профилей

**Описание:**
- Отслеживание изменений файлов профилей
- Автоматическое обновление кэша
- Уведомления UI

**Файлы для создания:**
```
include/bench/profile_watcher.h
src/bench/profile_watcher.cpp
```

**Содержание `profile_watcher.h`:**
```cpp
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>
#include <atomic>

namespace llama_gui {
namespace bench {

/**
 * @class ProfileWatcher
 * @brief Отслеживание изменений в файлах профилей
 */
class ProfileWatcher {
public:
    using ProfileChangedCallback = std::function<void(const std::string& profile_path)>;
    using ProfilesRefreshedCallback = std::function<void()>;

    ProfileWatcher();
    ~ProfileWatcher();

    // Запуск наблюдения
    bool startWatching(const std::string& profiles_directory);
    void stopWatching();
    bool isWatching() const;
    
    // Колбэки
    void setProfileChangedCallback(ProfileChangedCallback cb);
    void setProfilesRefreshedCallback(ProfilesRefreshedCallback cb);
    
    // Настройки
    void setPollIntervalMs(int interval_ms);
    void setRecursive(bool recursive);

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    void watchThread();
    std::vector<std::string> scanDirectory(const std::string& dir, bool recursive) const;
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Задача 3.2  
**Время:** 2-3 часа

---

## ФАЗА 4: Анализ моделью (RAG интеграция)

**Цель:** Интеграция с существующим RAG для анализа результатов бенчмарка.

**Время реализации:** 3-4 дня

### Задача 4.1: Экспорт результатов в JSON для анализа

**Описание:**
- Форматирование результатов в JSON для RAG
- Добавление метаданных
- Оптимизация для контекста

**Файлы для создания:**
```
include/bench/bench_exporter.h
src/bench/bench_exporter.cpp
```

**Содержание `bench_exporter.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include "llama_bench_results.h"
#include <string>
#include <nlohmann/json.hpp>

namespace llama_gui {
namespace bench {

/**
 * @class BenchExporter
 * @brief Экспорт результатов бенчмарка в различные форматы
 */
class BenchExporter {
public:
    // Экспорт в JSON для RAG анализа
    static std::string exportForRagAnalysis(const LlamaBenchResults& results);
    
    // Экспорт сравнения профилей
    static std::string exportComparisonForRag(const BenchComparisonResult& comparison);
    
    // Экспорт в Markdown отчёт
    static std::string exportMarkdownReport(const LlamaBenchResults& results);
    
    // Экспорт в CSV
    static std::string exportCsv(const std::vector<BenchRunResult>& results);
    
    // Экспорт в файл
    static bool exportToFile(const LlamaBenchResults& results,
                            const std::string& file_path,
                            OutputFormat format);
    
    // Создание summary для контекста
    static std::string createSummaryContext(const LlamaBenchResults& results,
                                           int max_tokens = 2000);

private:
    // Вспомогательные методы
    static nlohmann::json resultsToJson(const LlamaBenchResults& results);
    static std::string formatMetricsTable(const std::vector<BenchRunResult>& results);
    static std::string generateAnalysisPrompt(const std::string& context);
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Фаза 1 завершена  
**Время:** 2-3 часа

---

### Задача 4.2: RAG-интеграция для анализа

**Описание:**
- Загрузка результатов как контекста
- Специальный промпт для анализа производительности
- Интеграция с существующим RagManager

**Файлы для создания:**
```
include/bench/bench_rag_analyzer.h
src/bench/bench_rag_analyzer.cpp
```

**Содержание `bench_rag_analyzer.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include "llama_bench_results.h"
#include <string>
#include <memory>
#include <functional>

namespace llama_gui {
namespace core {
    class RagManager;
}

namespace llama_gui {
namespace bench {

struct AnalysisRequest {
    LlamaBenchResults results;
    std::string user_question;  // Например: "Какой профиль лучше для inference?"
    int max_context_tokens = 4000;
    bool include_recommendations = true;
};

struct AnalysisResult {
    std::string summary;
    std::string detailed_analysis;
    std::vector<std::string> recommendations;
    std::string best_profile;
    std::string reasoning;
    double confidence_score;  // 0.0 - 1.0
};

/**
 * @class BenchRagAnalyzer
 * @brief Анализ результатов бенчмарка с помощью RAG
 */
class BenchRagAnalyzer {
public:
    using AnalysisCallback = std::function<void(const AnalysisResult&)>;
    using StreamingCallback = std::function<void(const std::string& chunk)>;

    explicit BenchRagAnalyzer(core::RagManager& rag_manager);
    ~BenchRagAnalyzer();

    // Инициализация
    bool initialize();
    
    // Анализ
    AnalysisResult analyze(const AnalysisRequest& request);
    void analyzeAsync(const AnalysisRequest& request, AnalysisCallback callback);
    
    // Стриминг ответа
    void analyzeStreaming(const AnalysisRequest& request, StreamingCallback callback);
    
    // Отмена
    void cancelAnalysis();
    bool isAnalyzing() const;
    
    // Промпты
    static std::string buildAnalysisPrompt(const AnalysisRequest& request);
    static std::string buildComparisonPrompt(const BenchComparisonResult& comparison);
    
    // Контекст
    std::string buildRagContext(const LlamaBenchResults& results) const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    std::string generateSummary(const AnalysisRequest& request);
    std::vector<std::string> generateRecommendations(const AnalysisRequest& request);
    std::string extractBestProfile(const AnalysisRequest& request);
    double calculateConfidence(const AnalysisResult& result);
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Задача 4.1, доступ к RagManager  
**Время:** 5-6 часов

---

### Задача 4.3: UI для анализа моделью

**Описание:**
- Кнопка "Анализ моделью" в диалоге
- Окно просмотра анализа
- Стриминг ответа

**Файлы для создания:**
```
include/ui/bench_analysis_dialog.h
src/ui/bench_analysis_dialog.cpp
```

**Содержание `bench_analysis_dialog.h`:**
```cpp
#pragma once

#include "../external/imgui/imgui.h"
#include "../bench/bench_rag_analyzer.h"
#include <string>
#include <memory>

namespace llama_gui {
namespace ui {

/**
 * @class BenchAnalysisDialog
 * @brief Диалог для анализа результатов бенчмарка моделью
 */
class BenchAnalysisDialog {
public:
    BenchAnalysisDialog();
    ~BenchAnalysisDialog();

    // Управление
    void setVisible(bool visible);
    bool isVisible() const;
    
    // Рендеринг
    void render();
    
    // Запуск анализа
    void startAnalysis(const llama_gui::bench::LlamaBenchResults& results,
                      const std::string& user_question = "");
    
    // Прерывание
    void cancelAnalysis();
    
    // Сброс
    void reset();

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    void renderHeader();
    void renderQuestionInput();
    void renderAnalysisProgress();
    void renderAnalysisResult();
    void renderRecommendations();
    void renderActionButtons();
    
    void onAnalysisComplete(const llama_gui::bench::AnalysisResult& result);
    void onAnalysisChunk(const std::string& chunk);
};

} // namespace ui
} // namespace llama_gui
```

**Зависимости:** Задачи 2.1, 4.2  
**Время:** 4-5 часов

---

### Задача 4.4: Специальные промпты для анализа

**Описание:**
- Промпт для анализа производительности
- Промпт для сравнения профилей
- Промпт для рекомендаций

**Файлы для создания:**
```
include/bench/bench_prompts.h
src/bench/bench_prompts.cpp
```

**Содержание `bench_prompts.h`:**
```cpp
#pragma once

#include <string>
#include <vector>

namespace llama_gui {
namespace bench {

/**
 * @namespace BenchPrompts
 * @brief Промпты для анализа результатов бенчмарка
 */
namespace BenchPrompts {

// Основной промпт для анализа
std::string getAnalysisPrompt();

// Промпт для сравнения профилей
std::string getComparisonPrompt();

// Промпт для рекомендаций
std::string getRecommendationsPrompt();

// Промпт для summary
std::string getSummaryPrompt();

// Системный промпт для эксперта по производительности
std::string getSystemPrompt();

// Шаблон для контекста с результатами
std::string buildResultsContextTemplate(const std::string& results_json);

// Переменные для подстановки
namespace Variables {
    constexpr const char* RESULTS_JSON = "{{RESULTS_JSON}}";
    constexpr const char* USER_QUESTION = "{{USER_QUESTION}}";
    constexpr const char* PROFILE_NAMES = "{{PROFILE_NAMES}}";
    constexpr const char* METRICS_TABLE = "{{METRICS_TABLE}}";
    constexpr const char* BEST_PROFILE = "{{BEST_PROFILE}}";
}

} // namespace BenchPrompts

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Задача 4.2  
**Время:** 2-3 часа

---

## ФАЗА 5: Хранение данных

**Цель:** Организация хранения результатов и истории.

**Время реализации:** 1-2 дня

### Задача 5.1: Менеджер хранилища результатов

**Описание:**
- Структура папок bench_results/
- history.json для истории запусков
- Автоматическая очистка старых файлов

**Файлы для создания:**
```
include/bench/bench_storage.h
src/bench/bench_storage.cpp
```

**Содержание `bench_storage.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include "llama_bench_results.h"
#include <string>
#include <vector>
#include <chrono>

namespace llama_gui {
namespace bench {

struct StorageStats {
    size_t total_files;
    size_t total_size_bytes;
    size_t history_entries;
    std::chrono::system_clock::time_point oldest_result;
    std::chrono::system_clock::time_point newest_result;
};

/**
 * @class BenchStorage
 * @brief Управление хранилищем результатов бенчмарка
 */
class BenchStorage {
public:
    BenchStorage();
    explicit BenchStorage(const std::string& base_directory);
    ~BenchStorage();

    // Инициализация
    bool initialize(const std::string& base_directory);
    void shutdown();
    
    // Пути
    std::string getBaseDirectory() const;
    std::string getResultsDirectory() const;
    std::string getHistoryFilePath() const;
    std::string generateResultFilePath(const std::string& comparison_id) const;
    
    // Сохранение
    bool saveResult(const LlamaBenchResults& results);
    bool saveToHistory(const BenchComparisonResult& comparison);
    bool exportResult(const LlamaBenchResults& results, OutputFormat format);
    
    // Загрузка
    LlamaBenchResults loadResult(const std::string& result_id) const;
    LlamaBenchResults loadHistory() const;
    
    // Управление
    void cleanupOldResults(int days_to_keep = 30);
    void clearAll();
    
    // Статистика
    StorageStats getStats() const;
    std::vector<std::string> listResultFiles() const;

private:
    struct Impl;
    std::unique_ptr<Impl> pimpl_;
    
    bool createDirectoryStructure();
    std::string generateTimestamp() const;
    std::string sanitizeFileName(const std::string& name) const;
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Фаза 1 завершена  
**Время:** 3-4 часа

---

### Задача 5.2: Экспорт в CSV/Markdown

**Описание:**
- Генерация CSV файлов для импорта в таблицы
- Генерация Markdown отчётов
- Шаблоны отчётов

**Файлы для создания:**
```
include/bench/bench_report_generator.h
src/bench/bench_report_generator.cpp
```

**Содержание `bench_report_generator.h`:**
```cpp
#pragma once

#include "bench_types.h"
#include "llama_bench_results.h"
#include <string>
#include <vector>

namespace llama_gui {
namespace bench {

struct ReportOptions {
    bool include_summary = true;
    bool include_detailed_results = true;
    bool include_comparison_table = true;
    bool include_recommendations = false;
    bool include_charts_data = false;  // Данные для графиков
    std::string title;
    std::string author;
    std::string notes;
};

/**
 * @class BenchReportGenerator
 * @brief Генерация отчётов по результатам бенчмарка
 */
class BenchReportGenerator {
public:
    // Генерация Markdown отчёта
    static std::string generateMarkdownReport(const LlamaBenchResults& results,
                                             const ReportOptions& options = ReportOptions());
    
    // Генерация CSV
    static std::string generateCsv(const std::vector<BenchRunResult>& results);
    
    // Генерация HTML отчёта (опционально)
    static std::string generateHtmlReport(const LlamaBenchResults& results,
                                         const ReportOptions& options = ReportOptions());
    
    // Сохранение в файл
    static bool saveReport(const std::string& content,
                          const std::string& file_path);
    
    // Шаблоны
    static std::string getDefaultMarkdownTemplate();
    static std::string getCsvTemplate();

private:
    // Вспомогательные методы
    static std::string buildSummarySection(const LlamaBenchResults& results);
    static std::string buildComparisonTable(const std::vector<BenchRunResult>& results);
    static std::string buildDetailedResults(const std::vector<BenchRunResult>& results);
    static std::string buildRecommendationsSection(const std::vector<std::string>& recommendations);
    static std::string formatTimestamp(std::chrono::system_clock::time_point tp);
};

} // namespace bench
} // namespace llama_gui
```

**Зависимости:** Задача 5.1  
**Время:** 2-3 часа

---

## ФАЗА 6: Тестирование и документация

**Цель:** Обеспечить качество кода и документировать функциональность.

**Время реализации:** 2-3 дня

### Задача 6.1: Unit тесты

**Описание:**
- Тесты для парсера
- Тесты для конвертации профилей
- Тесты для экспорта

**Файлы для создания:**
```
tests/bench/test_bench_parser.cpp
tests/bench/test_profile_adapter.cpp
tests/bench/test_bench_exporter.cpp
```

**Пример `test_bench_parser.cpp`:**
```cpp
#include <gtest/gtest.h>
#include "bench/llama_bench_parser.h"
#include "bench/bench_types.h"

using namespace llama_gui::bench;

class BenchParserTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(BenchParserTest, ParseJsonOutput) {
    std::string json_input = R"({
        "model": "test-model",
        "prompt_tokens_per_sec": 45.2,
        "generation_tokens_per_sec": 32.1
    })";
    
    auto results = LlamaBenchParser::parseJson(json_input);
    
    EXPECT_EQ(results.size(), 1);
    EXPECT_NEAR(results[0].prompt_tokens_per_sec, 45.2, 0.01);
    EXPECT_NEAR(results[0].gen_tokens_per_sec, 32.1, 0.01);
}

TEST_F(BenchParserTest, ParseCsvOutput) {
    std::string csv_input = "model,prompt_tps,gen_tps\ntest-model,45.2,32.1";
    
    auto results = LlamaBenchParser::parseCsv(csv_input);
    
    EXPECT_EQ(results.size(), 1);
    EXPECT_NEAR(results[0].prompt_tokens_per_sec, 45.2, 0.01);
}

TEST_F(BenchParserTest, ValidateResult) {
    BenchRunResult result;
    result.prompt_tokens_per_sec = 45.2;
    result.gen_tokens_per_sec = 32.1;
    result.status = BenchStatus::Completed;
    
    EXPECT_TRUE(LlamaBenchParser::validateResult(result));
    
    result.prompt_tokens_per_sec = -1.0;
    EXPECT_FALSE(LlamaBenchParser::validateResult(result));
}
```

**Зависимости:** Фаза 1 завершена  
**Время:** 3-4 часа

---

### Задача 6.2: Интеграционные тесты

**Описание:**
- Тест запуска llama-bench (mock)
- Тест полного цикла
- Тест интеграции с UI

**Файлы для создания:**
```
tests/bench/test_bench_integration.cpp
tests/bench/test_bench_module.cpp
```

**Зависимости:** Задача 6.1  
**Время:** 3-4 часа

---

### Задача 6.3: Документация API

**Описание:**
- Doxygen комментарии в заголовочных файлах
- README для модуля
- Примеры использования

**Файлы для создания:**
```
docs/bench/README.md
docs/bench/API.md
docs/bench/USAGE.md
```

**Содержание `docs/bench/README.md`:**
```markdown
# Llama Bench Integration

## Обзор

Модуль интеграции llama-bench для llama-gui предоставляет возможность:
- Запуска бенчмарков производительности
- Сравнения профилей конфигурации
- Анализа результатов с помощью RAG

## Быстрый старт

### Включение в сборку

```bash
cmake -DENABLE_LLAMA_BENCH=ON ..
```

### Базовое использование

```cpp
#include "bench/llama_bench_module.h"

using namespace llama_gui::bench;

// Инициализация
LlamaBenchModule module;
module.initialize("/path/to/llama-bench", "bench_results");

// Запуск бенчмарка
BenchTestParams params;
params.n_prompt = 512;
params.n_gen = 128;
params.repetitions = 5;

module.runBenchmark("profiles/my_profile.json", params);

// Получение результатов
const auto& results = module.getResults();
```

## Архитектура

```
┌─────────────────────────────────────────────────────┐
│                 LlamaBenchModule                    │
│  (Единая точка входа, фасад)                        │
├─────────────────────────────────────────────────────┤
│  LlamaBenchRunner  │  LlamaBenchParser              │
│  (Запуск теста)    │  (Парсинг вывода)              │
├─────────────────────────────────────────────────────┤
│              LlamaBenchResults                      │
│         (Управление результатами)                   │
├─────────────────────────────────────────────────────┤
│  ProfileAdapter    │  BenchRagAnalyzer              │
│  (Конвертация)     │  (RAG анализ)                  │
└─────────────────────────────────────────────────────┘
```

## Документация

- [API Reference](API.md) - Полная документация API
- [Usage Guide](USAGE.md) - Руководство по использованию
- [Architecture](ARCHITECTURE.md) - Архитектурное описание

## Конфигурация

### Параметры бенчмарка

| Параметр | По умолчанию | Описание |
|----------|--------------|----------|
| n_prompt | 512 | Количество токенов промпта |
| n_gen | 128 | Количество генерируемых токенов |
| repetitions | 5 | Количество повторений |
| batch_size | 2048 | Размер батча |

## Экспорт результатов

Поддерживаемые форматы:
- JSON (для RAG анализа)
- CSV (для импорта в таблицы)
- Markdown (для отчётов)

## RAG Интеграция

Модуль автоматически интегрируется с существующим RagManager для анализа результатов.

## Лицензия

[Указать лицензию]
```

**Зависимости:** Все предыдущие задачи  
**Время:** 4-5 часов

---

### Задача 6.4: Документация пользователя

**Описание:**
- Руководство по использованию UI
- Примеры сценариев
- FAQ

**Файлы для создания:**
```
docs/bench/USER_GUIDE.md
docs/bench/FAQ.md
docs/bench/EXAMPLES.md
```

**Зависимости:** Фаза 2 завершена  
**Время:** 2-3 часа

---

## Сводная таблица задач

| Фаза | Задача | Название | Время | Зависимости |
|------|--------|----------|-------|-------------|
| 1 | 1.1 | Структура и типы | 2-3 ч | - |
| 1 | 1.2 | LlamaBenchRunner | 4-5 ч | 1.1 |
| 1 | 1.3 | LlamaBenchParser | 3-4 ч | 1.1 |
| 1 | 1.4 | LlamaBenchResults | 4-5 ч | 1.1, 1.3 |
| 1 | 1.5 | LlamaBenchModule | 3-4 ч | 1.2, 1.3, 1.4 |
| 1 | 1.6 | CMake интеграция | 1 ч | - |
| 2 | 2.1 | LlamaBenchDialog | 6-8 ч | Фаза 1 |
| 2 | 2.2 | Profile Selector | 3-4 ч | 2.1 |
| 2 | 2.3 | Params Editor | 3-4 ч | 2.1 |
| 2 | 2.4 | Results Table | 4-5 ч | 2.1 |
| 2 | 2.5 | Progress Indicator | 2-3 ч | 2.1 |
| 2 | 2.6 | MainWindow интеграция | 2-3 ч | 2.1 |
| 3 | 3.1 | Profile Adapter | 3-4 ч | Фаза 1 |
| 3 | 3.2 | Profile Manager | 3-4 ч | 3.1 |
| 3 | 3.3 | Profile Watcher | 2-3 ч | 3.2 |
| 4 | 4.1 | Bench Exporter | 2-3 ч | Фаза 1 |
| 4 | 4.2 | RAG Analyzer | 5-6 ч | 4.1 |
| 4 | 4.3 | Analysis Dialog | 4-5 ч | 2.1, 4.2 |
| 4 | 4.4 | Bench Prompts | 2-3 ч | 4.2 |
| 5 | 5.1 | Bench Storage | 3-4 ч | Фаза 1 |
| 5 | 5.2 | Report Generator | 2-3 ч | 5.1 |
| 6 | 6.1 | Unit тесты | 3-4 ч | Фаза 1 |
| 6 | 6.2 | Integration тесты | 3-4 ч | 6.1 |
| 6 | 6.3 | API документация | 4-5 ч | Все |
| 6 | 6.4 | User документация | 2-3 ч | Фаза 2 |

---

## Итоговая оценка

| Категория | Время (часы) | Время (дни*) |
|-----------|--------------|--------------|
| Фаза 1: Инфраструктура | 17-22 | 2-3 |
| Фаза 2: UI модуль | 20-27 | 3-4 |
| Фаза 3: Интеграция профилей | 8-11 | 1-2 |
| Фаза 4: RAG анализ | 13-17 | 2-3 |
| Фаза 5: Хранение | 5-7 | 1 |
| Фаза 6: Тестирование и docs | 12-16 | 2 |
| **ИТОГО** | **75-100** | **11-15** |

*При 8-часовом рабочем дне

---

## Критический путь

```
1.1 → 1.2 → 1.5 → 2.1 → 2.6  (Базовая функциональность)
              ↓
           1.3 → 1.4  (Парсинг и хранение)
              ↓
           3.1 → 3.2  (Интеграция профилей)
              ↓
           4.1 → 4.2 → 4.3  (RAG анализ)
```

**Минимальный набор для MVP:**
1. Задачи 1.1-1.6 (базовая инфраструктура)
2. Задача 2.1 + 2.6 (минимальный UI)
3. Задача 3.1 (интеграция профилей)

**Время MVP:** ~35-45 часов (5-6 дней)

---

## Риски и.mitigation

| Риск | Вероятность | Влияние | Mitigation |
|------|-------------|---------|------------|
| Изменения в формате вывода llama-bench | Средняя | Высокое | Поддержка нескольких форматов, адаптивный парсер |
| Проблемы с производительностью при большом числе профилей | Низкая | Среднее | Кэширование, ленивая загрузка |
| Сложности интеграции с RAG | Средняя | Высокое | Тесная координация с существующим кодом RAG |
| Проблемы с асинхронным выполнением | Средняя | Среднее | Тщательное тестирование, fallback на sync режим |
| Несовместимость версий llama-bench | Низкая | Высокое | Проверка версии, graceful degradation |

---

## Контрольные точки

### После Фазы 1 (День 3)
- [ ] Компиляция без ошибок
- [ ] Unit тесты парсера проходят
- [ ] Запуск llama-bench из кода работает

### После Фазы 2 (День 7)
- [ ] Диалог открывается из меню
- [ ] Выбор профилей работает
- [ ] Таблица результатов отображает данные

### После Фазы 3 (День 9)
- [ ] Профили загружаются из profiles/
- [ ] Параметры корректно маппятся
- [ ] Изменения профилей отслеживаются

### После Фазы 4 (День 12)
- [ ] Экспорт в JSON работает
- [ ] RAG анализ выдаёт результаты
- [ ] Стриминг ответа работает

### После Фазы 5 (День 13)
- [ ] Результаты сохраняются в bench_results/
- [ ] History.json обновляется
- [ ] Экспорт в CSV/Markdown работает

### После Фазы 6 (День 15)
- [ ] Все тесты проходят
- [ ] Документация полная
- [ ] Code review завершён

---

## Рекомендации по реализации

1. **Начать с MVP:** Реализовать задачи 1.1-1.6, 2.1, 2.6, 3.1 для базовой функциональности
2. **Итеративная разработка:** После каждой фазы тестировать на реальном проекте
3. **Mock для тестов:** Создать mock llama-bench для unit тестов
4. **Логирование:** Добавить детальное логирование для отладки
5. **Обработка ошибок:** Graceful degradation при ошибках llama-bench

---

## Приложения

### A. Структура файлов проекта

```
llama-gui/
├── include/
│   └── bench/
│       ├── bench_types.h
│       ├── bench_common.h
│       ├── llama_bench_runner.h
│       ├── llama_bench_parser.h
│       ├── llama_bench_results.h
│       ├── llama_bench_module.h
│       ├── profile_adapter.h
│       ├── bench_profile_manager.h
│       ├── profile_watcher.h
│       ├── bench_exporter.h
│       ├── bench_rag_analyzer.h
│       ├── bench_storage.h
│       ├── bench_report_generator.h
│       └── bench_prompts.h
├── src/
│   └── bench/
│       ├── llama_bench_runner.cpp
│       ├── llama_bench_parser.cpp
│       ├── llama_bench_results.cpp
│       ├── llama_bench_module.cpp
│       ├── profile_adapter.cpp
│       ├── bench_profile_manager.cpp
│       ├── profile_watcher.cpp
│       ├── bench_exporter.cpp
│       ├── bench_rag_analyzer.cpp
│       ├── bench_storage.cpp
│       ├── bench_report_generator.cpp
│       └── bench_prompts.cpp
├── include/ui/
│   ├── llama_bench_dialog.h
│   ├── bench_profile_selector.h
│   ├── bench_test_params_editor.h
│   ├── bench_results_table.h
│   ├── bench_progress_indicator.h
│   └── bench_analysis_dialog.h
├── src/ui/
│   ├── llama_bench_dialog.cpp
│   ├── bench_profile_selector.cpp
│   ├── bench_test_params_editor.cpp
│   ├── bench_results_table.cpp
│   ├── bench_progress_indicator.cpp
│   └── bench_analysis_dialog.cpp
├── tests/bench/
│   ├── test_bench_parser.cpp
│   ├── test_profile_adapter.cpp
│   ├── test_bench_exporter.cpp
│   ├── test_bench_integration.cpp
│   └── test_bench_module.cpp
├── bench_results/
│   ├── history.json
│   └── [timestamp]_results.json
└── docs/bench/
    ├── README.md
    ├── API.md
    ├── USAGE.md
    ├── USER_GUIDE.md
    ├── FAQ.md
    └── EXAMPLES.md
```

### B. Пример использования

```cpp
#include "bench/llama_bench_module.h"
#include "ui/llama_bench_dialog.h"

using namespace llama_gui;

// В main.cpp или инициализации
bench::LlamaBenchModule benchModule;

if (benchModule.initialize("llama-bench", "bench_results")) {
    // Настройка параметров
    bench::BenchTestParams params;
    params.n_prompt = 512;
    params.n_gen = 128;
    params.repetitions = 5;
    
    // Запуск сравнения профилей
    std::vector<std::string> profiles = {
        "profiles/default.json",
        "profiles/optimized.json"
    };
    
    benchModule.runComparison(profiles, params);
    
    // Получение результатов
    const auto& results = benchModule.getResults();
    
    // Экспорт
    results.exportToFile("bench_results/comparison.json", 
                        bench::OutputFormat::JSON);
}

// UI интеграция
ui::LlamaBenchDialog benchDialog;
benchDialog.initialize("llama-bench");
// В главном цикле: benchDialog.render();
```

---

**Документ создан:** 25 марта 2026 г.  
**Автор:** AI Project Planner  
**Статус:** Готов к реализации
