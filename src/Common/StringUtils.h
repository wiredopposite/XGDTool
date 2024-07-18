#ifndef _STRING_UTILS_H_
#define _STRING_UTILS_H_

#include <string>
#include <vector>

namespace StringUtils {
    bool safe_string(const std::string &string);
    std::string to_lower(const std::string& str);
    bool case_insensitive_search(const std::string& original, const std::string& to_find);
    std::string utf16_to_utf8(std::u16string utf16_str);
    std::vector<char16_t> utf8_to_utf16(const std::string& utf8_str);
    std::string truncate_utf8(const std::string& input, std::size_t max_bytes);
    std::string uint32_to_hex_string(uint32_t value);
};

#endif // _STRING_UTILS_H_