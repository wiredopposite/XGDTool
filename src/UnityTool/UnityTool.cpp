#include <sstream>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include "XGD.h"
#include "UnityTool/UnityTool.h"
#include "Common/StringUtils.h"

const std::string UnityTool::err_msg = "Error: No response from server.";

size_t UnityTool::unity_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

bool UnityTool::internet_connected() {
    CURL* curl;
    CURLcode res;
    bool is_connected = false;

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, "http://www.google.com");
        curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
        res = curl_easy_perform(curl);

        if (res == CURLE_OK) {
            XGDLog(Debug) << "Internet connection detected." << XGDLog::Endl;
            is_connected = true;
        }

        curl_easy_cleanup(curl);
    }

    return is_connected;
}

std::string UnityTool::query_xbox_unity(const std::string& title_id) {
    CURL* curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();

    if (curl) {
        std::string url = "http://xboxunity.net/Resources/Lib/TitleUpdateInfo.php?titleid=" + title_id;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, unity_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            XGDLog(Error) << "curl_easy_perform() failed: " << curl_easy_strerror(res) << XGDLog::Endl;
        }

        curl_easy_cleanup(curl);
    }

    return readBuffer;
}

std::string UnityTool::parse_unity_json(const std::string& json_response) {
    auto json = nlohmann::json::parse(json_response);

    if (json.contains("MediaIDS") && !json["MediaIDS"].empty()) {
        auto updates = json["MediaIDS"][0]["Updates"];

        if (!updates.empty() && updates[0].contains("Name")) {
            return updates[0]["Name"].get<std::string>();
        }
    }
    return err_msg;
}

std::string UnityTool::get_title_name(uint32_t title_id) {
    std::string title_id_str = StringUtils::uint32_to_hex_string(title_id);
    std::string response = query_xbox_unity(title_id_str);
    return parse_unity_json(response);
}

size_t UnityTool::unity_png_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total_size = size * nmemb;
    std::vector<char>* vec = static_cast<std::vector<char>*>(userp);
    vec->insert(vec->end(), static_cast<char*>(contents), static_cast<char*>(contents) + total_size);
    return total_size;
}

bool UnityTool::get_title_icon(uint32_t title_id, std::vector<char>& icon_data) {
    CURL* curl;
    CURLcode res;
    std::string url = "http://xboxunity.net/Resources/Lib/Icon.php?tid=" + StringUtils::uint32_to_hex_string(title_id);

    curl = curl_easy_init();

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, unity_png_write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &icon_data);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
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