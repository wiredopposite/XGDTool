#include <regex>
#include <algorithm>

#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <openssl/sha.h>

#include "Utils/StringUtils.h"
#include "TitleHelper/TitleHelper.h"

#ifndef min
    #define min(a, b) ((a) < (b) ? (a) : (b))
#endif

TitleHelper::TitleHelper(std::shared_ptr<ImageReader> image_reader, bool offline_mode) 
    : offline_mode_(offline_mode), image_reader_(image_reader) 
{
    initialize();
}

TitleHelper::TitleHelper(const std::filesystem::path& in_dir_path, bool offline_mode) 
    : offline_mode_(offline_mode), in_dir_path_(in_dir_path) 
{
    initialize();
}

void TitleHelper::initialize() 
{
    std::unique_ptr<ExeTool> exe_tool{nullptr};

    if (image_reader_)
    {
        exe_tool = std::make_unique<ExeTool>(*image_reader_, image_reader_->executable_entry().path); 
    }
    else
    {
        for (const auto& entry : std::filesystem::directory_iterator(in_dir_path_)) 
        {
            if (entry.is_regular_file() && StringUtils::case_insensitive_search(entry.path().string(), "default.xex")) 
            {
                exe_tool = std::make_unique<ExeTool>(entry.path());
                break;
            } 
            else if (entry.is_regular_file() && StringUtils::case_insensitive_search(entry.path().string(), "default.xbe")) 
            {
                exe_tool = std::make_unique<ExeTool>(entry.path());
                break;
            }
        }
    }

    if (!exe_tool) 
    {
        throw XGDException(ErrCode::MISC, HERE(), "No executable found.");
    }

    xex_cert_ = exe_tool->xex_cert();
    xbe_cert_ = exe_tool->xbe_cert();
    title_id_ = exe_tool->title_id();
    platform_ = exe_tool->platform();

    bool initialized = false;   

    if (!offline_mode_ && internet_connected()) 
    {
        switch (platform_) 
        {
            case Platform::X360:
                initialized = set_x360_titles_online(*exe_tool);
                break;
            case Platform::OGX:
                initialized = set_ogx_titles_online(*exe_tool);
                break;
            default:
                throw XGDException(ErrCode::MISC, HERE(), "Invalid platform.");
        }
    } 

    if (!initialized) 
    {
        initialize_offline(*exe_tool);
    }

    XGDLog(Debug) << "Title information retrieved for: " << title_name_ << XGDLog::Endl;
}

void TitleHelper::initialize_offline(ExeTool& exe_tool) 
{
    if (image_reader_) 
    {
        title_name_ = image_reader_->name();
    }
    else 
    {
        title_name_ = in_dir_path_.filename().string();
    }

    size_t t_pos = min(title_name_.find(" ("), title_name_.find(" ["));
    if (t_pos != std::string::npos) 
    {
        title_name_ = title_name_.substr(0, t_pos);
    }

    clean_title_name(title_name_);

    folder_name_ = title_name_;
    god_folder_name_ = title_name_;
    iso_name_ = title_name_;
    utf16_title_name_ = StringUtils::utf8_to_utf16(title_name_);

    unique_name_ = create_unique_name(exe_tool.xex_cert());
    folder_name_ = folder_name_.substr(0, 42);
    god_folder_name_ = god_folder_name_.substr(0, 31) + " [" + StringUtils::uint32_to_hex_string(exe_tool.title_id()) + "]";
    iso_name_ = iso_name_.substr(0, 36);

    if (utf16_title_name_.size() > 40) {
        utf16_title_name_.resize(40);
    }
}

bool TitleHelper::set_ogx_titles_online(ExeTool& exe_tool) 
{
    std::string url = REPACK_LIST_URL;
    std::string json_string;

    CURL* curl = curl_easy_init();

    if (curl) 
    {
        CURLcode res;
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, rpk_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &json_string);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) 
        {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_cleanup(curl);
    } 
    else 
    {
        std::cerr << "Failed to initialize CURL" << std::endl;
        return false;
    }

    nlohmann::json json = nlohmann::json::parse(json_string);

    std::stringstream title_id_ss;
    title_id_ss << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << exe_tool.xbe_cert().title_id;
    std::string title_id_str = title_id_ss.str();

    std::stringstream version_ss;
    version_ss << std::hex << std::setw(8) << std::setfill('0') << std::uppercase << exe_tool.xbe_cert().cert_version;
    std::string version_str = version_ss.str();

    std::string region_str;

    switch (exe_tool.xbe_cert().region_code) 
    {
        case 0x00000001:
            region_str = "USA";
            break;
        case 0x00000002:
            region_str = "JPN";
            break;
        case 0x00000004:
            region_str = "PAL";
            break;
        case 0x00000007:
            region_str = "GLO";
            break;
        case 0x80000000:
            region_str = "DBG";
            break;
        default:
            break;
    }

    if (!region_str.empty()) 
    {
        for (const auto& entry : json) 
        {
            if (entry["Title ID"] == title_id_str && entry["Version"] == version_str && entry["Region"] == region_str) 
            {
                title_name_ = entry["XBE Title"];
                iso_name_ = entry["ISO Name"];
                folder_name_ = entry["Folder Name"];
                break;
            }
        }
    }
    if (title_name_.empty() && !region_str.empty()) 
    {
        for (const auto& entry : json) 
        {
            if (entry["Title ID"] == title_id_str && entry["Region"] == region_str) 
            {
                title_name_ = entry["XBE Title"];
                iso_name_ = entry["ISO Name"];
                folder_name_ = entry["Folder Name"];
                break;
            }
        }
    }
    if (title_name_.empty()) 
    {
        for (const auto& entry : json) 
        {
            if (entry["Title ID"] == title_id_str) 
            {
                title_name_ = entry["XBE Title"];
                iso_name_ = entry["ISO Name"];
                folder_name_ = entry["Folder Name"];
                break;
            }
        }
    }
    if (title_name_.empty()) 
    {
        return false;
    }

    size_t cut_len = sizeof(" (RGN)") - 1;
    if (title_name_.size() > cut_len) 
    {
        title_name_ = title_name_.substr(0, title_name_.size() - cut_len);
    }

    utf16_title_name_ = StringUtils::utf8_to_utf16(title_name_);
    unique_name_ = create_unique_name(exe_tool.xex_cert());
    god_folder_name_ = title_name_;
    god_folder_name_ = god_folder_name_.substr(0, 31) + " [" + StringUtils::uint32_to_hex_string(title_id_) + "]";

    return true;
}

bool TitleHelper::set_x360_titles_online(ExeTool& exe_tool) 
{
    title_id_ = exe_tool.title_id();

    std::string name = unity_get_title_name(title_id_);
    if (name == "Error") 
    {
        XGDLog(Error) << "Failed to get title name from Unity." << XGDLog::Endl;
        return false;
    }

    clean_title_name(name);

    title_name_      = name;
    iso_name_        = name;
    folder_name_     = name;
    god_folder_name_ = name;

    title_name_      = title_name_.substr(0, 40);
    iso_name_        = iso_name_.substr(0, 36);
    folder_name_     = folder_name_.substr(0, 42);
    god_folder_name_ = god_folder_name_.substr(0, 31) + " [" + StringUtils::uint32_to_hex_string(title_id_) + "]";

    unique_name_     = create_unique_name(exe_tool.xex_cert());
    utf16_title_name_ = StringUtils::utf8_to_utf16(title_name_);

    if (utf16_title_name_.size() > 40) 
    {
        utf16_title_name_.resize(40);
    }

    return true;
}

const std::vector<char>& TitleHelper::title_icon() {
    if (title_icon_data_.empty() && !offline_mode_ && internet_connected()) 
    {
        unity_get_title_icon(title_id_, title_icon_data_);
    }
    return title_icon_data_;
}

bool TitleHelper::internet_connected() 
{
    CURL* curl;
    CURLcode res;
    bool is_connected = false;

    curl = curl_easy_init();

    if (curl) 
    {
        curl_easy_setopt(curl, CURLOPT_URL, "http://www.google.com");
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        res = curl_easy_perform(curl);

        if (res == CURLE_OK) 
        {
            XGDLog(Debug) << "Internet connection detected." << XGDLog::Endl;
            is_connected = true;
        }

        curl_easy_cleanup(curl);
    }

    return is_connected;
}

size_t TitleHelper::rpk_write_callback(void* contents, size_t size, size_t nmemb, std::string* s) 
{
    size_t newLength = size * nmemb;
    try 
    {
        s->append((char*)contents, newLength);
    } 
    catch (std::bad_alloc &e) 
    {
        XGDLog(Error) << "Failed to allocate memory for response: " << e.what() << XGDLog::Endl;
        return 0;
    }
    return newLength;
}

size_t TitleHelper::unity_write_callback(void* contents, size_t size, size_t nmemb, void* userp) 
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string TitleHelper::unity_query(const std::string& title_id) 
{
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();

    if (curl) 
    {
        std::string url = UNITY_URL_PREFIX + title_id;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, unity_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) 
        {
            XGDLog(Error) << "curl_easy_perform() failed: " << curl_easy_strerror(res) << XGDLog::Endl;
        }

        curl_easy_cleanup(curl);
    }

    return readBuffer;
}

std::string TitleHelper::unity_parse_json(const std::string& json_response) 
{
    auto json = nlohmann::json::parse(json_response);

    if (json.contains("MediaIDS") && !json["MediaIDS"].empty()) 
    {
        auto updates = json["MediaIDS"][0]["Updates"];

        if (!updates.empty() && updates[0].contains("Name")) 
        {
            return updates[0]["Name"].get<std::string>();
        }
    }
    return "Error";
}

std::string TitleHelper::unity_get_title_name(uint32_t title_id) 
{
    std::string title_id_str = StringUtils::uint32_to_hex_string(title_id);
    std::string response = unity_query(title_id_str);
    return unity_parse_json(response);
}

size_t TitleHelper::unity_png_write_callback(void* contents, size_t size, size_t nmemb, void* userp) 
{
    size_t total_size = size * nmemb;
    std::vector<char>* vec = static_cast<std::vector<char>*>(userp);
    vec->insert(vec->end(), static_cast<char*>(contents), static_cast<char*>(contents) + total_size);
    return total_size;
}

bool TitleHelper::unity_get_title_icon(uint32_t title_id, std::vector<char>& icon_data) 
{
    XGDLog(Debug) << "Downloading icon for title ID: " << StringUtils::uint32_to_hex_string(title_id) << XGDLog::Endl;

    CURL* curl;
    CURLcode res;
    std::string url = "http://xboxunity.net/Resources/Lib/Icon.php?tid=" + StringUtils::uint32_to_hex_string(title_id);

    curl = curl_easy_init();

    if (curl) 
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, unity_png_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &icon_data);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) 
        {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            curl_easy_cleanup(curl);
            return false;
        }

        curl_easy_cleanup(curl);
        return true;

    }

    XGDLog(Error) << "Failed to initialize curl" << XGDLog::Endl;
    return false;
}

// Games on Demand

template <typename T>
void TitleHelper::write_little_endian(std::ostream& os, T value) 
{
    for (size_t i = 0; i < sizeof(T); ++i) 
    {
        os.put(static_cast<char>(value & 0xFF));
        if (sizeof(T) > 1) 
        {
            value >>= 8;
        }
    }
}

std::string TitleHelper::create_unique_name(const Xex::ExecutionInfo& xex_cert) 
{
    std::ostringstream ss;
    write_little_endian(ss, xex_cert.title_id);
    write_little_endian(ss, xex_cert.media_id);
    write_little_endian(ss, xex_cert.disc_number);
    write_little_endian(ss, xex_cert.disc_count);

    std::string data = ss.str();

    unsigned char hash[SHA_DIGEST_LENGTH];
    SHA1(reinterpret_cast<const unsigned char*>(data.c_str()), data.size(), hash);

    std::ostringstream hex_stream;
    for (size_t i = 0; i < SHA_DIGEST_LENGTH / 2; ++i) 
    {
        hex_stream << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(hash[i]);
    }

    std::string unique_name = hex_stream.str();
    std::transform(unique_name.begin(), unique_name.end(), unique_name.begin(), ::toupper);

    return unique_name;
}

void TitleHelper::clean_title_name(std::string& title) 
{
    std::regex parenr_regex(R"(\(.*?\))");
    std::string result = std::regex_replace(title, parenr_regex, "");
    std::regex space_regex(R"(\s{2,})");
    result = std::regex_replace(result, space_regex, " ");
    title = result;
}