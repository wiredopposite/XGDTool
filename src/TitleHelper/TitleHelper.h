#ifndef _TITLE_HELPER_H_
#define _TITLE_HELPER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <sstream>
#include <memory>

#include "XGD.h"
#include "ImageReader/ImageReader.h"
#include "InputHelper/Types.h"
#include "Executable/ExeTool.h"
#include "Formats/Xex.h"
#include "Formats/Xbe.h"

class TitleHelper 
{
public:
    TitleHelper(std::shared_ptr<ImageReader> image_reader, bool offline_mode);
    TitleHelper(const std::filesystem::path& in_dir_path, bool offline_mode);

    ~TitleHelper() = default;

    const std::string& iso_name() { return iso_name_; };
    const std::string& folder_name() { return folder_name_; };
    const std::string& god_folder_name() { return god_folder_name_; };
    const std::string& unique_name() { return unique_name_; };
    const std::vector<char16_t>& title_name() { return utf16_title_name_; };
    const std::vector<char>& title_icon();

    Platform platform() { return platform_; };
    const Xex::ExecutionInfo& xex_cert() { return xex_cert_; };
    const Xbe::Cert& xbe_cert() { return xbe_cert_; };

private:
    const std::string REPACK_LIST_URL = "https://raw.githubusercontent.com/Team-Resurgent/Repackinator/main/RepackList.json";
    const std::string UNITY_URL_PREFIX = "http://xboxunity.net/Resources/Lib/TitleUpdateInfo.php?titleid=";

    bool offline_mode_{false};

    std::filesystem::path in_dir_path_; 
    std::shared_ptr<ImageReader> image_reader_{nullptr};

    uint32_t title_id_{0};

    std::string title_name_;
    std::string iso_name_;
    std::string folder_name_;
    std::string god_folder_name_;
    std::string unique_name_;
    std::vector<char16_t> utf16_title_name_;
    std::vector<char> title_icon_data_;

    // These are here so we don't need a bunch of ExeTool instances everywhere
    Xex::ExecutionInfo xex_cert_;
    Xbe::Cert xbe_cert_;
    Platform platform_{Platform::UNKNOWN};

    void initialize();  
    void initialize_offline(ExeTool& exe_tool);
    bool set_ogx_titles_online(ExeTool& exe_tool);
    bool set_x360_titles_online(ExeTool& exe_tool);

    template <typename T>
    void write_little_endian(std::ostream& os, T value);
    
    std::string create_unique_name(const Xex::ExecutionInfo& xex_cert);
    void clean_title_name(std::string& title);

    bool internet_connected();

    // XboxUnity
    static size_t unity_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    std::string unity_query(const std::string& title_id);
    std::string unity_parse_json(const std::string& json_response);
    static size_t unity_png_write_callback(void* contents, size_t size, size_t nmemb, void* userp);
    std::string unity_get_title_name(uint32_t title_id);
    bool unity_get_title_icon(uint32_t title_id, std::vector<char>& icon_data);

    // Repackinator
    static size_t rpk_write_callback(void* contents, size_t size, size_t nmemb, std::string* s);
};

#endif // _TITLE_HELPER_H_