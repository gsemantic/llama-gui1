#include "../include/core/rag_manager.h"
#include "../include/core/embedding_generator.h"
#include "../include/core/document_parser.h"
#include <fstream>
#include <iostream>

#ifdef USE_FAISS
#include <faiss/IndexFlat.h>
#endif

namespace llama_gui {
namespace core {

bool RagManager::process_document(const std::string& file_path) {
    // Проверяем, что путь к файлу не пуст
    if (file_path.empty()) {
        std::cerr << "Error: Empty file path provided for document processing" << std::endl;
        return false;
    }

    // Проверяем существование файла
    std::ifstream file(file_path);
    if (!file.good()) {
        std::cerr << "Error: Document file does not exist or is not accessible: " << file_path << std::endl;
        return false;
    }
    file.close();

    // === ПРОВЕРКА: Не был ли документ уже проиндексирован ===
    if (is_document_indexed(file_path)) {
        std::cout << "[RAG] Document already indexed, skipping: " << file_path << std::endl;
        return false;  // Документ уже есть в индексе
    }
    // ============================================================

    // Генерируем document_id
    std::string doc_id = generate_doc_id(file_path);

    // Используем DocumentParser для обработки различных форматов
    auto document_parts = DocumentParser::parse_document(file_path);

    if (document_parts.empty()) {
        std::cerr << "Error: Could not parse document " << file_path << std::endl;
        return false;
    }

    // === KV-CACHE: Сохраняем полный текст документа в KV-cache ===
    if (enable_kv_cache_) {
        // Проверяем, есть уже сохранённый KV-cache
        if (has_document_kv_cache(doc_id)) {
            // KV-cache уже существует
        } else {
            // Собираем полный текст документа
            std::string full_text;
            full_text.reserve(10000);
            for (const auto& part : document_parts) {
                if (!part.empty()) {
                    full_text += part + "\n";
                }
            }

            // Загружаем текст в слот для токенизации и сохраняем KV-cache
            // Используем слот 0 (резервируем слоты 1-3 для чата)
            int slot_id = 0;
            auto tokenize_result = llama_interface_->tokenize_text_in_slot(slot_id, full_text);

            if (tokenize_result.success) {
                // Сохраняем KV-cache
                if (save_document_kv_cache(doc_id, slot_id)) {
                    doc_id_to_slot_map_[doc_id] = slot_id;
                }

                // Сбрасываем слот после сохранения
                llama_interface_->reset_slot(slot_id);
            }
        }
    }
    // === КОНЕЦ KV-CACHE ===

    // Обрабатываем каждый фрагмент документа для RAG-поиска
    int total_chunks = 0;
    for (size_t i = 0; i < document_parts.size(); ++i) {
        // Проверяем, что фрагмент не пуст
        if (document_parts[i].empty()) {
            continue;
        }

        // Разбиваем каждый фрагмент на чанки, если он слишком большой
        // Используем max_tokens_per_chunk из настроек (по умолчанию 2048)
        auto chunks = split_into_chunks(document_parts[i], max_tokens_per_chunk_);

        if (chunks.empty()) {
            continue;
        }

        for (size_t j = 0; j < chunks.size(); ++j) {
            if (!process_text_chunk(chunks[j], file_path, static_cast<int>(i * 1000 + j))) {
                // Продолжаем обработку остальных чанков
            } else {
                total_chunks++;
            }
        }
    }

    // Добавляем документ в текущий профиль
    profile_manager_.add_document_to_current_profile(file_path);
    profile_manager_.update_current_profile_chunk_count(static_cast<int>(external_chunks_.size()));

    // === ПЕРСИСТЕНТНОСТЬ: Автосохранение индекса после обработки документа ===
    save_index();

    return total_chunks > 0;
}

bool RagManager::process_text_chunk(const std::string& text, const std::string& doc_id, int chunk_index) {
    // Проверяем, что текст не пуст
    if (text.empty()) {
        std::cerr << "Warning: Empty text provided for chunk processing" << std::endl;
        return false;
    }

    // Проверяем наличие индекса - если нет, создаем
#ifdef USE_FAISS
    if (!external_docs_index_) {
        external_docs_index_ = create_optimized_index(EMBEDDING_DIMENSION);
        if (!external_docs_index_) {
            std::cerr << "Error: Failed to create FAISS index" << std::endl;
            return false;
        }
    }
#endif

    // Проверяем ограничение на размер
    cleanup_old_chunks();

    RagChunk chunk;
    chunk.content = text;
    chunk.document_id = doc_id;
    chunk.chunk_index = chunk_index;

    // Генерируем эмбеддинг
    chunk.embedding = generate_embedding(text);

    // Проверяем, что эмбеддинг был успешно сгенерирован
    if (chunk.embedding.empty()) {
        std::cerr << "Error: Failed to generate embedding for chunk " << chunk_index << " of document " << doc_id << std::endl;
        return false;
    }

    // Добавляем в FAISS индекс
#ifdef USE_FAISS
    if (external_docs_index_) {
        // Преобразуем вектор в формат FAISS
        std::vector<float> vector_for_faiss = chunk.embedding;

        // Проверяем, что размер вектора соответствует размерности индекса
        if (vector_for_faiss.size() != external_docs_index_->d) {
            std::cerr << "Error: Embedding dimension mismatch. Expected: " << external_docs_index_->d
                      << ", got: " << vector_for_faiss.size() << std::endl;
            return false;
        }

        // Нормализуем вектор для косинусного сходства
        normalize_vector(vector_for_faiss);

        // Добавляем в индекс - используем текущий размер КАК ID (перед добавлением)
        try {
            external_docs_index_->add(1, vector_for_faiss.data());
        } catch (const std::exception& e) {
            std::cerr << "Error: Failed to add vector to FAISS index: " << e.what() << std::endl;
            return false;
        }
    } else {
        std::cerr << "Error: external_docs_index_ is null, cannot add chunk" << std::endl;
        return false;
    }
#endif

    external_chunks_.push_back(chunk);
    return true;
}

} // namespace core
} // namespace llama_gui
