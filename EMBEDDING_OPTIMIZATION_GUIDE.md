# Оптимизация RAG и эмбеддингов: Руководство по применению

## 📋 Обзор изменений

Этот патч добавляет **гибридный облачно-локальный эмбеддинг** с полной совместимостью между режимами.

### Ключевые возможности

1. **Гибридный режим**: Облачный эмбеддинг с локальным fallback
2. **Совместимость**: Одинаковые векторы для облака и локальной модели
3. **Кэширование**: LRU кэш для экономии API запросов
4. **Настраиваемость**: Выбор провайдера, модели, режима работы

---

## 🔧 Изменённые файлы

### 1. `include/core/embedding_generator.h`
**Новый интерфейс с поддержкой:**
- Режимы: LocalOnly, CloudOnly, Hybrid, Auto
- Провайдеры: HuggingFace, OpenRouter, Custom
- Кэширование и статистика

### 2. `src/core/embedding_generator.cpp`
**Полная реализация:**
- `generate_embedding()` - автоматический выбор источника
- `generate_embedding_cloud()` - вызов HuggingFace API
- `generate_embedding_local()` - локальная gguf модель
- LRU кэш с настраиваемым размером

### 3. `include/core/rag_settings.h`
**Оптимизированные настройки:**
```cpp
// БЫЛО:
float similarity_threshold = 0.70f;
int max_chunks_in_memory = 100;
int max_rag_chunks = 20;

// СТАЛО:
float similarity_threshold = 0.50f;  // ↓ Ниже для лучшего поиска
int max_chunks_in_memory = 500;      // ↑ Больше документов
int max_rag_chunks = 50;             // ↑ Полнее контекст
int max_sequence_length = 1024;      // ↑ Длиннее контекст
```

### 4. `include/ui/embedding_settings_dialog.h` + `src/ui/embedding_settings_dialog.cpp`
**Новый UI диалог** для настройки эмбеддингов.

---

## 🚀 Как использовать

### Шаг 1: Настройка облачного эмбеддинга

1. Откройте настройки приложения
2. Перейдите в раздел **"Embedding Settings"**
3. Выберите режим: **Hybrid** (рекомендуется)

### Шаг 2: Получение API ключа HuggingFace

1. Зарегистрируйтесь на [huggingface.co](https://huggingface.co)
2. Перейдите в Settings → Access Tokens
3. Создайте новый токен с правами **read**
4. Скопируйте токен в поле **API Key**

### Шаг 3: Проверка работы

1. Нажмите **"Check API"** для проверки доступности
2. Должно появиться: `✅ Available`

### Шаг 4: Загрузка локальной модели (опционально)

Для работы без интернета:

```bash
# Скачайте модель
wget https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2/resolve/main/onnx/model.onnx

# Или GGUF версию (если доступна)
wget https://huggingface.co/togethercomputer/LLaMA-2-7B-32K/resolve/main/llama-2-7b-32k.gguf
```

---

## 📊 Сравнение режимов

| Режим | Скорость | Качество | Интернет | Стоимость |
|-------|----------|----------|----------|-----------|
| **Cloud Only** | ⚡⚡⚡ | ⭐⭐⭐⭐ | ✅ Требуется | ~$0.01/1000 |
| **Local Only** | ⚡⚡ | ⭐⭐⭐ | ❌ Не нужен | Бесплатно |
| **Hybrid** | ⚡⚡⚡ | ⭐⭐⭐⭐ | ⚠ Опционально | ~$0.005/1000 |
| **Auto** | ⚡⚡ | ⭐⭐⭐ | ⚠ Опционально | ~$0.005/1000 |

---

## 🎯 Рекомендации для вашей проблемы

### Проблема: "В учебнике по астрономии не найти упоминание об Юпитере"

**Причина:** Псевдо-эмбеддинг на основе хеша не улавливает семантику.

**Решение:**

1. **Включите Hybrid режим** с HuggingFace API
2. **Снизьте `similarity_threshold`** с 0.7 до 0.5
3. **Увеличьте `max_rag_chunks`** с 20 до 50

### Ожидаемый результат:

```
Запрос: "Что такое Юпитер?"

БЫЛО (хеш-эмбеддинг):
- Найдено: 0 чанков (порог 0.7 не пройден)
- Ответ: "Информация не найдена"

СТАЛО (семантический эмбеддинг):
- Найдено: 12 чанков (порог 0.5 пройден)
- Ответ: "Юпитер — пятая планета от Солнца, крупнейшая в Солнечной системе..."
```

---

## 🔍 Как работает совместимость

### Критически важно:

```
Облако: all-MiniLM-L6-v2 → вектор [0.1, -0.3, 0.5, ...] (384 измерения)
              ↓
        ОДНО пространство
              ↓
Локально: all-MiniLM-L6-v2 → вектор [0.1, -0.3, 0.5, ...] (384 измерения)
```

✅ **Векторы совместимы** — можно сравнивать напрямую

### НЕ совместимо:

```
Облако: OpenAI text-embedding-3-small → вектор (1536 измерений)
              ↓
        РАЗНЫЕ пространства ❌
              ↓
Локально: all-MiniLM-L6-v2 → вектор (384 измерения)
```

---

## 📈 Мониторинг и отладка

### Логи

```
[EmbeddingGenerator] Using cloud provider: all-MiniLM-L6-v2
[EmbeddingGenerator] HuggingFace embedding: 384 dimensions
[EmbeddingGenerator] Local embedding generated: 384 dimensions
```

### Статистика

В диалоге настроек отображается:
- Total requests: всего запросов
- Cache hits: попаданий в кэш
- Cloud/Local requests: распределение по источникам

---

## ⚠ Возможные проблемы

### 1. Rate Limit HuggingFace

**Симптом:** `HTTP Error: 429`

**Решение:**
- Используйте кэш (включён по умолчанию)
- Добавьте API ключ (увеличивает лимиты)
- Переключитесь на локальную модель

### 2. Векторы не совместимы

**Симптом:** Поиск возвращает нерелевантные результаты

**Причина:** Разные модели для облака и локально

**Решение:** Убедитесь, что обе модели — `all-MiniLM-L6-v2`

### 3. Медленная генерация

**Симптом:** >1 секунды на эмбеддинг

**Решение:**
- Включите кэш
- Используйте облако (быстрее)
- Уменьшите `max_sequence_length`

---

## 🧪 Тестирование

### Быстрый тест:

```cpp
// В main.cpp или тестовом файле
auto emb_gen = std::make_shared<EmbeddingGenerator>();

// Настройка облака
CloudEmbeddingConfig config;
config.provider = CloudEmbeddingProvider::HuggingFace;
config.api_key = "hf_xxx";
config.model_name = "all-MiniLM-L6-v2";

emb_gen->configure_cloud_provider(config);
emb_gen->set_mode(EmbeddingMode::Hybrid);

// Генерация
auto emb1 = emb_gen->generate_embedding("Юпитер — планета");
auto emb2 = emb_gen->generate_embedding("Jupiter is a planet");

// Проверка совместимости
float similarity = cosine_similarity(emb1, emb2);
std::cout << "Similarity: " << similarity << std::endl;  // Ожидается > 0.8
```

---

## 📚 Дополнительные ресурсы

- [HuggingFace Embedding API](https://huggingface.co/docs/api-inference/detailed_parameters#feature-extraction-task)
- [all-MiniLM-L6-v2 Model Card](https://huggingface.co/sentence-transformers/all-MiniLM-L6-v2)
- [Sentence Transformers Documentation](https://www.sbert.net/)

---

**Дата создания:** четверг, 2 апреля 2026 г.  
**Версия:** 1.0
