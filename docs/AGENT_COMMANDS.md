# Справка по командам системы агентов

**Версия:** 1.0.0  
**Дата:** 20 февраля 2026

---

## 📖 Содержание

1. [Общий формат команд](#общий-формат-команд)
2. [Команды агентов](#команды-агентов)
3. [Параметры команд](#параметры-команд)
4. [Примеры использования](#примеры-использования)
5. [Коды ошибок](#коды-ошибок)

---

## Общий формат команд

```
/<команда> [аргументы] [параметры]
```

**Параметры** передаются в формате `key=value`:
```
/команда аргумент1 аргумент2 key1=value1 key2=value2
```

---

## Команды агентов

### /agent

**Описание:** Вызов любого агента для выполнения действия

**Синтаксис:**
```
/agent <имя_агента> <действие> [параметры]
```

**Примеры:**
```bash
/agent rag_agent search query='сервер'
/agent file_agent read file_path='test.txt'
/agent code_agent generate language=python prompt='Hello World'
```

---

### /rag

**Описание:** Быстрый вызов RAG-поиска по документам

**Синтаксис:**
```
/rag <запрос>
```

**Параметры:**
- `query` — текст запроса
- `k` — количество результатов (по умолчанию 5)
- `threshold` — порог схожести (0.0-1.0, по умолчанию 0.7)

**Примеры:**
```bash
/rag Как настроить сервер?
/rag query='документация API' k=10
/rag 'лучшие практики C++' threshold=0.8
```

---

### /search

**Описание:** Поиск в интернете через веб-поисковик

**Синтаксис:**
```
/search <запрос>
```

**Параметры:**
- `query` — текст запроса
- `engine` — поисковый движок (duckduckgo, google)

**Примеры:**
```bash
/search лучшие практики C++
/search tutorial Python
/search query='C++ templates' engine='google'
```

---

### /summarize

**Описание:** Суммаризация текста

**Синтаксис:**
```
/summarize <текст>
```

**Параметры:**
- `text` — текст для суммаризации
- `max_sentences` — максимальное количество предложений (по умолчанию 3)

**Примеры:**
```bash
/summarize Длинный текст для сокращения...
/summarize '...' max_sentences=5
```

---

### /file

**Описание:** Файловые операции

**Синтаксис:**
```
/file <действие> <путь> [параметры]
```

**Действия:**

| Действие | Параметры | Описание |
|----------|-----------|----------|
| `read` | `file_path` | Чтение файла |
| `write` | `file_path`, `content` | Запись файла |
| `append` | `file_path`, `content` | Добавление в файл |
| `delete` | `file_path` | Удаление файла |
| `exists` | `file_path` | Проверка существования |
| `list` | `dir` | Список файлов |
| `copy` | `src`, `dst` | Копирование |
| `move` | `src`, `dst` | Перемещение |
| `info` | `file_path` | Информация о файле |

**Примеры:**
```bash
/file read /home/user/document.txt
/file write /home/user/output.txt content='Hello!'
/file list /home/user/projects
/file copy src=source.txt dst=backup.txt
/file delete /tmp/temp.txt
```

---

### /code

**Описание:** Генерация кода

**Синтаксис:**
```
/code <язык> <описание>
```

**Параметры:**
- `language` — язык программирования
- `prompt` — описание кода

**Примеры:**
```bash
/code python функция для вычисления факториала
/code cpp класс для работы с файлами
/code javascript сортировка массива
/code language=python prompt='REST API client'
```

**Поддерживаемые языки:**
- `cpp`, `c` — C/C++
- `python` — Python
- `javascript`, `js` — JavaScript
- `typescript`, `ts` — TypeScript
- `java` — Java
- `csharp`, `cs` — C#
- `go` — Go
- `rust` — Rust
- `ruby` — Ruby
- `php` — PHP
- `swift` — Swift
- `kotlin` — Kotlin

---

### /agents

**Описание:** Управление системой агентов

**Синтаксис:**
```
/agents <подкоманда>
```

**Подкоманды:**

| Подкоманда | Описание |
|------------|----------|
| `list` | Список всех агентов |
| `status` | Статистика по агентам |
| `enable <name>` | Включить агента |
| `disable <name>` | Отключить агента |

**Примеры:**
```bash
/agents list
/agents status
/agents enable terminal_agent
/agents disable rag_agent
```

---

## Параметры команд

### Общие параметры

| Параметр | Описание | Пример |
|----------|----------|--------|
| `timeout_ms` | Таймаут выполнения | `timeout_ms=30000` |
| `verbose` | Подробный вывод | `verbose=true` |

### Параметры для всех агентов

| Параметр | Описание | Пример |
|----------|----------|--------|
| `file_path` | Путь к файлу | `file_path='test.txt'` |
| `content` | Содержимое | `content='Hello'` |
| `query` | Запрос | `query='сервер'` |

---

## Примеры использования

### Комбинированные запросы

```bash
# Чтение файла и суммаризация
/file read /home/user/document.txt
/summarize <результат чтения>

# Поиск и генерация кода
/search C++ REST API example
/code cpp создать HTTP клиент на основе найденного
```

### Сценарии использования

#### 1. Работа с документацией

```bash
# Загрузить документ в RAG
/agent rag_agent add_document file_path='/home/user/docs/api.md'

# Поиск по документации
/rag как использовать API?

# Суммаризация результатов
/summarize <результаты поиска>
```

#### 2. Разработка кода

```bash
# Генерация кода
/code python функция для парсинга JSON

# Анализ сгенерированного кода
/agent code_agent analyze code='<код>'

# Проверка стиля
/agent code_agent lint code='<код>'
```

#### 3. Файловые операции

```bash
# Создание резервной копии
/file copy src=config.json dst=config.json.bak

# Просмотр содержимого
/file read config.json.bak

# Редактирование
/file write config.json content='{...}'
```

---

## Коды ошибок

### Формат ответа

```json
{
    "success": false,
    "message": "Описание ошибки",
    "agent_name": "имя_агента",
    "action": "действие"
}
```

### Типичные ошибки

| Код/Сообщение | Описание | Решение |
|---------------|----------|---------|
| `Agent not found` | Агент не найден | Проверьте имя агента |
| `Agent is disabled` | Агент отключён | Включите агента |
| `Agent not ready` | Агент не готов | Проверьте статус агента |
| `Unknown action` | Неизвестное действие | Проверьте доступные действия |
| `Missing parameter` | Отсутствует параметр | Добавьте требуемый параметр |
| `Invalid parameter` | Неверный параметр | Проверьте формат параметра |
| `Execution timeout` | Превышено время ожидания | Увеличьте `timeout_ms` |
| `Access denied` | Доступ запрещён | Проверьте права доступа |
| `File not found` | Файл не найден | Проверьте путь к файлу |
| `Dangerous command` | Опасная команда | Используйте безопасные команды |

---

## Быстрая справка

### Получить помощь по команде

```bash
/agent --help
/rag --help
/file --help
```

### Список доступных действий агента

```bash
/agent <имя> help
```

**Пример:**
```bash
/agent file_agent help
# Вывод: Доступные действия: read, write, delete, list, copy, move, info
```

---

## Ссылки

- [Обзор системы агентов](AGENT_SYSTEM_OVERVIEW.md)
- [Руководство пользователя](AGENT_USER_GUIDE.md)
- [Руководство по плагинам](AGENT_PLUGIN_GUIDE.md)

---

**Это справка по командам системы агентов llama-gui.**
