#!/bin/bash
# clone-clean.sh — клонирование без истории

REPO_URL="${1:-git@github.com:gsemantic/llama-gui1.git}"
TARGET_DIR="${2:-llama-gui}"

echo "📥 Cloning $REPO_URL (shallow, no history)..."

# Shallow clone — только последний коммит
git clone --depth 1 "$REPO_URL" "$TARGET_DIR"

cd "$TARGET_DIR"

# Удаляем .git полностью
rm -rf .git

# Инициализируем заново
git init
git branch -M main
git add -A
git commit -m "Clean clone: $(date '+%Y-%m-%d %H:%M')"
git remote add origin "$REPO_URL"

echo "✅ Clean clone complete. Size:"
du -sh .
du -sh .git
