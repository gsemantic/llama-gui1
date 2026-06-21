# Архитектура системы OpenRouter RAG Deep Analysis

## Схема архитектуры

```
┌─────────────────────────────────────────────────────────────────────────┐
│                        RAG Deep Analysis System                         │
└─────────────────────────────────────────────────────────────────────────┘
                                    │
                    ┌───────────────┴───────────────┐
                    │                               │
          ┌─────────▼─────────┐           ┌────────▼────────┐
          │  OpenRouter API   │           │ Local LLM Server│
          │  (Cloud)          │           │ (localhost:8081)│
          │                   │           │                 │
          │  • Free models    │           │ • llama.cpp     │
          │  • No API key     │           │ • Full control  │
          │  • Rate limits    │           │ • No limits     │
          └─────────┬─────────┘           └────────┬────────┘
                    │                               │
                    │         ┌─────────────────────┘
                    │         │
          ┌─────────▼─────────▼──────────┐
          │     OpenRouterClient         │
          │                              │
          │  • is_api_available()  ✅    │ ← NEW: Проверка API
          │  • get_rate_limit()    ✅    │ ← NEW: Проверка лимитов
          │  • complete()          ✅    │ ← IMPROVED: Таймауты
          │  • parse_response()    ✅    │ ← IMPROVED: Логирование
          └──────────────┬───────────────┘
                         │
          ┌──────────────▼───────────────┐
          │      RagManager              │
          │                              │
          │  process_deep_analysis()     │
          │       │                      │
          │       ▼                      │
          │  process_deep_analysis_      │
          │       mapreduce()            │
          │       │                      │
          │       ├─→ MAP Phase          │
          │       │   • check_api()      │ ← NEW
          │       │   • check_rate_limit()│ ← NEW
          │       │   • set_timeout(120s)│ ← NEW
          │       │   • select_model(Map)│ ← NEW
          │       │   • generate_chunk_  │
          │       │     summary_with_    │ ← NEW
          │       │     fallback()       │
          │       │                      │
          │       └─→ REDUCE Phase       │
          │           • set_timeout(180s)│ ← NEW
          │           • select_model(    │ ← NEW
          │                Reduce)       │
          │           • synthesize_      │
          │             final_answer()   │
          └──────────────────────────────┘
                         │
          ┌──────────────▼───────────────┐
          │   Model Selection Logic      │
          │   (rag_manager_model_        │
          │    selection.cpp)            │
          │                              │
          │  select_model_for_stage()    │
          │       │                      │
          │       ├─→ Map Stage          │
          │       │   • Fast model       │
          │       │   • google/gemma-2-  │
          │       │     9b-it:free       │
          │       │                      │
          │       └─→ Reduce Stage       │
          │           • Quality model    │
          │           • meta-llama/      │
          │             llama-3-8b-      │
          │             instruct:free    │
          └──────────────────────────────┘
```

---

## Поток данных: Map-Reduce режим

```
┌──────────────────────────────────────────────────────────────────┐
│                    Map-Reduce Workflow                            │
└──────────────────────────────────────────────────────────────────┘

User Query: "Проанализируй документ"
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 1. PRE-FLIGHT CHECKS (НОВЫЕ МЕТОДЫ)                             │
│                                                                 │
│    ┌─────────────────────┐     ┌─────────────────────┐         │
│    │ check_openrouter_   │     │ check_openrouter_   │         │
│    │ availability()      │     │ rate_limit(10)      │         │
│    │                     │     │                     │         │
│    │ GET /api/v1/models  │     │ GET /api/v1/auth/key│         │
│    │ ❌/✅                │     │ ❌/✅                │         │
│    └─────────────────────┘     └─────────────────────┘         │
│                                                                 │
│    Если ❌ → Fallback на локальный сервер                       │
└─────────────────────────────────────────────────────────────────┘
       │ ✅
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 2. MAP PHASE (Суммаризация чанков)                              │
│                                                                 │
│    Chunks: [C1][C2][C3][C4][C5][C6][C7][C8]...                 │
│              │                    │                             │
│              ▼                    ▼                             │
│    ┌─────────────────────────────────────────────────┐         │
│    │ Batch 1: C1+C2+C3+C4                            │         │
│    │   • set_timeout(120000) ← НОВЫЙ ТАЙМАУТ         │         │
│    │   • select_model(Map) ← google/gemma-2-9b-it    │         │
│    │   • generate_chunk_summary_with_fallback()      │         │
│    │     ├─→ OpenRouter (с retry)                    │         │
│    │     └─→ Local server (fallback)                 │         │
│    │   • Log: (model: google/gemma-2-9b-it:free)     │         │
│    │   → Summary 1                                   │         │
│    └─────────────────────────────────────────────────┘         │
│                                                                 │
│    ┌─────────────────────────────────────────────────┐         │
│    │ Batch 2: C5+C6+C7+C8                            │         │
│    │   • ... (аналогично)                            │         │
│    │   → Summary 2                                   │         │
│    └─────────────────────────────────────────────────┘         │
│                                                                 │
│    Result: [Summary 1][Summary 2]...                           │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 3. REDUCE PHASE (Синтез финального ответа)                      │
│                                                                 │
│    Summaries: [S1][S2][S3]                                     │
│                    │                                            │
│                    ▼                                            │
│    ┌─────────────────────────────────────────────────┐         │
│    │ Synthesize Final Answer                         │         │
│    │   • set_timeout(180000) ← НОВЫЙ ТАЙМАУТ         │         │
│    │   • select_model(Reduce) ← meta-llama/llama-3   │         │
│    │   • synthesize_final_answer_openrouter()        │         │
│    │     ├─→ OpenRouter (с fallback)                 │         │
│    │     └─→ Local server (fallback)                 │         │
│    │   • Log: (model: meta-llama/llama-3-8b...)      │         │
│    │   → Final Answer                                │         │
│    └─────────────────────────────────────────────────┘         │
│                                                                 │
│    Result: "Интегрированный ответ на основе всех источников"   │
└─────────────────────────────────────────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────────────────────────────────┐
│ 4. RESPONSE TO USER                                             │
│                                                                 │
│    Final Answer → User Interface                                │
│                                                                 │
│    Log Summary:                                                 │
│    ✅ API проверено                                             │
│    ✅ Лимиты проверены                                          │
│    ✅ Map: 120s timeout, model: google/gemma-2-9b-it:free       │
│    ✅ Reduce: 180s timeout, model: meta-llama/llama-3-8b...     │
│    ✅ Фактические модели залогированы                           │
└─────────────────────────────────────────────────────────────────┘
```

---

## Fallback механизм

```
┌──────────────────────────────────────────────────────────────────┐
│                    Fallback Decision Tree                        │
└──────────────────────────────────────────────────────────────────┘

generate_chunk_summary_with_fallback()
         │
         ▼
    ┌────────────────────┐
    │ OpenRouter enabled?│
    └────┬───────────┬───┘
         │ No        │ Yes
         │           ▼
         │    ┌────────────────────┐
         │    │ check_api_         │
         │    │ availability()     │
         │    └────┬───────────┬───┘
         │         │ ❌        │ ✅
         │         │           ▼
         │         │    ┌────────────────────┐
         │         │    │ check_rate_limit() │
         │         │    └────┬───────────┬───┘
         │         │         │ ❌        │ ✅
         │         │         │           ▼
         │         │         │    ┌────────────────────┐
         │         │         │    │ OpenRouter request │
         │         │         │    │ (max 1 retry)      │
         │         │         │    └────┬───────────┬───┘
         │         │         │         │ ❌        │ ✅
         │         │         │         │           │
         │         │         │         │           ▼
         │         │         │         │    Success!
         │         │         │         │    Return summary
         │         │         │         │
         │         │         │         ▼
         │         │         │    Error type?
         │         │         │    ├─→ Rate Limit (429)
         │         │         │    ├─→ API Error (400/401/403/500)
         │         │         │    └─→ Timeout (retry 1x)
         │         │         │         │
         │         │         │         ▼
         │         │         │    Fallback to local
         │         │         │
         │         │         ▼
         │         │    Fallback immediately
         │         │
         │         ▼
         ▼    ▼
    ┌────────────────────────┐
    │ generate_chunk_summary │
    │ _local()               │
    │                        │
    │ • llama_interface_     │
    │ • 90s timeout          │
    │ • No API needed        │
    └────────────────────────┘
```

---

## Выбор моделей

```
┌──────────────────────────────────────────────────────────────────┐
│                    Model Selection Strategy                      │
└──────────────────────────────────────────────────────────────────┘

select_model_for_stage(stage, settings)
         │
         ├─→ Manual model specified?
         │   └─→ Use manual model
         │
         ├─→ Auto-select enabled?
         │   └─→ Select from free models by profile
         │       ├─→ "fast" profile
         │       │   └─→ Max speed rating
         │       ├─→ "quality" profile
         │       │   └─→ Max quality rating
         │       ├─→ "economy" profile
         │       │   └─→ Smallest context
         │       └─→ "balanced" profile (default)
         │           ├─→ Map: google/gemma-2-9b-it:free
         │           └─→ Reduce: meta-llama/llama-3-8b-instruct:free
         │
         └─→ Default
             ├─→ Map: google/gemma-2-9b-it:free
             └─→ Reduce: meta-llama/llama-3-8b-instruct:free


Free Models Database:
┌─────────────────────────────────────────────────────────────────┐
│ Model ID                          │ Speed │ Quality │ Best for │
├─────────────────────────────────────────────────────────────────┤
│ google/gemma-2-2b-it:free         │  5/5  │   2/5   │   Map    │
│ microsoft/phi-3-mini-128k:free    │  4/5  │   3/5   │   Map    │
│ google/gemma-2-9b-it:free         │  4/5  │   4/5   │  Both    │
│ meta-llama/llama-3-8b-instruct    │  4/5  │   4/5   │  Both    │
│ nousresearch/hermes-2-pro:free    │  3/5  │   5/5   │  Reduce  │
│ huggingface/zephyr-7b-beta:free   │  3/5  │   3/5   │  Reduce  │
└─────────────────────────────────────────────────────────────────┘
```

---

## RAII Timeout Management

```
┌──────────────────────────────────────────────────────────────────┐
│                    Timeout Management (RAII)                     │
└──────────────────────────────────────────────────────────────────┘

process_deep_analysis_mapreduce()
         │
         ▼
    ┌─────────────────────────────────────────┐
    │ 1. Save original timeout (30000ms)      │
    │    original_timeout = 30000             │
    │                                         │
    │ 2. Create RAII restorer                 │
    │    TimeoutRestorer restorer(client,     │
    │                             original_timeout)
    │                                         │
    │ 3. Set new timeout (120000ms)           │
    │    client->set_timeout(120000)          │
    │                                         │
    │ 4. Execute Map phase...                 │
    │    • Parallel batch processing          │
    │    • Multiple API calls                 │
    │                                         │
    │ 5. Exit function (normal or exception)  │
    │         │                               │
    │         ▼                               │
    │    ┌─────────────────────────┐          │
    │    │ ~TimeoutRestorer()      │          │
    │    │   Automatically calls:  │          │
    │    │   client->set_timeout(  │          │
    │    │     original_timeout)   │          │
    │    └─────────────────────────┘          │
    │                                         │
    │ 6. Timeout restored to 30000ms ✅       │
    └─────────────────────────────────────────┘


Benefits:
✅ Guaranteed restoration even on exceptions
✅ No manual cleanup required
✅ Exception-safe
✅ Clear ownership semantics
```

---

## API Endpoints

```
┌──────────────────────────────────────────────────────────────────┐
│                    OpenRouter API Endpoints                      │
└──────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ GET /api/v1/models                                              │
│                                                                 │
│ Purpose: Проверка доступности API                               │
│ Cost: FREE (не тратит лимиты генерации)                         │
│ Response: Список всех моделей                                   │
│                                                                 │
│ Usage: is_api_available()                                       │
│                                                                 │
│ Example:                                                        │
│   curl https://openrouter.ai/api/v1/models                      │
│   → {"data": [{"id": "google/gemma-2-9b-it:free", ...}, ...]}   │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ GET /api/v1/auth/key                                            │
│                                                                 │
│ Purpose: Проверка лимитов API ключа                             │
│ Cost: FREE (не тратит лимиты генерации)                         │
│ Response: Информация о лимитах                                  │
│                                                                 │
│ Usage: get_rate_limit()                                         │
│                                                                 │
│ Example:                                                        │
│   curl -H "Authorization: Bearer sk-or-..." \                   │
│        https://openrouter.ai/api/v1/auth/key                    │
│   → {"data": {"usage": {"requests": 42}, "limit": 50}}          │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│ POST /api/v1/chat/completions                                   │
│                                                                 │
│ Purpose: Генерация текста (Map и Reduce этапы)                  │
│ Cost: 1 запрос из дневного лимита (для free моделей)            │
│ Request: Messages, model, parameters                            │
│ Response: Generated text, model info, usage stats               │
│                                                                 │
│ Usage: complete()                                               │
│                                                                 │
│ Example:                                                        │
│   curl -X POST https://openrouter.ai/api/v1/chat/completions \  │
│        -H "Authorization: Bearer sk-or-..." \                   │
│        -d '{"model": "google/gemma-2-9b-it:free", ...}'         │
│   → {"id": "...", "model": "google/gemma-2-9b-it:free", ...}    │
│                                                                 │
│ Note: response.model содержит фактическую модель                │
└─────────────────────────────────────────────────────────────────┘
```

---

## Логирование

```
┌──────────────────────────────────────────────────────────────────┐
│                    Logging Examples                              │
└──────────────────────────────────────────────────────────────────┘

Normal Flow (OpenRouter):
═══════════════════════════════════════════════════════════════════
[OPENROUTER CHECK] Checking API availability...
[OPENROUTER] Checking API availability via GET /api/v1/models...
[OPENROUTER] ✅ API is available, 245 models found
[OPENROUTER CHECK] ✅ API is available

[RATE LIMIT CHECK] Checking OpenRouter rate limits...
[RATE LIMIT] Updated cache: 48/50 requests remaining
[RATE LIMIT CHECK] Limit: 50, Used: 2, Remaining: 48 (Free tier: yes)
[RATE LIMIT CHECK] ✅ Rate limit check passed

[MAP-REDUCE] === PHASE 1: MAP (Creating batch summaries) ===
[MAP-REDUCE] Timeout set to 120s for MAP phase
[MODEL SELECT] Auto-select enabled (FREE ONLY), profile: balanced
[MODEL SELECT] Map stage using model: google/gemma-2-9b-it:free

[FALLBACK] Attempt 1 to OpenRouter (chunk 0)
[SUMMARY] OpenRouter summary: 215 chars (~54 tokens) (model: google/gemma-2-9b-it:free)

[MAP-REDUCE] MAP phase completed in 2.3s
[MAP-REDUCE] Generated 2 batch summaries

[MAP-REDUCE] === PHASE 2: REDUCE (Synthesizing final answer) ===
[SYNTHESIS] Timeout set to 180s for REDUCE phase
[MODEL SELECT] Reduce stage using model: meta-llama/llama-3-8b-instruct:free

[SYNTHESIS] OpenRouter final answer: 1024 chars (~256 tokens) (model: meta-llama/llama-3-8b-instruct:free)

[MAP-REDUCE] REDUCE phase completed in 4.1s
[MAP-REDUCE] Timeout restored to 30000ms
[SYNTHESIS] Timeout restored to 30000ms

Fallback Flow (Local Server):
═══════════════════════════════════════════════════════════════════
[OPENROUTER CHECK] Checking API availability...
[OPENROUTER] Checking API availability via GET /api/v1/models...
[OPENROUTER] API check failed: empty response
[OPENROUTER CHECK] ❌ API is NOT available
[FALLBACK] OpenRouter unavailable, switching to local server
[SUMMARY] Using LOCAL server for summary (chunk 0, doc: batch_1)
[SUMMARY] Sending summary request to local server...
[SUMMARY] Local server summary: 198 chars (~50 tokens)
```

---

## State Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                    RAG Deep Analysis State Machine               │
└──────────────────────────────────────────────────────────────────┘

                    ┌─────────────┐
                    │   Idle      │
                    └──────┬──────┘
                           │ User query
                           ▼
                    ┌─────────────┐
              ┌─────│ Pre-flight  │─────┐
              │     │   Checks    │     │
              │     └──────┬──────┘     │
              │            │            │
              │     ┌──────┴──────┐     │
              │     │             │     │
              ▼     ▼             │     │
         ┌─────────┐       ┌──────────┐
         │ Local   │       │ OpenRouter│
         │ Server  │       │ Available?│
         │ Mode    │       └─────┬────┘
         │         │             │
         └────┬────┘      ┌──────┴──────┐
              │          │             │
              │          ▼             ▼
              │   ┌──────────┐  ┌──────────┐
              │   │ Rate     │  │ API      │
              │   │ Limit OK?│  │ Error    │
              │   └────┬─────┘  └────┬─────┘
              │        │             │
              │   ┌────┴─────┐       │
              │   │          │       │
              │   ▼          ▼       │
              │ ┌──────┐ ┌────────┐  │
              │ │ Map  │ │ Retry  │  │
              │ │ Phase│ │ (1x)   │  │
              │ └──┬───┘ └───┬────┘  │
              │    │         │       │
              │    │    ┌────┴───┐   │
              │    │    │        │   │
              │    │    ▼        ▼   │
              │    │ ┌──────┐ ┌──────┐
              │    │ │Success│ │Fail  │
              │    │ └──┬───┘ └──┬───┘
              │    │    │        │   │
              │    │    │   ┌────┴───┤
              │    │    │   │        │
              │    ▼    ▼   ▼        ▼
              │ ┌──────────────────────┐
              │ │   Reduce Phase       │
              │ │   • Timeout 180s     │
              │ │   • Quality model    │
              │ └──────────┬───────────┘
              │            │
              │     ┌──────┴──────┐
              │     │             │
              │     ▼             ▼
              │ ┌──────┐   ┌──────────┐
              │ │Success│   │Fallback  │
              │ └───┬───┘   │to Local  │
              │     │       └────┬─────┘
              │     │            │
              └─────┴────────────┘
                    │
                    ▼
              ┌─────────────┐
              │   Return    │
              │   Result    │
              └─────────────┘
```

---

**Документ создан:** четверг, 2 апреля 2026 г.  
**Версия:** 1.0  
**Статус:** Готово к применению
