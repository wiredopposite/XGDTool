#ifndef _UNITY_TOOL_H_
#define _UNITY_TOOL_H_

#include <string>
#include <cstdint>
#include <vector>

class UnityTool {
public:
    const std::string& error() { return err_msg; };
    bool internet_connected();
    std::string get_title_name(uint32_t title_id);
    bool get_title_icon(uint32_t title_id, std::vector<char>& icon_data);
    
private:
    static const std::string err_msg;
    // std::string uint32_to_hex_string(uint32_t value);
    static size_t unity_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    std::string query_xbox_unity(const std::string& title_id);
    std::string parse_unity_json(const std::string& json_response);
    static size_t unity_png_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
};

#endif // _UNITY_TOOL_H_