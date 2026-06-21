# Сводная таблица изменений OpenRouter RAG Deep Analysis

## Обзор файлов для изменения

| Файл | Строк изменено | Строк добавлено | Сложность |
|------|----------------|-----------------|-----------|
| `include/core/rag_manager.h` | 0 | 25 | ⭐ |
| `src/core/rag_manager_deep_analysis.cpp` | 50 | 350 | ⭐⭐⭐ |
| `src/core/openrouter_client.cpp` | 30 | 40 | ⭐⭐ |
| `src/core/rag_manager_model_selection.cpp` | 0 | 0 | ✅ Готов |
| **Итого** | **80** | **415** | |

---

## Детальная карта изменений

### 1. include/core/rag_manager.h

| Строка | Тип | Описание |
|--------|-----|----------|
| ~165 | Добавить | `check_openrouter_availability()` |
| ~170 | Добавить | `check_openrouter_rate_limit()` |
| ~176 | Добавить | `generate_chunk_summary_with_fallback()` |
| ~182 | Добавить | `generate_chunk_summary_local()` |

**Статус:** ⏳ Требуется ручное редактирование

---

### 2. src/core/rag_manager_deep_analysis.cpp

| Строка | Тип | Описание | Патч |
|--------|-----|----------|------|
| ~240 | Изменить | `process_deep_analysis_mapreduce()` - таймаут 120с | `mapreduce_timeout_patch.cpp` |
| ~530 | Заменить | `generate_chunk_summary()` → fallback | - |
| ~530 | Добавить | `check_openrouter_availability()` | `rag_manager_fallback_methods.cpp` |
| ~550 | Добавить | `check_openrouter_rate_limit()` | `rag_manager_fallback_methods.cpp` |
| ~580 | Добавить | `generate_chunk_summary_with_fallback()` | `rag_manager_fallback_methods.cpp` |
| ~700 | Добавить | `generate_chunk_summary_local()` | `rag_manager_fallback_methods.cpp` |
| ~630 | Изменить | `generate_chunk_summary_openrouter()` - выбор модели Map | `map_model_selection_patch.cpp` |
| ~860 | Изменить | `synthesize_final_answer_openrouter()` - таймаут 180с | `reduce_timeout_patch.cpp` |
| ~870 | Изменить | `synthesize_final_answer_openrouter()` - выбор модели Reduce | `reduce_model_selection_patch.cpp` |

**Статус:** ⏳ Требуется ручное редактирование  
**Общий объём:** ~350 строк кода

---

### 3. src/core/openrouter_client.cpp

| Строка | Тип | Описание | Патч |
|--------|-----|----------|------|
| ~710 | Заменить | `is_api_available()` - улучшенная проверка | `openrouter_client_improvements.cpp` |
| ~760 | Изменить | `get_rate_limit()` - улучшенный парсинг | `openrouter_client_improvements.cpp` |

**Статус:** ⏳ Требуется ручное редактирование  
**Общий объём:** ~40 строк кода

---

### 4. src/core/rag_manager_model_selection.cpp

| Строка | Тип | Описание | Статус |
|--------|-----|----------|--------|
| - | - | Метод `select_model_for_stage()` уже готов | ✅ Без изменений |

---

## Матрица зависимостей

```
include/core/rag_manager.h (объявления методов)
    ↓
src/core/rag_manager_deep_analysis.cpp (реализация)
    ↓
    ├─→ openrouter_client::is_api_available()
    ├─→ openrouter_client::get_rate_limit()
    ├─→ select_model_for_stage() (из model_selection.cpp)
    └─→ llama_interface_ (локальный сервер)
```

---

## Последовательность применения

### Этап 1: Подготовка
1. ✅ Создать резервные копии файлов
2. ✅ Изучить `IMPROVEMENTS_PLAN.md`
3. ✅ Открыть `APPLY_INSTRUCTIONS.md`

### Этап 2: Изменение заголовков
1. ⏳ Обновить `include/core/rag_manager.h`
2. ⏳ Добавить 4 объявления методов

### Этап 3: Обновление клиента OpenRouter
1. ⏳ Улучшить `is_api_available()` в `openrouter_client.cpp`
2. ⏳ Улучшить `get_rate_limit()` в `openrouter_client.cpp`

### Этап 4: Основная реализация
1. ⏳ Добавить методы проверки API и лимитов
2. ⏳ Добавить методы fallback
3. ⏳ Изменить `generate_chunk_summary()`
4. ⏳ Добавить таймауты в MapReduce
5. ⏳ Добавить выбор моделей для Map/Reduce

### Этап 5: Тестирование
1. ⏳ Сборка проекта
2. ⏳ Тест нормальной работы
3. ⏳ Тест fallback при ошибке API
4. ⏳ Тест fallback при лимите
5. ⏳ Тест retry при таймауте

---

## Проверка готовности

### Чеклист перед сборкой

- [ ] Все методы объявлены в `.h` файле
- [ ] Все методы реализованы в `.cpp` файле
- [ ] `#include <thread>` присутствует
- [ ] `#include <chrono>` присутствует
- [ ] Нет синтаксических ошибок
- [ ] RAII класс `TimeoutRestorer` внутри методов
- [ ] `DeepAnalysisSettings` используется корректно

### Чеклист после сборки

- [ ] Проект компилируется без ошибок
- [ ] Нет предупреждений компилятора
- [ ] Приложение запускается
- [ ] GUI отображается корректно

### Чеклист после тестирования

- [ ] Проверка API работает (лог `[OPENROUTER CHECK]`)
- [ ] Проверка лимитов работает (лог `[RATE LIMIT CHECK]`)
- [ ] Таймауты увеличены (лог `[MAP-REDUCE] Timeout set to`)
- [ ] Модели выбираются (лог `[MODEL SELECT]`)
- [ ] Фактическая модель логируется (лог `(model: ...)`)
- [ ] Fallback работает (лог `[FALLBACK]`)
- [ ] Retry работает (лог `[FALLBACK] Timeout, retrying`)

---

## Статус улучшений

| Улучшение | Файл | Статус |
|-----------|------|--------|
| 1. Проверка API | `rag_manager_deep_analysis.cpp` | ⏳ |
| 2. Проверка лимитов | `rag_manager_deep_analysis.cpp` | ⏳ |
| 3. Таймаут 120с Map | `rag_manager_deep_analysis.cpp` | ⏳ |
| 4. Таймаут 180с Reduce | `rag_manager_deep_analysis.cpp` | ⏳ |
| 5. Логирование модели | `rag_manager_deep_analysis.cpp` | ✅ Уже есть |
| 6. Fallback на локальный | `rag_manager_deep_analysis.cpp` | ⏳ |
| 7. Разные модели Map/Reduce | `rag_manager_deep_analysis.cpp` | ⏳ |
| 8. Retry при таймауте | `rag_manager_deep_analysis.cpp` | ⏳ |
| 9. Улучшение is_api_available | `openrouter_client.cpp` | ⏳ |
| 10. Улучшение get_rate_limit | `openrouter_client.cpp` | ⏳ |

---

## Созданные файлы

| Файл | Назначение |
|------|------------|
| `IMPROVEMENTS_PLAN.md` | Полный план с обоснованием |
| `APPLY_INSTRUCTIONS.md` | Пошаговая инструкция |
| `CHANGES_SUMMARY.md` | Этот файл |
| `patches/rag_manager_header_patch.h` | Изменения заголовка |
| `patches/rag_manager_fallback_methods.cpp` | Методы fallback |
| `patches/mapreduce_timeout_patch.cpp` | Таймаут для Map |
| `patches/reduce_timeout_patch.cpp` | Таймаут для Reduce |
| `patches/map_model_selection_patch.cpp` | Выбор модели Map |
| `patches/reduce_model_selection_patch.cpp` | Выбор модели Reduce |
| `patches/openrouter_client_improvements.cpp` | Улучшения клиента |

---

## Метрики успеха

### До улучшений

| Метрика | Значение |
|---------|----------|
| Таймаут | 30 секунд |
| Проверка API | ❌ Нет |
| Проверка лимитов | ❌ Нет |
| Логирование модели | ❌ Нет |
| Fallback | ❌ Нет |
| Разные модели Map/Reduce | ❌ Нет |
| Retry при таймауте | ❌ Нет |

### После улучшений

| Метрика | Значение |
|---------|----------|
| Таймаут Map | ✅ 120 секунд |
| Таймаут Reduce | ✅ 180 секунд |
| Проверка API | ✅ Есть |
| Проверка лимитов | ✅ Есть |
| Логирование модели | ✅ Есть |
| Fallback | ✅ Есть |
| Разные модели Map/Reduce | ✅ Есть |
| Retry при таймауте | ✅ 1 попытка |

---

## Контакты и поддержка

Документация:
- `IMPROVEMENTS_PLAN.md` - Детальное описание
- `APPLY_INSTRUCTIONS.md` - Инструкция по применению

Патчи:
- `patches/*.cpp` - Готовый код для вставки
- `patches/*.h` - Изменения заголовков

Резервные копии (создать перед применением):
- `include/core/rag_manager.h.bak`
- `src/core/rag_manager_deep_analysis.cpp.bak`
- `src/core/openrouter_client.cpp.bak`
