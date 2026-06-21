# Руководство пользователя системы агентов

**Версия:** 1.0.0  
**Дата:** 20 февраля 2026

---

## 📖 Содержание

1. [Введение](#введение)
2. [Начало работы](#начало-работы)
3. [Использование агентов](#использование-агентов)
4. [Команды агентов](#команды-агентов)
5. [Настройка агентов](#настройка-агентов)
6. [Решение проблем](#решение-проблем)

---

## Введение

Система агентов llama-gui позволяет расширять функциональность приложения с помощью специализированных модулей — агентов.

### Что такое агент?

**Агент** — это модуль, выполняющий определённую задачу:
- Поиск информации (RAG, веб-поиск)
- Работа с файлами
- Генерация кода
- Выполнение команд
- Суммаризация текста

### Доступные агенты

| Агент | Назначение | Статус |
|-------|------------|--------|
| RAG Agent | Поиск по документам | ✅ Встроен |
| File Agent | Файловые операции | ✅ Плагин |
| Web Search Agent | Поиск в интернете | ✅ Плагин |
| Code Agent | Генерация кода | ✅ Плагин |
| Terminal Agent | Команды терминала | ✅ Плагин |
| Summarization Agent | Суммаризация | ✅ Плагин |

---

## Начало работы

### 1. Открытие панели агентов

```
Menu → Tools → Agents Panel
```

Или используйте горячую клавишу (если настроена).

### 2. Просмотр доступных агентов

В панели агентов вы увидите список всех загруженных агентов:

- **Зелёный индикатор** `[●]` — агент готов
- **Жёлтый индикатор** `[◍]` — агент выполняет задачу
- **Серый индикатор** `[○]` — агент отключён
- **Красный индикатор** `[✖]` — ошибка агента

### 3. Включение/отключение агента

1. Найдите агента в списке
2. Нажмите кнопку **Enable** или **Disable**

---

## Использование агентов

### Через панель управления

1. Откройте **Agents Panel**
2. Выберите агента (кликните по имени)
3. Нажмите **Execute**
4. Введите параметры
5. Получите результат

### Через команды

Введите команду в чате или консоли:

```
/agent <имя_агента> <действие> [параметры]
```

**Пример:**
```
/agent file_agent read file_path='test.txt'
```

---

## Команды агентов

### RAG Agent (Поиск по документам)

```bash
# Поиск по документам
/rag <запрос>

# Примеры
/rag Как настроить сервер?
/rag документация по API
```

**Параметры:**
- `k` — количество результатов (по умолчанию 5)
- `threshold` — порог схожести (0.0-1.0)

```bash
/rag query='сервер' k=10 threshold=0.8
```

---

### File Agent (Файловые операции)

```bash
# Чтение файла
/file read <путь>

# Запись файла
/file write <путь> content='<содержимое>'

# Список файлов
/file list <директория>

# Информация о файле
/file info <путь>

# Копирование
/file copy src=<источник> dst=<назначение>

# Перемещение
/file move src=<источник> dst=<назначение>

# Удаление
/file delete <путь>
```

**Примеры:**
```bash
/file read /home/user/document.txt
/file write /home/user/output.txt content='Hello, World!'
/file list /home/user/projects
/file info /etc/hosts
```

---

### Web Search Agent (Веб-поиск)

```bash
# Поиск в интернете
/search <запрос>

# HTTP GET запрос
/agent web_search_agent get url='<URL>'

# HTTP POST запрос
/agent web_search_agent post url='<URL>' body='<данные>'
```

**Примеры:**
```bash
/search лучшие практики C++
/search tutorial Python
/agent web_search_agent get url='https://api.github.com/users/octocat'
```

**Поисковые движки:**
- DuckDuckGo (по умолчанию)
- Google

```bash
/agent web_search_agent search query='C++' engine='google'
```

---

### Code Agent (Генерация кода)

```bash
# Генерация кода
/code <язык> <описание>

# Анализ кода
/agent code_agent analyze code='<код>'

# Форматирование
/agent code_agent format code='<код>'

# Проверка стиля (lint)
/agent code_agent lint code='<код>'
```

**Примеры:**
```bash
/code python функция для вычисления факториала
/code cpp класс для работы с файлами
/code javascript сортировка массива

/agent code_agent analyze code='int main() { return 0; }'
```

**Поддерживаемые языки:**
- C++, C, Python, JavaScript, TypeScript
- Java, C#, Go, Rust, Ruby, PHP
- Swift, Kotlin

---

### Terminal Agent (Команды терминала)

```bash
# Выполнение команды
/agent terminal_agent exec command='<команда>'

# Безопасное выполнение (только белый список)
/agent terminal_agent exec_safe command='ls -la'

# Список разрешённых команд
/agent terminal_agent list_commands

# Добавление команды в белый список
/agent terminal_agent add_command command='git status'
```

**Примеры:**
```bash
/agent terminal_agent exec_safe command='pwd'
/agent terminal_agent exec_safe command='ls -la'
/agent terminal_agent exec_safe command='cat file.txt'
```

**Белый список по умолчанию:**
```
ls, pwd, whoami, date, echo, cat, head, tail, wc, grep, find, du, df, uname, hostname
```

---

### Summarization Agent (Суммаризация)

```bash
# Суммаризация текста
/summarize <текст>

# Суммаризация файла
/agent summarization_agent summarize_file file_path='<путь>'

# Извлечение ключевых точек
/agent summarization_agent extract_key_points text='<текст>'

# Очень краткое содержание (TL;DR)
/agent summarization_agent tldr text='<текст>'
```

**Примеры:**
```bash
/summarize Длинный текст для сокращения...

/agent summarization_agent summarize_file file_path='/home/user/document.txt'

/agent summarization_agent extract_key_points text='...' max_points=5
```

---

### Управление агентами

```bash
# Список всех агентов
/agents list

# Статус агентов
/agents status

# Включение агента
/agent <имя> enable

# Отключение агента
/agent <имя> disable
```

**Примеры:**
```bash
/agents list
# Вывод: Found 6 agents

/agents status
# Вывод: Total: 6, Enabled: 5, Disabled: 1
```

---

## Настройка агентов

### Через конфигурационный файл

**Файл:** `agents_config.json`

```json
{
    "agent_settings": {
        "rag_agent": {
            "enabled": true,
            "timeout_ms": 60000,
            "settings": {
                "max_chunks": 10,
                "similarity_threshold": 0.7
            }
        },
        "file_agent": {
            "enabled": true,
            "timeout_ms": 30000,
            "settings": {
                "allowed_extensions": [".txt", ".md", ".json"],
                "max_file_size_mb": 10
            }
        },
        "terminal_agent": {
            "enabled": false,
            "settings": {
                "allowed_commands": ["ls", "pwd", "cat"],
                "strict_mode": true
            }
        }
    }
}
```

### Через панель настроек

1. Откройте **Settings** (Ctrl+,)
2. Перейдите на вкладку **Agents**
3. Выберите агента
4. Измените параметры
5. Нажмите **Save**

---

## Решение проблем

### Агент не найден

**Ошибка:** `Agent 'name' not found`

**Причины:**
- Агент не загружен
- Опечатка в имени

**Решение:**
1. Проверьте список агентов: `/agents list`
2. Убедитесь, что плагин находится в директории `plugins/`
3. Перезапустите приложение

---

### Агент отключён

**Ошибка:** `Agent 'name' is disabled`

**Решение:**
1. Откройте **Agents Panel**
2. Нажмите **Enable** для агента
3. Или в конфиге `agents_config.json` установите `"enabled": true`

---

### Таймаут выполнения

**Ошибка:** `Execution timeout`

**Причины:**
- Агент выполняет долгую операцию
- Слишком маленький таймаут

**Решение:**
1. Увеличьте таймаут в параметрах:
   ```bash
   /agent rag_agent search query='...' timeout_ms=60000
   ```
2. Или в `agents_config.json`:
   ```json
   "rag_agent": {"timeout_ms": 60000}
   ```

---

### Ошибка доступа к файлу

**Ошибка:** `Access denied` или `Permission denied`

**Причины:**
- Файл в заблокированной директории
- Нет прав на чтение/запись

**Решение:**
1. Проверьте путь к файлу
2. Добавьте путь в разрешённые в `agents_config.json`:
   ```json
   "file_agent": {
       "settings": {
           "allowed_paths": ["/home/user/documents"]
       }
   }
   ```

---

### Ошибка безопасности

**Ошибка:** `Dangerous command blocked`

**Причины:**
- Попытка выполнить опасную команду

**Решение:**
- Не выполняйте опасные команды
- Для терминала используйте только команды из белого списка

---

## Часто задаваемые вопросы

### Как добавить нового агента?

См. [Руководство по разработке плагинов](AGENT_PLUGIN_GUIDE.md)

### Где найти логи агентов?

Логи находятся в файле `logs/agents.log` (если включено логирование).

### Как отключить логирование?

В `agents_config.json`:
```json
{
    "enable_logging": false
}
```

### Можно ли использовать несколько агентов одновременно?

Да, агенты работают независимо. Однако один агент может вызывать другие через контекст.

### Как обновить агента?

1. Замените файл плагина (.so) на новую версию
2. Используйте команду перезагрузки (если включена горячая перезагрузка)
3. Или перезапустите приложение

---

## Ссылки

- [Обзор системы агентов](AGENT_SYSTEM_OVERVIEW.md)
- [Руководство по плагинам](AGENT_PLUGIN_GUIDE.md)
- [Справка по командам](AGENT_COMMANDS.md)

---

**Это руководство является частью документации системы агентов llama-gui.**
