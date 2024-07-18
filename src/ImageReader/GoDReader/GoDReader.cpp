#include <cstring>
#include <algorithm>
#include <numeric>

#include "XGD.h"
#include "Common/Utils.h"
#include "ImageReader/GoDReader/GoDReader.h"

GoDReader::GoDReader(const std::vector<std::filesystem::path>& in_god_directory) 
    : in_god_directory_(in_god_directory.front()) {

    // in_god_directory_ = in_god_directory.front();
    populate_data_files(in_god_directory_, 5);

    auto last_blocks = std::filesystem::file_size(in_god_data_paths_.back()) / GoD::BLOCK_SIZE;
    auto last_hash_index = (last_blocks - 1) / (GoD::DATA_BLOCKS_PER_SHT + 1);
    auto last_data_blocks = last_blocks - (last_hash_index + 1);
    auto total_data_blocks = ((in_god_data_paths_.size() - 1) * GoD::DATA_BLOCKS_PER_PART) + last_blocks;
    total_sectors_ = static_cast<uint32_t>(total_data_blocks * (GoD::BLOCK_SIZE / Xiso::SECTOR_SIZE));

    for (const auto& data_path : in_god_data_paths_) {
        in_files_.push_back(std::make_unique<std::ifstream>(data_path, std::ios::binary));
        if (!in_files_.back()->is_open()) {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), data_path.string());
        }
    }

    populate_directory_entries(true);

    if (StringUtils::case_insensitive_search(executable_entry_.filename, "default.xbe")) {
        platform_ = Platform::OGX;
    } else if (StringUtils::case_insensitive_search(executable_entry_.filename, "default.xex")) {
        platform_ = Platform::X360;
    }
}

GoDReader::~GoDReader() {
    for (auto& in_file : in_files_) {
        in_file->close();
    }
}

const std::vector<Xiso::DirectoryEntry>& GoDReader::directory_entries() {
    if (directory_entries_.empty()) {
        populate_directory_entries(false);
    }
    return directory_entries_;
}

uint64_t GoDReader::total_file_bytes() {
    if (total_file_bytes_ == 0) {
        for (const auto& entry : directory_entries()) {
            if (!(entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY)) {
                total_file_bytes_ += entry.header.file_size;
            }
        }
    }
    return total_file_bytes_;
}

const std::unordered_set<uint32_t>& GoDReader::data_sectors() {
    if (data_sectors_.empty()) {
        populate_data_sectors();

        auto max_data_sector_it = std::max_element(data_sectors_.begin(), data_sectors_.end());
        max_data_sector_ = (max_data_sector_it != data_sectors_.end()) ? *max_data_sector_it : 0;    

        if (platform_ == Platform::OGX) {
            std::unordered_set<uint32_t> security_sectors;
            get_security_sectors(data_sectors_, security_sectors, false);

            for (const auto& sector : security_sectors) {
                data_sectors_.insert(sector);
            }
        }
    }
    return data_sectors_;
}

uint32_t GoDReader::max_data_sector() {
    if (data_sectors_.empty()) {
        data_sectors();
    }
    return max_data_sector_;
}

void GoDReader::populate_data_files(const std::filesystem::path& in_directory, int search_depth) {
    if (search_depth < 0) {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(in_directory)) {
        if (entry.is_regular_file() && StringUtils::case_insensitive_search(entry.path().string(), "Data")) {
            in_god_data_paths_.push_back(entry.path());
        } else if (entry.is_directory()) {
            populate_data_files(entry.path(), search_depth - 1);
        }
    }
}

GoDReader::Remap GoDReader::remap_sector(uint64_t xiso_sector) {
    uint64_t block_num = (xiso_sector * Xiso::SECTOR_SIZE) / GoD::BLOCK_SIZE;
    uint32_t file_index = static_cast<uint32_t>(block_num / GoD::DATA_BLOCKS_PER_PART);
    uint64_t data_block_within_file = block_num % GoD::DATA_BLOCKS_PER_PART;
    uint32_t hash_index = static_cast<uint32_t>(data_block_within_file / GoD::DATA_BLOCKS_PER_SHT);

    uint64_t remapped_offset = GoD::BLOCK_SIZE; // master hashtable
    remapped_offset += ((hash_index + 1) * GoD::BLOCK_SIZE); // Add subhash table blocks
    remapped_offset += (data_block_within_file * GoD::BLOCK_SIZE); // Add data blocks
    remapped_offset += (xiso_sector * Xiso::SECTOR_SIZE) % GoD::BLOCK_SIZE; // Add offset within data block
    return { remapped_offset, file_index };
}

GoDReader::Remap GoDReader::remap_offset(uint64_t xiso_offset) {
    Remap remapped = remap_sector(xiso_offset / Xiso::SECTOR_SIZE);
    remapped.offset += (xiso_offset % Xiso::SECTOR_SIZE);
    return { remapped.offset, remapped.file_index };
}

void GoDReader::read_sector(const uint32_t sector, char* out_buffer) {
    Remap remap = remap_sector(static_cast<uint64_t>(sector));
    in_files_[remap.file_index]->seekg(remap.offset, std::ios::beg);
    in_files_[remap.file_index]->read(out_buffer, Xiso::SECTOR_SIZE);
    if (in_files_[remap.file_index]->fail()) {
        throw XGDException(ErrCode::FILE_READ, HERE());
    }
}

void GoDReader::read_bytes(const uint64_t offset, const size_t size, char* out_buffer) {
    auto sectors_to_read = (size / Xiso::SECTOR_SIZE) + ((size % Xiso::SECTOR_SIZE) > 0 ? 1 : 0);
    auto start_sector = offset / Xiso::SECTOR_SIZE;
    auto start_offset = offset % Xiso::SECTOR_SIZE;

    std::vector<char> buffer(sectors_to_read * Xiso::SECTOR_SIZE);

    for (auto i = 0; i < sectors_to_read; i++) {
        read_sector(static_cast<uint32_t>(start_sector + i), buffer.data() + (i * Xiso::SECTOR_SIZE));
    }

    std::memcpy(out_buffer, buffer.data() + start_offset, size);
}

void GoDReader::populate_directory_entries(bool exe_only) {
    Xiso::DirectoryEntry root_entry;
    read_bytes(Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN, sizeof(uint32_t) * 2, reinterpret_cast<char*>(&root_entry.header.start_sector));
    // Remap remap = remap_offset(Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN);

    // in_files_[remap.file_index]->seekg(remap.offset, std::ios::beg);
    // in_files_[remap.file_index]->read(reinterpret_cast<char*>(&root_entry.header.start_sector), sizeof(uint32_t));
    // in_files_[remap.file_index]->read(reinterpret_cast<char*>(&root_entry.header.file_size), sizeof(uint32_t));
    // if (in_files_[remap.file_index]->fail()) {
    //     throw XGDException(ErrCode::FILE_READ, HERE());
    // }

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

        uint64_t current_position = current_entry.position + (current_entry.offset * sizeof(uint32_t));
        // uint32_t current_sector = static_cast<uint32_t>(current_position / Xiso::SECTOR_SIZE);
        // uint64_t position_in_sector = current_position % Xiso::SECTOR_SIZE;

        // std::vector<char> buffer(Xiso::SECTOR_SIZE * 2);

        // read_sector(current_sector, buffer.data());
        // read_sector(current_sector + 1, buffer.data() + Xiso::SECTOR_SIZE);

        // Xiso::DirectoryEntry read_entry;
        // std::memcpy(&read_entry.header, buffer.data() + position_in_sector, sizeof(Xiso::DirectoryEntry::Header));
        // read_entry.filename.assign(buffer.data() + position_in_sector + sizeof(Xiso::DirectoryEntry::Header), std::min(read_entry.header.name_length, static_cast<uint8_t>(UINT8_MAX)));

        // for (const auto& in_file : in_files_) {
        //     if (in_file->fail()) {
        //         throw XGDException(ErrCode::FILE_READ, HERE());
        //     }
        // }
        Xiso::DirectoryEntry read_entry;
        read_bytes(current_position, sizeof(Xiso::DirectoryEntry::Header), reinterpret_cast<char*>(&read_entry.header));
        
        std::vector<char> filename_buffer(std::min(read_entry.header.name_length, static_cast<uint8_t>(UINT8_MAX)));
        read_bytes(current_position + sizeof(Xiso::DirectoryEntry::Header), filename_buffer.size(), filename_buffer.data());
        read_entry.filename.assign(filename_buffer.data(), filename_buffer.size());

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
        throw XGDException(ErrCode::MISC, HERE(), "No executable found in GoD image");
    }

    if (directory_entries_.size() > 1) {
        std::sort(directory_entries_.begin(), directory_entries_.end(), [](const Xiso::DirectoryEntry& a, const Xiso::DirectoryEntry& b) {
            return a.path < b.path;
        });
    } else if (directory_entries_.empty()) {
        throw XGDException(ErrCode::MISC, HERE(), "No directory entries found in XISO image");
    }
}

void GoDReader::populate_data_sectors() {
    data_sectors_.clear();

    uint32_t header_sector = static_cast<uint32_t>(Xiso::MAGIC_OFFSET / Xiso::SECTOR_SIZE);
    data_sectors_.insert(header_sector);
    data_sectors_.insert(header_sector + 1);

    Xiso::DirectoryEntry root_entry;
    Remap remap = remap_offset(Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN);

    in_files_[remap.file_index]->seekg(remap.offset, std::ios::beg);
    in_files_[remap.file_index]->read(reinterpret_cast<char*>(&root_entry.header.start_sector), sizeof(uint32_t));
    in_files_[remap.file_index]->read(reinterpret_cast<char*>(&root_entry.header.file_size), sizeof(uint32_t));
    if (in_files_[remap.file_index]->fail()) {
        throw XGDException(ErrCode::FILE_READ, HERE());
    }

    root_entry.offset = 0;
    root_entry.position = static_cast<uint64_t>(root_entry.header.start_sector) * Xiso::SECTOR_SIZE;
    root_entry.path = "";

    std::vector<Xiso::DirectoryEntry> unprocessed_entries;
    unprocessed_entries.push_back(root_entry);

    bool exe_found = false;

    while (!unprocessed_entries.empty()) {
        Xiso::DirectoryEntry current_entry = unprocessed_entries.front();
        unprocessed_entries.erase(unprocessed_entries.begin());

        uint64_t current_position = current_entry.position + (current_entry.offset * sizeof(uint32_t));

        for (auto i = current_position >> 11; i < (current_position >> 11) + ((current_entry.header.file_size - (current_entry.offset * 4) + 2047) >> 11); i++) {
            data_sectors_.insert(static_cast<uint32_t>(i));
        }

        if ((current_entry.offset * sizeof(uint32_t)) >= current_entry.header.file_size) {
            continue;
        }

        uint32_t current_sector = static_cast<uint32_t>(current_position / Xiso::SECTOR_SIZE);
        uint64_t position_in_sector = current_position % Xiso::SECTOR_SIZE;

        std::vector<char> buffer(Xiso::SECTOR_SIZE * 2);

        read_sector(current_sector, buffer.data());
        read_sector(current_sector + 1, buffer.data() + Xiso::SECTOR_SIZE);

        Xiso::DirectoryEntry read_entry;
        std::memcpy(&read_entry.header, buffer.data() + position_in_sector, sizeof(Xiso::DirectoryEntry::Header));
        read_entry.filename.assign(buffer.data() + position_in_sector + sizeof(Xiso::DirectoryEntry::Header), std::min(read_entry.header.name_length, static_cast<uint8_t>(UINT8_MAX)));

        for (const auto& in_file : in_files_) {
            if (in_file->fail()) {
                throw XGDException(ErrCode::FILE_READ, HERE());
            }
        }

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
        } else if (read_entry.header.file_size > 0) {
            for (auto i = read_entry.header.start_sector; i < read_entry.header.start_sector + ((read_entry.header.file_size + Xiso::SECTOR_SIZE - 1) / Xiso::SECTOR_SIZE); i++) {
                data_sectors_.insert(static_cast<uint32_t>(i));
            }
        }

        if (read_entry.header.right_offset != 0) {
            current_entry.offset = static_cast<uint64_t>(read_entry.header.right_offset);
            unprocessed_entries.push_back(current_entry);
        }
    }
}

void GoDReader::get_security_sectors(const std::unordered_set<uint32_t>& data_sectors, std::unordered_set<uint32_t>& out_security_sectors, bool compare_mode) {
    const uint32_t end_sector = 0x345B60;
    bool flag = false;
    uint32_t start = 0;
    std::vector<char> sector_buffer(Xiso::SECTOR_SIZE);

    XGDLog() << "Getting security sectors..." << XGDLog::Endl;

    for (uint32_t sector_index = 0; sector_index <= end_sector; ++sector_index) {
        read_sector(sector_index, sector_buffer.data());

        bool is_empty_sector = std::accumulate(sector_buffer.begin(), sector_buffer.end(), 0) == 0;
        bool is_data_sector = data_sectors.find(sector_index) != data_sectors.end();

        if (is_empty_sector && !flag && !is_data_sector) {
            start = sector_index;
            flag = true;
        } else if (!is_empty_sector && flag) {
            uint32_t end = sector_index - 1;
            flag = false;

            if (end - start == 0xFFF) {
                for (uint32_t i = start; i <= end; ++i) {
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