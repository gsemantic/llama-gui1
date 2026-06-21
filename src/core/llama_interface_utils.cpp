#include "../include/core/llama_interface.h"
#include <algorithm>
#include <cctype>
#include <iostream>

namespace llama_gui {
namespace core {

bool LlamaInterface::is_server_healthy() const {
    try {
        json response = make_http_request(server_url_ + "/health", "GET", {});
        return !response.empty() && !response.contains("error");
    } catch (const std::exception& e) {
        std::cerr << "Health check failed: " << e.what() << std::endl;
        return false;
    }
}

json LlamaInterface::get_server_info() const {
    try {
        json response = make_http_request(server_url_ + "/props", "GET", {});
        if (!response.empty() && response.contains("data")) {
            return response["data"];
        }
        return response;
    } catch (const std::exception& e) {
        std::cerr << "Failed to get server info: " << e.what() << std::endl;
        return json{};
    }
}

json LlamaInterface::get_models() const {
    try {
        json response = make_http_request(server_url_ + "/v1/models", "GET", {});
        if (!response.empty() && response.contains("data")) {
            return response["data"];
        }
        return json::array();
    } catch (const std::exception& e) {
        std::cerr << "Failed to get models: " << e.what() << std::endl;
        return json::array();
    }
}

json LlamaInterface::get_slots_status() const {
    try {
        json response = make_http_request(server_url_ + "/slots", "GET", {});
        return response;
    } catch (const std::exception& e) {
        std::cerr << "Failed to get slots status: " << e.what() << std::endl;
        return json{};
    }
}

std::string LlamaInterface::validate_and_clean_utf8(const std::string& input) {
    std::string output;
    output.reserve(input.size());

    for (size_t i = 0; i < input.size(); ) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        // Check for valid UTF-8 start bytes
        if (c <= 0x7F) {
            // 1-byte character (0xxxxxxx) - ASCII
            // Allow only printable ASCII (32-126) and whitespace (9, 10, 11, 12, 13, 32)
            // Filter out ALL control characters (0x00-0x1F except 9, 10, 11, 12, 13) and DEL (0x7F)
            if ((c >= 32 && c <= 126) || c == '\t' || c == '\n' || c == '\v' || c == '\f' || c == '\r') {
                output += c;
            }
            // Skip control characters silently
            i++;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte character (110xxxxx 10xxxxxx)
            if (i + 1 < input.size() && (input[i+1] & 0xC0) == 0x80) {
                // Check for overlong encoding (invalid)
                if ((c & 0x1E) != 0) { // Not 1100000x (overlong)
                    output += input.substr(i, 2);
                } else {
                    // Overlong encoding - invalid, replace with space
                    output += ' ';
                }
                i += 2;
            } else {
                // Invalid sequence, replace with space
                output += ' ';
                i++;
            }
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte character (1110xxxx 10xxxxxx 10xxxxxx)
            if (i + 2 < input.size() &&
                (input[i+1] & 0xC0) == 0x80 &&
                (input[i+2] & 0xC0) == 0x80) {
                // Check for overlong encoding and surrogate pairs
                if ((c != 0xE0 || (input[i+1] & 0xE0) != 0x80) && // Not overlong
                    (c < 0xED || c > 0xED) || // Not surrogate
                    (c == 0xED && (input[i+1] & 0xE0) != 0xA0)) { // Valid surrogate
                    output += input.substr(i, 3);
                } else {
                    // Invalid surrogate or overlong, replace with space
                    output += ' ';
                }
                i += 3;
            } else {
                // Invalid sequence, replace with space
                output += ' ';
                i++;
            }
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte character (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
            if (i + 3 < input.size() &&
                (input[i+1] & 0xC0) == 0x80 &&
                (input[i+2] & 0xC0) == 0x80 &&
                (input[i+3] & 0xC0) == 0x80) {
                // Check for valid range (not beyond U+10FFFF)
                if (c <= 0xF4 && // Not beyond 11110100
                    (c < 0xF4 || (c == 0xF4 && input[i+1] <= 0x8F))) { // Valid range
                    output += input.substr(i, 4);
                } else {
                    // Invalid range, replace with space
                    output += ' ';
                }
                i += 4;
            } else {
                // Invalid sequence, replace with space
                output += ' ';
                i++;
            }
        } else {
            // Invalid UTF-8 start byte, replace with space
            output += ' ';
            i++;
        }
    }

    return output;
}

json LlamaInterface::extract_json_from_response(const std::string& response) {
    // Try to find JSON boundaries in the response
    size_t start = response.find('{');
    if (start == std::string::npos) {
        start = response.find('[');
    }

    if (start != std::string::npos) {
        // Find matching end bracket
        int bracket_count = 0;
        size_t end = start;
        char start_char = response[start];
        char end_char = (start_char == '{') ? '}' : ']';

        for (size_t i = start; i < response.size(); i++) {
            if (response[i] == start_char) {
                bracket_count++;
            } else if (response[i] == end_char) {
                bracket_count--;
                if (bracket_count == 0) {
                    end = i + 1;
                    break;
                }
            }
        }

        if (end > start) {
            std::string json_str = response.substr(start, end - start);
            try {
                return json::parse(json_str);
            } catch (const std::exception& e) {
                std::cerr << "Failed to parse extracted JSON: " << e.what() << std::endl;
            }
        }
    }

    return json{};
}

} // namespace core
} // namespace llama_gui
