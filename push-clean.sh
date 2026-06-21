#!/bin/bash
# push-clean.sh — пуш без истории

set -e

echo "🧹 Cleaning up before push..."

# 1. Удаляем временные файлы
find . -type f \( -name "*.log" -o -name "*.tmp" -o -name "*.bak" -o -name ".DS_Store" -o -name "Thumbs.db" \) -delete 2>/dev/null || true

# 2. Добавляем изменения
git add -A

# 3. Проверяем, есть ли что коммитить
if git diff --cached --quiet; then
    echo "✅ No changes to commit"
    exit 0
fi

# 4. Squash в один коммит (если есть история)
COMMIT_COUNT=$(git rev-list --count HEAD 2>/dev/null || echo "0")

if [ "$COMMIT_COUNT" -gt 1 ]; then
    echo "📦 Squashing $COMMIT_COUNT commits..."
    
    # Создаем orphan-ветку
    git checkout --orphan temp-clean
    git add -A
    git commit -m "Update: $(date '+%Y-%m-%d %H:%M')"
    
    # Удаляем старую main
    git branch -D main
    git branch -m main
    
    # Force push
    git push -f origin main
else
    # Первый коммит или уже один
    git commit -m "Update: $(date '+%Y-%m-%d %H:%M')"
    git push -f origin main
fi

# 5. Чистим локальный Git (освобождаем место)
git reflog expire --expire=now --all
git gc --prune=now --aggressive

echo "✅ Push complete. Repository size:"
du -sh .git
