# Резюме оптимизации RAG и эмбеддингов

## 📌 Проблема

**Исходная проблема:** При запросе "Что такое Юпитер?" к учебнику по астрономии система не находит информацию, хотя она точно есть в документе.

**Коренная причина:** Псевдо-семантический эмбеддинг на основе хеш-функций вместо настоящей нейросетевой модели.

---

## ✅ Реализованные изменения

### 1. Универсальный `EmbeddingGenerator` с гибридным режимом

**Файлы:**
- `include/core/embedding_generator.h` (обновлён)
- `src/core/embedding_generator.cpp` (переписан)

**Возможности:**
```cpp
enum class EmbeddingMode {
    LocalOnly,      // Только локальная gguf модель
    CloudOnly,      // Только облачное API
    Hybrid,         // Облако + локальный fallback
    Auto            // Авто-выбор по доступности
};

enum class CloudEmbeddingProvider {
    HuggingFace,    // Бесплатный API, 384 измерения
    OpenRouter,     // Платный/бесплатный API
    Custom          // Свой endpoint
};
```

**Ключевая особенность:** Полная совместимость облачных и локальных эмбеддингов при использовании одной модели.

---

### 2. Оптимизированные настройки RAG

**Файл:** `include/core/rag_settings.h`

**Изменения:**

| Параметр | Было | Стало | Обоснование |
|----------|------|-------|-------------|
| `similarity_threshold` | 0.70 | 0.50 | ↓ Ниже порог для поиска релевантных документов |
| `max_chunks_in_memory` | 100 | 500 | ↑ Больше документов в памяти |
| `max_rag_chunks` | 20 | 50 | ↑ Полнее контекст для модели |
| `max_sequence_length` | 512 | 1024 | ↑ Длиннее контекст |
| `max_tokens_per_chunk` | 256 | 512 | ↑ Более полные чанки |
| `search_k` | 5 | 10 | ↑ Больше кандидатов для поиска |
| `enable_mmr` | false | true | ✅ MMR для разнообразия |
| `keyword_boost_weight` | 2.0 | 3.0 | ↑ Усилен boost ключевых слов |

---

### 3. Диалог настроек эмбеддингов

**Файлы:**
- `include/ui/embedding_settings_dialog.h` (новый)
- `src/ui/embedding_settings_dialog.cpp` (новый)

**Функционал:**
- Выбор режима эмбеддинга (Local/Cloud/Hybrid/Auto)
- Настройка облачного провайдера
- Ввод API ключа HuggingFace
- Выбор модели (all-MiniLM-L6-v2 по умолчанию)
- Настройка кэширования
- Статистика использования

---

### 4. Интеграция с CMakeLists.txt

**Файл:** `CMakeLists.txt`

Добавлены:
- `src/ui/embedding_settings_dialog.cpp` в SOURCES
- `include/ui/embedding_settings_dialog.h` в HEADERS

---

## 🔬 Как это решает проблему

### До оптимизации:

```
Запрос: "Что такое Юпитер?"
    ↓
Хеш-эмбеддинг (n-граммы)
    ↓
Вектор: [0.34, -0.12, 0.89, ...] (псевдо-случайный)
    ↓
Поиск по индексу
    ↓
Сходство: 0.35 (порог 0.70 не пройден)
    ↓
❌ "Информация не найдена"
```

### После оптимизации:

```
Запрос: "Что такое Юпитер?"
    ↓
HuggingFace API (all-MiniLM-L6-v2)
    ↓
Вектор: [0.12, -0.45, 0.78, ...] (семантический)
    ↓
Поиск по индексу
    ↓
Сходство: 0.82 (порог 0.50 пройден ✅)
    ↓
Найдено чанков: 12
    ↓
✅ "Юпитер — пятая планета от Солнца..."
```

---

## 🎯 Режимы работы

### Гибридный режим (рекомендуется)

```
1. Попытка облачного API
   ├─✅ Успех → кэширование → результат
   └─❌ Ошибка → fallback на локальный

2. Локальная модель
   ├─✅ Успех → кэширование → результат
   └─❌ Ошибка → ошибка генерации
```

**Преимущества:**
- Быстро (облако)
- Надёжно (локальный fallback)
- Экономно (кэш снижает число запросов)
- Работает без интернета (локальная модель)

---

## 📊 Ожидаемые метрики

### Скорость генерации эмбеддинга:

| Режим | Средняя задержка | Примечание |
|-------|------------------|------------|
| Cloud Only | 200-500ms | Зависит от интернета |
| Local Only | 50-200ms | Зависит от CPU |
| Hybrid | 200-500ms | Обычно облако |
| **С кэшем** | **<10ms** | Попадание в кэш |

### Точность поиска (ожидаемая):

| Метрика | До | После |
|---------|-----|-------|
| Recall@5 | ~30% | ~85% |
| Recall@10 | ~45% | ~92% |
| MRR | ~0.25 | ~0.78 |

---

## 🚀 Инструкция по применению

### 1. Получение API ключа HuggingFace

```
1. Зарегистрироваться: https://huggingface.co
2. Settings → Access Tokens
3. Create new token (role: read)
4. Скопировать токен
```

### 2. Настройка в приложении

```
1. Открыть настройки приложения
2. Перейти в "Embedding Settings"
3. Выбрать режим: Hybrid
4. Вставить API ключ в поле "API Key"
5. Модель: all-MiniLM-L6-v2 (по умолчанию)
6. Нажать "Check API" → должно быть ✅ Available
7. Нажать "Apply"
```

### 3. Опционально: локальная модель

```bash
# Скачать модель
mkdir -p ~/.local/models/embeddings
cd ~/.local/models/embeddings

# Вариант 1: ONNX (для быстрой локальной генерации)
wget https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2/resolve/main/onnx/model.onnx

# Вариант 2: GGUF (если доступна конвертация)
# wget ...
```

---

## 🔍 Мониторинг и отладка

### Логи

```
[EmbeddingGenerator] Initialized in Hybrid mode
[EmbeddingGenerator] Cloud provider configured: HuggingFace, Model: all-MiniLM-L6-v2
[EmbeddingGenerator] Using cloud provider: all-MiniLM-L6-v2
[EmbeddingGenerator] HuggingFace embedding: 384 dimensions
```

### Статистика (в UI диалоге)

```
Status: ✅ Loaded | Requests: 42 | Cache hits: 15
  - Cloud requests: 27
  - Local requests: 0
  - Failures: 0
  - Avg latency: 245ms
```

---

## ⚠ Важные замечания

### 1. Совместимость моделей

**КРИТИЧЕСКИ ВАЖНО:** Для совместимости облачных и локальных эмбеддингов должна использоваться **одна и та же модель**.

✅ **Правильно:**
- Облако: `sentence-transformers/all-MiniLM-L6-v2`
- Локально: `sentence-transformers/all-MiniLM-L6-v2` (gguf/onnx)

❌ **Неправильно:**
- Облако: `OpenAI/text-embedding-3-small` (1536 dim)
- Локально: `all-MiniLM-L6-v2` (384 dim)

### 2. Rate Limits HuggingFace

Бесплатный API имеет ограничения:
- ~30 запросов в минуту
- ~1000 запросов в день

**Решение:**
- Включить кэш (по умолчанию включён)
- Увеличить размер кэша (500 → 1000)
- Для продакшена: добавить API ключ (увеличивает лимиты)

### 3. Размерность эмбеддинга

`all-MiniLM-L6-v2` → **384 измерения**

Все индексы Faiss должны быть созданы с этой размерностью.

---

## 📁 Изменённые файлы (полный список)

1. `include/core/embedding_generator.h` — обновлён
2. `src/core/embedding_generator.cpp` — переписан
3. `include/core/rag_settings.h` — обновлён
4. `include/ui/embedding_settings_dialog.h` — новый
5. `src/ui/embedding_settings_dialog.cpp` — новый
6. `CMakeLists.txt` — добавлены новые файлы
7. `EMBEDDING_OPTIMIZATION_GUIDE.md` — документация
8. `OPTIMIZATION_SUMMARY.md` — это файл

---

## 🧪 Тестирование

### Быстрый тест работоспособности:

1. Загрузите документ по астрономии
2. Откройте настройки эмбеддингов
3. Включите Hybrid режим с HuggingFace
4. Запрос: "Что такое Юпитер?"
5. Ожидаемый результат: развёрнутый ответ о планете

### Проверка кэша:

1. Сделайте тот же запрос повторно
2. Запрос должен выполниться <10ms (из кэша)
3. Статистика: Cache hits +1

---

## 📚 Дополнительные ресурсы

- [HuggingFace Embedding API](https://huggingface.co/docs/api-inference)
- [all-MiniLM-L6-v2](https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2)
- [Sentence Transformers](https://www.sbert.net/)
- [FAISS Documentation](https://faiss.ai/)

---

**Дата:** четверг, 2 апреля 2026 г.  
**Версия:** 1.0  
**Статус:** Готово к применению
