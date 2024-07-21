#include <sstream>
#include <iostream>
#include <iomanip>
#include <unordered_set>
#include <algorithm>

#include "XGD.h"
#include "Common/StringUtils.h"

namespace StringUtils {

// const std::unordered_set<char> bad_chars = {
//     '*', '?', ':', '"', '\'', '<', '>', '|', ';', '&', '#', '$', '%', '@', '!', '^', '(', ')', '[', ']', '{', '}', '\\', '=', '+', ','
// };

const std::unordered_set<std::string> bad_names = {
    "CON", "PRN", "AUX", "NUL",
    "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7", "COM8", "COM9",
    "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6", "LPT7", "LPT8", "LPT9"
};

bool safe_string(const std::string &string) {
    bool safe = true;
    // for (const char &c : string) {
    //     if (bad_chars.find(c) != bad_chars.end()) {
    //         safe = false;
    //         break;
    //     }
    // }
    for (const std::string &name : bad_names) {
        if (string == name) {
            safe = false;
            break;
        }
    }
    if (string.find("..") != std::string::npos || 
        string.find("./") != std::string::npos || 
        string.find(".\\") != std::string::npos) {
        safe = false;
    }
    return safe;
}

std::string to_lower(const std::string& str) {
    std::string lower_str = str;
    std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
    return lower_str;
}

bool case_insensitive_search(const std::string& original, const std::string& to_find) {
    std::string lower_original = to_lower(original);
    std::string lower_to_find = to_lower(to_find);

    return lower_original.find(lower_to_find) != std::string::npos;
}

std::string utf16_to_utf8(std::u16string utf16_str) {
    std::string utf8_str;
    for (char16_t ch : utf16_str) {
        if (ch == u'\0') break;
        if (ch <= 0x7F) {
            utf8_str += static_cast<char>(ch);
        } else if (ch <= 0x7FF) {
            utf8_str += static_cast<char>(0xC0 | ((ch >> 6) & 0x1F));
            utf8_str += static_cast<char>(0x80 | (ch & 0x3F));
        } else {
            utf8_str += static_cast<char>(0xE0 | ((ch >> 12) & 0x0F));
            utf8_str += static_cast<char>(0x80 | ((ch >> 6) & 0x3F));
            utf8_str += static_cast<char>(0x80 | (ch & 0x3F));
        }
    }
    return utf8_str;
}

std::vector<char16_t> utf8_to_utf16(const std::string& utf8_str) {
    std::vector<char16_t> utf16_vec;
    utf16_vec.reserve(utf8_str.size()); // Reserve space to avoid frequent reallocations

    for (size_t i = 0; i < utf8_str.size();) {
        uint32_t codepoint = 0;
        size_t num_bytes = 0;

        unsigned char ch = static_cast<unsigned char>(utf8_str[i]);
        if (ch <= 0x7F) {
            codepoint = ch;
            num_bytes = 1;
        } else if ((ch & 0xE0) == 0xC0) {
            codepoint = ch & 0x1F;
            num_bytes = 2;
        } else if ((ch & 0xF0) == 0xE0) {
            codepoint = ch & 0x0F;
            num_bytes = 3;
        } else if ((ch & 0xF8) == 0xF0) {
            codepoint = ch & 0x07;
            num_bytes = 4;
        } else {
            throw XGDException(ErrCode::STR_ENCODING, HERE(), "Invalid UTF-8 encoding");
        }

        if (i + num_bytes > utf8_str.size()) {
            throw XGDException(ErrCode::STR_ENCODING, HERE(), "Incomplete UTF-8 sequence");
        }

        for (size_t j = 1; j < num_bytes; ++j) {
            ch = static_cast<unsigned char>(utf8_str[i + j]);
            if ((ch & 0xC0) != 0x80) {
                throw XGDException(ErrCode::STR_ENCODING, HERE(), "Invalid UTF-8 encoding");
            }
            codepoint = (codepoint << 6) | (ch & 0x3F);
        }

        if (codepoint <= 0xFFFF) {
            utf16_vec.push_back(static_cast<char16_t>(codepoint));
        } else {
            codepoint -= 0x10000;
            utf16_vec.push_back(static_cast<char16_t>((codepoint >> 10) + 0xD800));
            utf16_vec.push_back(static_cast<char16_t>((codepoint & 0x3FF) + 0xDC00));
        }

        i += num_bytes;
    }

    return utf16_vec;
}

std::string truncate_utf8(const std::string& input, size_t max_bytes) {
    if (max_bytes == 0) return "";

    size_t current_length = 0;
    size_t char_length = 0;
    size_t last_valid_index = 0;

    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);

        // Determine the length of the current UTF-8 character
        if (c < 0x80) {
            char_length = 1; // ASCII character
        } else if (c < 0xE0) {
            char_length = 2; // 2-byte character
        } else if (c < 0xF0) {
            char_length = 3; // 3-byte character
        } else {
            char_length = 4; // 4-byte character
        }

        // Check if adding this character would exceed the max bytes limit (including null terminator)
        if (current_length + char_length + 1 > max_bytes) {
            break;
        }

        // Update the length and last valid index
        current_length += char_length;
        last_valid_index = i + char_length - 1;

        // Advance the loop by the length of the current character
        i += char_length - 1;
    }

    return input.substr(0, last_valid_index + 1);
}

std::string uint32_to_hex_string(uint32_t value) {
    std::stringstream ss;
    ss << std::hex << std::setw(8) << std::setfill('0') << value;
    
    std::string hex_str = ss.str();
    std::transform(hex_str.begin(), hex_str.end(), hex_str.begin(), ::toupper);
    return hex_str;
}

}; // namespace StringUtils