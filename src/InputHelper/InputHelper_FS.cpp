#include "Common/StringUtils.h"
#include "InputHelper/InputHelper.h"

bool InputHelper::has_extension(const std::filesystem::path& path, const std::string& extension) 
{
    if (path.filename().string().size() > extension.size()) 
    {
        return StringUtils::case_insensitive_search(path.filename().string(), extension);
    }
    return false;
}

bool InputHelper::is_extracted_dir(const std::filesystem::path& path) 
{
    for (const auto& entry : std::filesystem::directory_iterator(path)) 
    {
        if (entry.is_regular_file()) 
        {
            if (has_extension(entry.path(), ".xbe")) 
            {
                return true;
            }
            else if (has_extension(entry.path(), ".xex")) 
            {
                return true;
            }
        }
    }
    return false;
}

bool InputHelper::is_god_dir_helper(const std::filesystem::path& path, int current_depth, int max_depth) {
    if (current_depth > max_depth) 
    {
        return false;
    }

    for (const auto& entry : std::filesystem::directory_iterator(path)) 
    {
        if (entry.is_regular_file() && entry.path().filename().string().rfind("Data", 0) == 0) 
        {
            if (entry.path().parent_path().extension() == ".data") 
            {
                return true;
            }
        } 
        else if (entry.is_directory()) 
        {
            if (is_god_dir_helper(entry.path(), current_depth + 1, max_depth)) 
            {
                return true;
            }
        }
    }
    return false;
}

bool InputHelper::is_god_dir(const std::filesystem::path& path) 
{
    return is_god_dir_helper(path, 0, 3);
}

FileType InputHelper::get_filetype(const std::filesystem::path& path) 
{
    if (std::filesystem::is_directory(path)) 
    {
        if (is_extracted_dir(path)) 
        {
            return FileType::DIR;
        } 
        else if (is_god_dir(path)) 
        {
            return FileType::GoD;
        }
    }
    else if (std::filesystem::is_regular_file(path))
    {
        if (has_extension(path, ".iso")) 
        {
            return FileType::ISO;
        } 
        else if (has_extension(path, ".zar")) 
        {
            return FileType::ZAR;
        } 
        else if (has_extension(path, ".cso")) 
        {
            return FileType::CSO;
        } 
        else if (has_extension(path, ".cci")) 
        {
            return FileType::CCI;
        } 
        else if (has_extension(path, ".xbe")) 
        {
            return FileType::XBE;
        }
    }
    return FileType::UNKNOWN;
}

bool InputHelper::is_batch_dir(const std::filesystem::path& path) 
{
    for (const auto& entry : std::filesystem::directory_iterator(path)) 
    {
        if (get_filetype(entry.path()) != FileType::UNKNOWN) 
        {
            return true;
        }
    }
    return false;
}

std::vector<std::filesystem::path> InputHelper::find_split_filepaths(const std::filesystem::path& in_filepath) 
{
    std::string extension = in_filepath.extension().string();
    std::filesystem::path stem = in_filepath.stem();
    std::string stem_str = stem.string();

    if (stem_str.size() > 2) 
    {
        std::string subextension = stem_str.substr(stem_str.size() - 2);
        if (subextension == ".1" || subextension == ".2") 
        {
            std::string base_filename = stem_str.substr(0, stem_str.size() - 2);
            std::filesystem::path base_path = in_filepath.parent_path() / base_filename;

            if (subextension == ".1") 
            {
                std::filesystem::path in_filepath1 = in_filepath;
                std::filesystem::path in_filepath2 = base_path.string() + ".2" + extension;
                if (std::filesystem::exists(in_filepath2)) 
                {
                    return { in_filepath1, in_filepath2 };
                }
            } 
            else if (subextension == ".2") 
            {
                std::filesystem::path in_filepath1 = base_path.string() + ".1" + extension;
                std::filesystem::path in_filepath2 = in_filepath;
                if (std::filesystem::exists(in_filepath1)) 
                {
                    return { in_filepath1, in_filepath2 };
                } 
                else 
                {
                    throw XGDException(ErrCode::FS_EXISTS, HERE(), "Missing first part of split file: " + in_filepath.string());
                }
            }
        }
    }
    return { in_filepath };
}