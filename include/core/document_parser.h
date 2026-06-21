#pragma once

#include <string>
#include <vector>

namespace llama_gui {
namespace core {

class DocumentParser {
public:
    static std::vector<std::string> parse_txt(const std::string& file_path);
    static std::vector<std::string> parse_pdf(const std::string& file_path);
    static std::vector<std::string> parse_docx(const std::string& file_path);
    static std::vector<std::string> parse_md(const std::string& file_path);

    // Универсальный метод для определения типа файла и его парсинга
    static std::vector<std::string> parse_document(const std::string& file_path);

private:
    static std::string get_file_extension(const std::string& file_path);
};

} // namespace core
} // namespace llama_gui