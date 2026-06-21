# Универсальный OpenAI-совместимый API

## Обзор

Новая вкладка "Универсальный OpenAI" в диалоге "Cloud Services" позволяет использовать **любой** OpenAI-совместимый API вместо локальной модели llama-server.

## Особенности

- ✅ Поддержка любых OpenAI-совместимых API
- ✅ Произвольный endpoint и базовый URL
- ✅ Поддержка chat completions API
- ✅ Произвольные параметры (temperature, top_p, etc.)
- ✅ Полная совместимость с OpenAI API спецификацией

## Поддерживаемые провайдеры

### 1. OpenRouter
```bash
# .env
OPENROUTER_API_KEY=sk-or-...
```
- URL: `https://openrouter.ai/api/v1`
- Endpoint: `chat/completions`
- Модели: 100+ моделей от разных провайдеров

### 2. KiloCode
```bash
# .env
KILOCODE_API_KEY=sk-kilo-...
```
- URL: `https://api.kilo.ai/api/gateway`
- Endpoint: `chat/completions`
- Использует Tor прокси для анонимности

### 3. OpenAI (официальный)
```bash
# .env
OPENAI_API_KEY=sk-...
```
- URL: `https://api.openai.com/v1`
- Endpoint: `chat/completions`
- Официальные модели GPT

### 4. Локальные инстансы
```bash
# .env
UNIVERSAL_OPENAI_API_KEY=local-key
```
- URL: `http://localhost:8081`
- Endpoint: `chat/completions`
- Любой локальный сервер с OpenAI-совместимой API

### 5. Другие провайдеры
- Любой сервис с OpenAI-совместимой API
- API ключи хранятся в `.env` файле

## Настройка

### 1. Открыть диалог
- Главное меню → Cloud Services
- Или горячая клавиша: `Ctrl+Shift+C`

### 2. Выбрать провайдер
- **Custom** — произвольный API
- **OpenRouter** — быстрый выбор
- **KiloCode** — с Tor прокси
- **OpenAI** — официальный API

### 3. Настроить API ключ
- Введите API ключ в поле "API Key"
- Или нажмите "Paste" для вставки из буфера обмена
- Ключ автоматически сохраняется в `.env` файле

### 4. Настроить URL (если нужно)
- **Custom Base URL**: `https://api.openai.com/v1`
- **Custom Endpoint**: `chat/completions`

### 5. Выбрать модель
- Нажмите "Выбрать модель..."
- Система загрузит список моделей из API
- Поиск по имени модели
- Фильтр бесплатных моделей

### 6. Включить API
- Поставьте галочку "Использовать универсальный API"
- Автоматически отключит другие провайдеры

## Использование

После включения универсального API:

1. Все запросы будут отправляться в выбранный API
2. Локальная модель llama-server не используется
3. API ключ берётся из `.env` файла
4. Настройки модели сохраняются в `settings.json`

## Примеры конфигурации

### OpenRouter
```
Custom Base URL: https://openrouter.ai/api/v1
Endpoint: chat/completions
API Key: sk-or-...
```

### KiloCode
```
Custom Base URL: https://api.kilo.ai/api/gateway
Endpoint: chat/completions
API Key: sk-kilo-...
```

### OpenAI
```
Custom Base URL: https://api.openai.com/v1
Endpoint: chat/completions
API Key: sk-...
```

### Локальный сервер
```
Custom Base URL: http://localhost:8081
Endpoint: chat/completions
API Key: local-key
```

## Безопасность

- ✅ API ключи хранятся в `.env` файле (не в settings.json)
- ✅ `.env` добавлен в `.gitignore`
- ✅ Ключи не коммитятся в git
- ✅ Парольное поле для ввода ключа

## Совместимость

API полностью совместим с [OpenAI Chat Completions API](https://platform.openai.com/docs/api-reference/chat):

- ✅ Chat completions
- ✅ Messages (system, user, assistant)
- ✅ Temperature, top_p, top_k
- ✅ Presence/frequency penalty
- ✅ Stop sequences
- ✅ Stream responses
- ✅ Usage statistics

## Файлы проекта

### Клиент
- `include/core/universal_openai_client.h` - интерфейс клиента
- `src/core/universal_openai_client.cpp` - реализация

### Типы данных
- `include/core/universal_openai_types.h` - структуры данных

### Настройки
- `include/core/universal_openai_settings.h` - настройки

### UI
- `include/ui/universal_openai_dialog.h` - интерфейс диалога
- `src/ui/universal_openai_dialog.cpp` - реализация UI

### Интеграция
- `src/core/settings_serialization_advanced.cpp` - сериализация
- `src/core/settings_core.cpp` - методы API ключей

## Troubleshooting

### Ошибка авторизации
- Проверьте API ключ в `.env` файле
- Убедитесь, что ключ правильный
- Попробуйте перезагрузить приложение

### Модель не найдена
- Проверьте название модели в списке
- Попробуйте обновить список моделей
- Убедитесь, что API доступен

### Rate limit exceeded
- Подождите несколько минут
- Используйте бесплатную модель
- Добавьте свой API ключ

## Дополнительные ресурсы

- [OpenAI API Documentation](https://platform.openai.com/docs/api-reference/chat)
- [OpenRouter Documentation](https://openrouter.ai/docs)
- [KiloCode Documentation](https://kilo.ai/docs)

## Лицензия

То же, что и у проекта llama-gui
