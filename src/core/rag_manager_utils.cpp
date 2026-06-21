#include "../include/core/rag_manager.h"
#include <cmath>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <regex>

namespace llama_gui {
namespace core {

void RagManager::normalize_vector(std::vector<float>& vec) {
    float norm = 0.0f;
    for (float val : vec) {
        norm += val * val;
    }
    norm = std::sqrt(norm);
    if (norm > 0.0f) {
        for (float& val : vec) {
            val /= norm;
        }
    }
}

std::vector<std::string> RagManager::split_into_chunks(const std::string& text, int max_tokens) {
    std::vector<std::string> chunks;
    
    if (text.empty()) {
        return chunks;
    }

    // Шаг 1: Разбиваем на абзацы
    std::vector<std::string> paragraphs;
    size_t start = 0;
    size_t pos = 0;
    
    while ((pos = text.find("\n\n", start)) != std::string::npos) {
        std::string para = text.substr(start, pos - start);
        // Убираем лишние пробелы
        while (!para.empty() && (para.back() == '\n' || para.back() == ' ' || para.back() == '\t')) {
            para.pop_back();
        }
        if (!para.empty()) {
            paragraphs.push_back(para);
        }
        start = pos + 2;
        // Пропускаем все переводы строк
        while (start < text.size() && (text[start] == '\n' || text[start] == ' ' || text[start] == '\t')) {
            start++;
        }
    }
    // Последний абзац
    std::string last_para = text.substr(start);
    while (!last_para.empty() && (last_para.back() == '\n' || last_para.back() == ' ' || last_para.back() == '\t')) {
        last_para.pop_back();
    }
    if (!last_para.empty()) {
        paragraphs.push_back(last_para);
    }

    // Шаг 2: Каждый абзац разбиваем на предложения
    std::vector<std::string> sentences;
    for (const auto& para : paragraphs) {
        std::vector<std::string> para_sentences = split_into_sentences(para);
        sentences.insert(sentences.end(), para_sentences.begin(), para_sentences.end());
    }

    // Шаг 3: Группируем предложения в чанки с overlap
    const int overlap_sentences = 1;  // 1 предложение overlap
    std::string current_chunk;
    std::vector<std::string> recent_sentences;  // Для overlap
    
    for (size_t i = 0; i < sentences.size(); ++i) {
        const auto& sentence = sentences[i];
        
        // Проверяем размер
        int estimated_tokens = count_tokens_approx(current_chunk + " " + sentence);
        
        if (estimated_tokens <= max_tokens || current_chunk.empty()) {
            // Добавляем в текущий чанк
            if (!current_chunk.empty()) {
                current_chunk += " ";
            }
            current_chunk += sentence;
        } else {
            // Чанк заполнен
            if (!current_chunk.empty()) {
                chunks.push_back(current_chunk);
            }
            
            // Начинаем новый чанк с overlap
            current_chunk = "";
            for (size_t j = std::max<int>(0, recent_sentences.size() - overlap_sentences); 
                 j < recent_sentences.size(); ++j) {
                if (!current_chunk.empty()) {
                    current_chunk += " ";
                }
                current_chunk += recent_sentences[j];
            }
            
            // Добавляем текущее предложение
            if (!current_chunk.empty()) {
                current_chunk += " ";
            }
            current_chunk += sentence;
        }
        
        // Сохраняем для overlap
        recent_sentences.push_back(sentence);
        // Ограничиваем историю
        if (recent_sentences.size() > 5) {
            recent_sentences.erase(recent_sentences.begin());
        }
    }

    // Добавляем последний чанк
    if (!current_chunk.empty()) {
        chunks.push_back(current_chunk);
    }

    return chunks;
}

std::vector<std::string> RagManager::split_into_sentences(const std::string& text) {
    std::vector<std::string> sentences;
    
    if (text.empty()) {
        return sentences;
    }

    // Разбиваем по предложениям: . ! ? \n
    // Учитываем многоточие "...", инициалы "И.И.", и т.д.
    std::string current_sentence;
    bool in_abbreviation = false;
    
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        current_sentence += c;
        
        // Проверяем конец предложения
        bool is_sentence_end = (c == '.' || c == '!' || c == '?');
        
        if (is_sentence_end) {
            // Проверяем, не многоточие ли это
            if (c == '.' && i + 1 < text.size() && text[i + 1] == '.') {
                // Многоточие - продолжаем
                continue;
            }
            
            // Проверяем, не инициал ли это (одна буква с точкой)
            if (c == '.' && current_sentence.size() >= 2) {
                // Ищем последнюю букву перед точкой
                size_t last_alpha = current_sentence.size() - 2;
                while (last_alpha > 0 && !std::isalpha(current_sentence[last_alpha])) {
                    last_alpha--;
                }
                
                // Если между последней буквой и точкой только одна позиция - это инициал
                if (std::isalpha(current_sentence[last_alpha]) && 
                    (current_sentence.size() - 2 - last_alpha) <= 1) {
                    // Это инициал, не конец предложения
                    continue;
                }
            }
            
            // Это конец предложения
            // Убираем лишние пробелы
            while (!current_sentence.empty() && 
                   (current_sentence.back() == ' ' || current_sentence.back() == '\t')) {
                current_sentence.pop_back();
            }
            
            if (!current_sentence.empty()) {
                sentences.push_back(current_sentence);
            }
            current_sentence = "";
            
            // Пропускаем пробелы после предложения
            while (i + 1 < text.size() && 
                   (text[i + 1] == ' ' || text[i + 1] == '\t' || text[i + 1] == '\n')) {
                i++;
            }
        }
    }
    
    // Добавляем остаток
    while (!current_sentence.empty() && 
           (current_sentence.back() == ' ' || current_sentence.back() == '\t')) {
        current_sentence.pop_back();
    }
    if (!current_sentence.empty()) {
        sentences.push_back(current_sentence);
    }
    
    return sentences;
}

int RagManager::count_tokens_approx(const std::string& text) {
    if (text.empty()) {
        return 0;
    }
    
    // Улучшенный подсчёт токенов для BGE-M3 и подобных моделей
    // BGE-M3 использует SentencePiece, примерно:
    // - 1 токен ≈ 3-4 символа для кириллицы
    // - 1 токен ≈ 1 слово для английского
    // - Пунктуация = отдельные токены
    
    int token_count = 0;
    int utf8_char_count = 0;
    bool in_word = false;
    
    for (size_t i = 0; i < text.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(text[i]);
        
        // Пробелы = разделители токенов
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            if (in_word) {
                token_count++;
                in_word = false;
            }
            continue;
        }
        
        // Пунктуация = отдельные токены
        if (c == '.' || c == ',' || c == '!' || c == '?' || c == ';' || c == ':' ||
            c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' ||
            c == '"' || c == '\'' || c == '-' || c == '/' || c == '\\') {
            if (in_word) {
                token_count++;  // Закрываем текущее слово
                in_word = false;
            }
            token_count++;  // Пунктуация = отдельный токен
            continue;
        }
        
        // UTF-8 символы (кириллица и другие)
        if (c >= 0x80) {
            // Это начало UTF-8 последовательности
            if (!in_word) {
                in_word = true;
                utf8_char_count = 0;
            }
            
            // Считаем байты (кириллица = 2 байта)
            utf8_char_count++;
            
            // Для кириллицы: ~2 символа = 1 токен
            if (utf8_char_count >= 2) {
                token_count++;
                utf8_char_count = 0;
                in_word = false;
            }
            continue;
        }
        
        // Обычные ASCII буквы
        if (!in_word) {
            in_word = true;
        }
    }
    
    // Закрываем последнее слово
    if (in_word) {
        token_count++;
    }
    
    // Минимум 1 токен
    return std::max(1, token_count);
}

std::string RagManager::get_file_extension(const std::string& file_path) {
    size_t pos = file_path.find_last_of('.');
    if (pos != std::string::npos) {
        return file_path.substr(pos);
    }
    return "";
}

std::string RagManager::read_txt_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

} // namespace core
} // namespace llama_gui
