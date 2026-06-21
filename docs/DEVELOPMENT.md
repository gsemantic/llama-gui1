# Разработка и поддержка

## 🔄 Автоматический бэкап

После значительных изменений делайте бэкап:

```bash
./scripts/backup.sh "Описание изменений"
```

**Что происходит:**
- ✅ Локально сохраняется только последний бэкап
- ✅ В Яндекс.Диске сохраняется вся история
- ✅ Старые локальные бэкапы удаляются автоматически

**Подробнее:** см. `docs/BACKUP_README.md`

---

## 📊 Версионирование

Текущая версия: **0.1.0-alpha.2 "Archimedes"**

Для обновления версии откройте `CMakeLists.txt` (строки 5-11):

```cmake
set(VERSION_MAJOR 0)
set(VERSION_MINOR 1)
set(VERSION_PATCH 0)
set(VERSION_SUFFIX "alpha.3")  # Измените здесь
set(VERSION_CODENAME "Archimedes")
```

**Подробнее:** см. `docs/VERSIONING.md`

---

## 🛠️ Сборка

```bash
mkdir build && cd build
cmake ..
make -j4
```

---

## 📁 Структура проекта

```
llama-gui/
├── src/              # Исходный код
├── include/          # Заголовочные файлы
├── scripts/          # Скрипты (backup.sh)
├── docs/             # Документация
├── _backups/         # Локальные бэкапы (авто)
├── build/            # Сборка (не хранится в git)
└── CMakeLists.txt    # Конфигурация сборки
```
