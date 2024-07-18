#include <cstring>
#include <algorithm>
#include <numeric>

#include "XGD.h"
#include "Common/Utils.h"
#include "ImageReader/XisoReader/XisoReader.h"

XisoReader::XisoReader(const std::vector<std::filesystem::path>& in_xiso_paths) 
    : in_xiso_paths_(in_xiso_paths) {
    
    in_file_ = split::ifstream(in_xiso_paths_);
    if (!in_file_.is_open()) {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), in_xiso_paths_.front().string() + ((in_xiso_paths_.size() > 1) ? (" and " + in_xiso_paths_.back().string()) : ""));
    }

    in_file_.seekg(0, std::ios::end);
    total_sectors_ = static_cast<uint32_t>(in_file_.tellg() / Xiso::SECTOR_SIZE);
    image_offset_ = get_image_offset();

    populate_directory_entries(true);

    XGDLog(Debug) << "Found executable: " << executable_entry_.filename << XGDLog::Endl;

    if (StringUtils::case_insensitive_search(executable_entry_.filename, "default.xbe")) {
        platform_ = Platform::OGX;
    } else if (StringUtils::case_insensitive_search(executable_entry_.filename, "default.xex")) {
        platform_ = Platform::X360;
    }
}

XisoReader::~XisoReader() {
    in_file_.close();
}

std::vector<Xiso::DirectoryEntry>& XisoReader::directory_entries() {
    if (directory_entries_.empty()) {
        populate_directory_entries(false);
    }
    return directory_entries_;
}

uint64_t XisoReader::total_file_bytes() {
    if (total_file_bytes_ == 0) {
        for (const auto& entry : directory_entries()) {
            if (!(entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY)) {
                total_file_bytes_ += entry.header.file_size;
            }
        }
    }
    return total_file_bytes_;
}

std::unordered_set<uint32_t>& XisoReader::data_sectors() {
    if (data_sectors_.empty()) {
        populate_data_sectors();

        auto max_data_sector_it = std::max_element(data_sectors_.begin(), data_sectors_.end());
        max_data_sector_ = (max_data_sector_it != data_sectors_.end()) ? *max_data_sector_it : 0;

        XGDLog(Debug)   << "Max data sector: " 
                        << max_data_sector_ 
                        << "\nResulting image size: " 
                        << (static_cast<uint64_t>(static_cast<uint64_t>(max_data_sector_ + 1) * Xiso::SECTOR_SIZE) - image_offset()) / 0x400 
                        << " KB" << XGDLog::Endl;

        if (platform_ == Platform::OGX) {
            std::unordered_set<uint32_t> security_sectors;
            get_security_sectors(data_sectors_, security_sectors);

            XGDLog(Debug) << "Security sectors: " << security_sectors.size() << XGDLog::Endl;

            for (const auto& sector : security_sectors) {
                data_sectors_.insert(sector);
            }
        }
    }
    return data_sectors_;
}

uint32_t XisoReader::max_data_sector() {
    if (data_sectors_.empty()) {
        data_sectors();
    }
    return max_data_sector_;
}

void XisoReader::read_sector(const uint32_t sector, char* out_buffer) {
    uint64_t position = static_cast<uint64_t>(sector) * static_cast<uint64_t>(Xiso::SECTOR_SIZE);
    in_file_.seekg(position, std::ios::beg);
    in_file_.read(out_buffer, Xiso::SECTOR_SIZE);

    if (in_file_.fail() || in_file_.gcount() != Xiso::SECTOR_SIZE) {
        std::cerr << "Failed to read sector: " << sector 
                  << ", Bytes read: " << in_file_.gcount() 
                  << ", Expected bytes: " << Xiso::SECTOR_SIZE << std::endl;
        throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read sector from input file");
    }
}

void XisoReader::read_bytes(const uint64_t offset, const size_t size, char* out_buffer) {
    in_file_.seekg(offset, std::ios::beg);
    if (in_file_.fail() || in_file_.tellg() != offset) {
        throw XGDException(ErrCode::FILE_SEEK, HERE(), "Failed to seek to offset in input file");
    }
    in_file_.read(out_buffer, size);
    if (in_file_.fail() || in_file_.gcount() != size) {
        throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read bytes from input file");
    }
}

uint64_t XisoReader::get_image_offset() {
    XGDLog(Debug) << "Getting image offset..." << XGDLog::Endl;

    char buffer[Xiso::MAGIC_DATA_LEN];

    const std::vector<int> seek_offsets = {
        0,
        Xiso::LSEEK_OFFSET_GLOBAL,
        Xiso::LSEEK_OFFSET_XGD3,
        Xiso::LSEEK_OFFSET_XGD1
    };

    for (int offset : seek_offsets) {
        in_file_.seekg(Xiso::MAGIC_OFFSET + offset, std::ios::beg);
        in_file_.read(buffer, Xiso::MAGIC_DATA_LEN);

        if (in_file_.fail()) {
            throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read magic data from input file");
        }

        if (std::memcmp(buffer, Xiso::MAGIC_DATA, Xiso::MAGIC_DATA_LEN) == 0) {
            XGDLog(Debug) << "Found XISO magic data, image offset: " << offset << " sector: " << (offset / Xiso::SECTOR_SIZE) << XGDLog::Endl;
            return offset;
        }
    }

    throw XGDException(ErrCode::MISC, HERE(), "Invalid XISO file, failed to find XISO magic data and get image offset");
    return 0;
}

void XisoReader::populate_directory_entries(bool exe_only) {
    XGDLog(Debug) << "Getting " << (exe_only ? "executable entry..." : "directory entries...") << XGDLog::Endl;

    Xiso::DirectoryEntry root_entry;

    in_file_.seekg(image_offset_ + Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN, std::ios::beg);
    in_file_.read(reinterpret_cast<char*>(&root_entry.header.start_sector), sizeof(uint32_t));
    in_file_.read(reinterpret_cast<char*>(&root_entry.header.file_size), sizeof(uint32_t));
    if (in_file_.fail()) {
        throw XGDException(ErrCode::FILE_READ, HERE());
    }

    root_entry.offset = 0;
    root_entry.position = static_cast<uint64_t>(root_entry.header.start_sector) * Xiso::SECTOR_SIZE;
    root_entry.path = "";

    std::vector<Xiso::DirectoryEntry> unprocessed_entries;
    unprocessed_entries.push_back(root_entry);

    while (!unprocessed_entries.empty()) {
        Xiso::DirectoryEntry current_entry = unprocessed_entries.front();
        unprocessed_entries.erase(unprocessed_entries.begin());

        if ((current_entry.offset * sizeof(uint32_t)) >= current_entry.header.file_size) {
            continue;
        }

        uint64_t current_position = image_offset_ + current_entry.position + (current_entry.offset * sizeof(uint32_t));

        Xiso::DirectoryEntry read_entry;
        in_file_.seekg(current_position, std::ios::beg);
        in_file_.read(reinterpret_cast<char*>(&read_entry.header), sizeof(Xiso::DirectoryEntry::Header));

        std::vector<char> buffer(std::min(read_entry.header.name_length, static_cast<uint8_t>(UINT8_MAX)), 0);
        in_file_.read(buffer.data(), buffer.size());
        if (in_file_.fail()) {
            throw XGDException(ErrCode::FILE_READ, HERE(), "File read error");
        }

        read_entry.filename.assign(buffer.data(), buffer.size());

        // XGDLog(Debug) << "Read directory entry: " << read_entry.filename << XGDLog::Endl;

        if (read_entry.header.left_offset == Xiso::PAD_SHORT) {
            continue;
        }

        if (read_entry.header.left_offset != 0) {
            current_entry.offset = static_cast<uint64_t>(read_entry.header.left_offset);
            unprocessed_entries.push_back(current_entry);
        }

        if (exe_only) {
            if (!(read_entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) && read_entry.header.file_size > 0) {
                if (StringUtils::case_insensitive_search(read_entry.filename, "default.xbe")) {
                    read_entry.path = read_entry.filename;
                    executable_entry_ = read_entry;
                    return;
                } else if (StringUtils::case_insensitive_search(read_entry.filename, "default.xex")) {
                    read_entry.path = read_entry.filename;
                    executable_entry_ = read_entry;
                    return;
                }
            }
        } else {
            if ((read_entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) != 0) {
                Xiso::DirectoryEntry dir_entry = read_entry;
                dir_entry.offset = 0;
                dir_entry.position = static_cast<uint64_t>(read_entry.header.start_sector) * Xiso::SECTOR_SIZE;
                dir_entry.path = current_entry.path / read_entry.filename;
                directory_entries_.push_back(dir_entry);

                if (read_entry.header.file_size > 0) {
                    unprocessed_entries.push_back(dir_entry);
                }
            } else if (read_entry.header.file_size > 0) {
                read_entry.path = current_entry.path / read_entry.filename;
                directory_entries_.push_back(read_entry);
            }
        }

        if (read_entry.header.right_offset != 0) {
            current_entry.offset = static_cast<uint64_t>(read_entry.header.right_offset);
            unprocessed_entries.push_back(current_entry);
        }
    }

    if (exe_only) {
        throw XGDException(ErrCode::MISC, HERE(), "No executable found in XISO image");
    }

    if (directory_entries_.empty()) {
        throw XGDException(ErrCode::MISC, HERE(), "No directory entries found in XISO image");
    }

    std::sort(directory_entries_.begin(), directory_entries_.end(), [](const Xiso::DirectoryEntry& a, const Xiso::DirectoryEntry& b) {
        // Directories come before files
        bool a_is_dir = a.header.attributes & Xiso::ATTRIBUTE_DIRECTORY;
        bool b_is_dir = b.header.attributes & Xiso::ATTRIBUTE_DIRECTORY;
        
        if (a_is_dir != b_is_dir) {
            return a_is_dir > b_is_dir;
        }
        // Within the same directory level, sort lexicographically by path
        return a.path < b.path;
    });

    for (const auto& entry : directory_entries_) {
        XGDLog(Debug) << "Directory entry: " << entry.path << XGDLog::Endl;
    }
}

void XisoReader::populate_data_sectors() {
    data_sectors_.clear();

    uint32_t sector_offset = static_cast<uint32_t>(image_offset_ / Xiso::SECTOR_SIZE);
    uint32_t header_sector = sector_offset + static_cast<uint32_t>(Xiso::MAGIC_OFFSET / Xiso::SECTOR_SIZE);

    data_sectors_.insert(header_sector);
    data_sectors_.insert(header_sector + 1);

    Xiso::DirectoryEntry root_entry;

    in_file_.seekg(image_offset_ + Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN, std::ios::beg);
    in_file_.read(reinterpret_cast<char*>(&root_entry.header.start_sector), sizeof(uint32_t));
    in_file_.read(reinterpret_cast<char*>(&root_entry.header.file_size), sizeof(uint32_t));
    if (in_file_.fail()) {
        throw XGDException(ErrCode::FILE_READ, HERE(), "File read error");
    }

    root_entry.offset = 0;
    root_entry.position = static_cast<uint64_t>(root_entry.header.start_sector) * Xiso::SECTOR_SIZE;

    std::vector<Xiso::DirectoryEntry> unprocessed_entries;
    unprocessed_entries.push_back(root_entry);

    XGDLog() << "Getting data sectors..." << XGDLog::Endl;

    while (!unprocessed_entries.empty()) {
        Xiso::DirectoryEntry current_entry = unprocessed_entries.front();
        unprocessed_entries.erase(unprocessed_entries.begin());

        uint64_t current_position = image_offset_ + current_entry.position + (current_entry.offset * sizeof(uint32_t));

        for (auto i = current_position >> 11; i < (current_position >> 11) + ((current_entry.header.file_size - (current_entry.offset * 4) + 2047) >> 11); i++) {
            data_sectors_.insert(static_cast<uint32_t>(i));
        }

        if ((current_entry.offset * 4) >= current_entry.header.file_size) {
            continue;
        }

        Xiso::DirectoryEntry read_entry;

        in_file_.seekg(current_position, std::ios::beg);
        in_file_.read(reinterpret_cast<char*>(&read_entry.header), sizeof(Xiso::DirectoryEntry::Header));

        std::vector<char> buffer(std::min(read_entry.header.name_length, static_cast<uint8_t>(UINT8_MAX)), 0);

        in_file_.read(buffer.data(), buffer.size());
        if (in_file_.fail()) {
            throw XGDException(ErrCode::FILE_READ, HERE(), "File read error");
        }

        read_entry.filename.assign(buffer.data(), buffer.size());
        XGDLog(Debug) << "Read directory entry: " << read_entry.filename << XGDLog::Endl;

        if (read_entry.header.left_offset == Xiso::PAD_SHORT) {
            continue;
        }

        if (read_entry.header.left_offset != 0) {
            current_entry.offset = static_cast<uint64_t>(read_entry.header.left_offset);
            unprocessed_entries.push_back(current_entry);
        }

        if ((read_entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) != 0) {
            if (read_entry.header.file_size > 0) {
                Xiso::DirectoryEntry dir_entry = read_entry;
                dir_entry.offset = 0;
                dir_entry.position = static_cast<uint64_t>(read_entry.header.start_sector) * Xiso::SECTOR_SIZE;
                unprocessed_entries.push_back(dir_entry);
            }
        } else {
            if (read_entry.header.file_size > 0) {
                for (auto i = sector_offset + read_entry.header.start_sector; i < sector_offset + read_entry.header.start_sector + ((read_entry.header.file_size + Xiso::SECTOR_SIZE - 1) / Xiso::SECTOR_SIZE); i++) {
                    data_sectors_.insert(static_cast<uint32_t>(i));
                }
            }
        }

        if (read_entry.header.right_offset != 0) {
            current_entry.offset = static_cast<uint64_t>(read_entry.header.right_offset);
            unprocessed_entries.push_back(current_entry);
        }
    }
}

void XisoReader::get_security_sectors(const std::unordered_set<uint32_t>& data_sectors, std::unordered_set<uint32_t>& out_security_sectors) {
    out_security_sectors.clear();

    bool compare_mode = false;
    uint32_t sector_offset = static_cast<uint32_t>(image_offset_ / Xiso::SECTOR_SIZE);
    const uint32_t end_sector = 0x345B60;
    bool flag = false;
    uint32_t start = 0;
    std::vector<char> sector_buffer(Xiso::SECTOR_SIZE);

    XGDLog() << "Getting security sectors..." << XGDLog::Endl;

    in_file_.seekg(sector_offset * Xiso::SECTOR_SIZE, std::ios::beg);
    
    for (uint32_t sector_index = 0; sector_index <= end_sector; ++sector_index) {
        uint32_t current_sector = sector_offset + sector_index;

        // in_file_.seekg(current_sector * Xiso::SECTOR_SIZE, std::ios::beg);
        in_file_.read(sector_buffer.data(), Xiso::SECTOR_SIZE);
        if (in_file_.fail() || in_file_.gcount() != Xiso::SECTOR_SIZE) {
            throw XGDException(ErrCode::FILE_READ, HERE());
        }

        // bool is_empty_sector = std::accumulate(sector_buffer.begin(), sector_buffer.end(), 0) == 0;
        bool is_data_sector = data_sectors.find(current_sector) != data_sectors.end();
        bool is_empty_sector = true;
        for (auto& byte : sector_buffer) {
            if (byte != 0) {
                is_empty_sector = false;
                break;
            }
        }

        if (is_empty_sector && !flag && !is_data_sector) {
            start = current_sector;
            flag = true;
        } else if (!is_empty_sector && flag) {
            uint32_t end = current_sector - 1;
            flag = false;

            if (end - start == 0xFFF) {
                for (uint32_t i = start; i <= end; ++i) {
                    // if (i == (sector_offset + 2101752)) {
                    //     XGDLog(Debug) << "Security sector: " << i << XGDLog::Endl;
                    // }
                    out_security_sectors.insert(i);
                }
            } else if (compare_mode && (end - start) > 0xFFF) {
                XGDLog().print_progress(100, 100);
                return;
            }
        }

        XGDLog().print_progress(sector_index, end_sector);
    }
}