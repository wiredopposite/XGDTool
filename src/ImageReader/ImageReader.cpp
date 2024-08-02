#include <algorithm>

#include "ImageReader/XisoReader/XisoReader.h"
#include "ImageReader/CCIReader/CCIReader.h"
#include "ImageReader/GoDReader/GoDReader.h"
#include "ImageReader/CSOReader/CSOReader.h"
#include "ImageReader/ImageReader.h"
#include "XGD.h"
#include "Utils/StringUtils.h"

std::shared_ptr<ImageReader> ImageReader::create_instance(FileType in_file_type, const std::vector<std::filesystem::path>& paths) 
{
    switch (in_file_type) 
    {
        case FileType::CCI:
            return std::make_shared<CCIReader>(paths);
        case FileType::ISO:
            return std::make_shared<XisoReader>(paths);
        case FileType::GoD:
            return std::make_shared<GoDReader>(paths);
        case FileType::CSO:
            return std::make_shared<CSOReader>(paths);
        default:
            throw XGDException(ErrCode::ISO_INVALID, HERE(), "Unsupported ImageReader file type");
    }
}

const std::vector<Xiso::DirectoryEntry>& ImageReader::directory_entries() 
{
    if (directory_entries_.empty()) 
    {
        populate_directory_entries(false);
    }
    return directory_entries_;
}

const Xiso::DirectoryEntry& ImageReader::executable_entry() 
{
    if (executable_entry_.path.empty()) 
    {
        populate_directory_entries(true);
    }
    return executable_entry_;
}

Platform ImageReader::platform() 
{
    if (platform_ == Platform::UNKNOWN) 
    {
        if (StringUtils::case_insensitive_search(executable_entry().filename, ".xex"))
        {
            platform_ = Platform::X360;
        }
        else if (StringUtils::case_insensitive_search(executable_entry().filename, ".xbe"))
        {
            platform_ = Platform::OGX;
        }
        else
        {
            throw XGDException(ErrCode::MISC, HERE(), "Unknown platform");
        }
    }
    return platform_;
}

Xiso::FileTime ImageReader::file_time() 
{
    read_bytes( image_offset() + Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN + sizeof(uint32_t) * 2, 
                sizeof(Xiso::FileTime), 
                reinterpret_cast<char*>(&file_time_));

    return file_time_;
}

uint64_t ImageReader::total_file_bytes() 
{
    if (total_file_bytes_ == 0) 
    {
        for (const auto& entry : directory_entries()) 
        {
            if (!(entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY)) 
            {
                total_file_bytes_ += entry.header.file_size;
            }
        }
    }
    return total_file_bytes_;
}

uint32_t ImageReader::max_data_sector() 
{
    if (data_sectors_.empty()) 
    {
        data_sectors();
    }
    return max_data_sector_;
}

const std::unordered_set<uint32_t>& ImageReader::data_sectors() 
{
    if (!data_sectors_.empty()) 
    {
        return data_sectors_;
    }

    populate_data_sectors();

    auto max_data_sector_it = std::max_element(data_sectors_.begin(), data_sectors_.end());
    max_data_sector_ = (max_data_sector_it != data_sectors_.end()) ? *max_data_sector_it : 0;

    std::unordered_set<uint32_t> security_sectors;

    if (platform() == Platform::OGX && get_security_sectors(security_sectors)) 
    {
        for (const auto& sector : security_sectors) 
        {
            data_sectors_.insert(sector);
        }
    }
    else //Image is either Xbox 360 or already scrubbed/rewritten
    {
        for (uint32_t i = static_cast<uint32_t>(image_offset() / Xiso::SECTOR_SIZE); i < max_data_sector_; ++i) 
        {
            data_sectors_.insert(i);
        } 
    }
    return data_sectors_;
}

void ImageReader::populate_directory_entries(bool exe_only) 
{
    directory_entries_.clear();

    Xiso::DirectoryEntry root_entry;
    read_bytes(image_offset() + Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN, sizeof(uint32_t) * 2, reinterpret_cast<char*>(&root_entry.header.start_sector));

    root_entry.offset = 0;
    root_entry.position = static_cast<uint64_t>(root_entry.header.start_sector) * Xiso::SECTOR_SIZE;
    root_entry.path = "";

    std::vector<Xiso::DirectoryEntry> unprocessed_entries;
    unprocessed_entries.push_back(root_entry);

    while (!unprocessed_entries.empty())
    {
        Xiso::DirectoryEntry current_entry = unprocessed_entries.front();
        unprocessed_entries.erase(unprocessed_entries.begin());

        if ((current_entry.offset * sizeof(uint32_t)) >= current_entry.header.file_size) 
        {
            continue;
        }

        uint64_t current_position = image_offset() + current_entry.position + (current_entry.offset * sizeof(uint32_t));

        Xiso::DirectoryEntry read_entry;
        read_bytes(current_position, sizeof(Xiso::DirectoryEntry::Header), reinterpret_cast<char*>(&read_entry.header));

        std::vector<char> filename_buffer(std::min(read_entry.header.name_length, static_cast<uint8_t>(UINT8_MAX)));
        read_bytes(current_position + sizeof(Xiso::DirectoryEntry::Header), filename_buffer.size(), filename_buffer.data());
        read_entry.filename.assign(filename_buffer.data(), filename_buffer.size());

        if (read_entry.header.left_offset == Xiso::PAD_BYTE)
        {
            continue;
        }

        if (read_entry.header.left_offset != 0)
        {
            current_entry.offset = static_cast<uint64_t>(read_entry.header.left_offset);
            unprocessed_entries.push_back(current_entry);
        }

        if (exe_only)
        {
            if (!(read_entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) && read_entry.header.file_size > 0)
            {
                if (StringUtils::case_insensitive_search(read_entry.filename, "default.xex") ||
                    StringUtils::case_insensitive_search(read_entry.filename, "default.xbe"))
                {
                    read_entry.path = read_entry.filename;
                    executable_entry_ = read_entry;
                    return;
                } 
            }
        }
        else
        {
            if (read_entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY)
            {
                Xiso::DirectoryEntry dir_entry = read_entry;
                dir_entry.offset = 0;
                dir_entry.position = static_cast<uint64_t>(read_entry.header.start_sector) * Xiso::SECTOR_SIZE;
                dir_entry.path = current_entry.path / read_entry.filename;
                directory_entries_.push_back(dir_entry);

                if (read_entry.header.file_size > 0) 
                {
                    unprocessed_entries.push_back(dir_entry);
                }  
            }
            else if (read_entry.header.file_size > 0) 
            {
                read_entry.path = current_entry.path / read_entry.filename;
                directory_entries_.push_back(read_entry);
            }
        }

        if (read_entry.header.right_offset != 0) 
        {
            current_entry.offset = static_cast<uint64_t>(read_entry.header.right_offset);
            unprocessed_entries.push_back(current_entry);
        }
    }

    if (exe_only) 
    {
        throw XGDException(ErrCode::MISC, HERE(), "No executable found in GoD image");
    }

    std::sort(directory_entries_.begin(), directory_entries_.end(), [](const Xiso::DirectoryEntry& a, const Xiso::DirectoryEntry& b) 
    {
        bool a_is_dir = a.header.attributes & Xiso::ATTRIBUTE_DIRECTORY;
        bool b_is_dir = b.header.attributes & Xiso::ATTRIBUTE_DIRECTORY;
        
        if (a_is_dir != b_is_dir) 
        {
            return a_is_dir > b_is_dir;
        }

        return a.path < b.path;
    });
}

void ImageReader::populate_data_sectors() 
{
    data_sectors_.clear();

    uint32_t sector_offset = static_cast<uint32_t>(image_offset() / Xiso::SECTOR_SIZE);
    uint32_t header_sector = sector_offset + static_cast<uint32_t>(Xiso::MAGIC_OFFSET / Xiso::SECTOR_SIZE);

    data_sectors_.insert(header_sector);
    data_sectors_.insert(header_sector + 1);

    Xiso::DirectoryEntry root_entry;
    read_bytes(image_offset() + Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN, sizeof(uint32_t) * 2, reinterpret_cast<char*>(&root_entry.header.start_sector));

    root_entry.offset = 0;
    root_entry.position = static_cast<uint64_t>(root_entry.header.start_sector) * Xiso::SECTOR_SIZE;

    std::vector<Xiso::DirectoryEntry> unprocessed_entries;
    unprocessed_entries.push_back(root_entry);

    XGDLog() << "Reading data sectors" << XGDLog::Endl;

    while (!unprocessed_entries.empty()) 
    {
        Xiso::DirectoryEntry current_entry = unprocessed_entries.front();
        unprocessed_entries.erase(unprocessed_entries.begin());

        uint64_t current_position = image_offset() + current_entry.position + (current_entry.offset * sizeof(uint32_t));

        for (auto i = current_position >> 11; i < (current_position >> 11) + ((current_entry.header.file_size - (current_entry.offset * 4) + 2047) >> 11); i++) 
        {
            data_sectors_.insert(static_cast<uint32_t>(i));
        }

        if ((current_entry.offset * 4) >= current_entry.header.file_size) 
        {
            continue;
        }

        Xiso::DirectoryEntry read_entry;
        read_bytes(current_position, sizeof(Xiso::DirectoryEntry::Header), reinterpret_cast<char*>(&read_entry.header));

        if (read_entry.header.left_offset == Xiso::PAD_SHORT) 
        {
            continue;
        }

        if (read_entry.header.left_offset != 0) 
        {
            current_entry.offset = static_cast<uint64_t>(read_entry.header.left_offset);
            unprocessed_entries.push_back(current_entry);
        }

        if ((read_entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) != 0) 
        {
            if (read_entry.header.file_size > 0) 
            {
                Xiso::DirectoryEntry dir_entry = read_entry;
                dir_entry.offset = 0;
                dir_entry.position = static_cast<uint64_t>(read_entry.header.start_sector) * Xiso::SECTOR_SIZE;
                unprocessed_entries.push_back(dir_entry);
            }
        } 
        else 
        {
            if (read_entry.header.file_size > 0) 
            {
                for (auto i = sector_offset + read_entry.header.start_sector; i < sector_offset + read_entry.header.start_sector + ((read_entry.header.file_size + Xiso::SECTOR_SIZE - 1) / Xiso::SECTOR_SIZE); i++) 
                {
                    data_sectors_.insert(static_cast<uint32_t>(i));
                }
            }
        }

        if (read_entry.header.right_offset != 0) 
        {
            current_entry.offset = static_cast<uint64_t>(read_entry.header.right_offset);
            unprocessed_entries.push_back(current_entry);
        }
    }
}

bool ImageReader::get_security_sectors(std::unordered_set<uint32_t>& out_security_sectors) 
{
    out_security_sectors.clear();

    if (total_sectors() != Xiso::REDUMP_GAME_SECTORS && total_sectors() != Xiso::REDUMP_TOTAL_SECTORS)
    {
        return false;
    }

    bool compare_mode = false;
    uint32_t sector_offset = static_cast<uint32_t>(image_offset() / Xiso::SECTOR_SIZE);
    bool flag = false;
    uint32_t start = 0;
    const uint32_t end_sector = 0x345B60;

    std::vector<char> sector_buffer(Xiso::SECTOR_SIZE);

    XGDLog() << "Reading security sectors" << XGDLog::Endl;
    
    for (uint32_t sector_index = 0; sector_index <= end_sector; ++sector_index) 
    {
        uint32_t current_sector = sector_offset + sector_index;
        read_sector(current_sector, sector_buffer.data());

        bool is_data_sector = data_sectors_.find(current_sector) != data_sectors_.end();
        bool is_empty_sector = true;
        for (auto& byte : sector_buffer) 
        {
            if (byte != 0) 
            {
                is_empty_sector = false;
                break;
            }
        }

        if (is_empty_sector && !flag && !is_data_sector) 
        {
            start = current_sector;
            flag = true;
        } 
        else if (!is_empty_sector && flag) 
        {
            uint32_t end = current_sector - 1;
            flag = false;

            if (end - start == 0xFFF) 
            {
                for (uint32_t i = start; i <= end; ++i) 
                {
                    out_security_sectors.insert(i);
                }
            } 
            else if (compare_mode && (end - start) > 0xFFF) 
            {
                XGDLog().print_progress(100, 100);
                return false;
            }
        }

        XGDLog().print_progress(sector_index, end_sector);
    }
    return true;
}