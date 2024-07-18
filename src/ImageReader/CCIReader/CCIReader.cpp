#include <cstring>
#include <algorithm>
#include <numeric>

#include <lz4hc.h>

#include "XGD.h"
#include "Common/Utils.h"
#include "ImageReader/CCIReader/CCIReader.h"

CCIReader::CCIReader(const std::vector<std::filesystem::path>& in_cci_paths) 
    : in_cci_paths_(in_cci_paths) {

    for (const auto& path : in_cci_paths_) {
        in_files_.push_back(std::make_unique<std::ifstream>(path, std::ios::binary));
        if (!in_files_.back()->is_open()) {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), path.string());
        }
    }

    verify_and_populate_index_infos();

    total_sectors_ = static_cast<uint32_t>(index_infos_.size());

    populate_directory_entries(true);
}

CCIReader::~CCIReader() {
    for (auto& file : in_files_) {
        file->close();
    }
}

const std::vector<Xiso::DirectoryEntry>& CCIReader::directory_entries() {
    if (directory_entries_.empty()) {
        populate_directory_entries(false);
    }
    return directory_entries_;
}

uint64_t CCIReader::total_file_bytes() {
    if (total_file_bytes_ == 0) {
        for (const auto& entry : directory_entries()) {
            if (!(entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY)) {
                total_file_bytes_ += entry.header.file_size;
            }
        }
    }
    return total_file_bytes_;
}

const std::unordered_set<uint32_t>& CCIReader::data_sectors() {
    if (data_sectors_.empty()) {
        populate_data_sectors();

        auto max_data_sector_it = std::max_element(data_sectors_.begin(), data_sectors_.end());
        max_data_sector_ = (max_data_sector_it != data_sectors_.end()) ? *max_data_sector_it : 0;

        std::unordered_set<uint32_t> security_sectors;
        get_security_sectors(data_sectors_, security_sectors, false);

        for (const auto& sector : security_sectors) {
            data_sectors_.insert(sector);
        }
    }
    return data_sectors_;
}

uint32_t CCIReader::max_data_sector() {
    if (data_sectors_.empty()) {
        data_sectors();
    }
    return max_data_sector_;
}

void CCIReader::verify_and_populate_index_infos() {
    for (int i = 0; i < in_files_.size(); ++i) {
        in_files_[i]->seekg(0, std::ios::beg);

        CCI::Header header;

        in_files_[i]->read(reinterpret_cast<char*>(&header), sizeof(CCI::Header));
        if (in_files_[i]->fail()) {
            throw XGDException(ErrCode::FILE_READ, HERE());
        }

        if ((std::memcmp(&header.magic, CCI::MAGIC, CCI::MAGIC_LEN) != 0) ||
            header.block_size != CCI::BLOCK_SIZE ||
            header.header_size != CCI::HEADER_SIZE ||
            header.version != CCI::VERSION ||
            header.index_alignment != CCI::INDEX_ALIGNMENT) {
            throw XGDException(ErrCode::ISO_INVALID, HERE());
        }

        if (i == 0) {
            part_1_sectors_ = static_cast<uint32_t>(header.uncompressed_size / CCI::BLOCK_SIZE);
        }

        in_files_[i]->seekg(header.index_offset, std::ios::beg);

        for (uint32_t j = 0; j <= header.uncompressed_size / CCI::BLOCK_SIZE; ++j) {
            uint32_t index;
            
            in_files_[i]->read(reinterpret_cast<char*>(&index), sizeof(uint32_t));
            if (in_files_[i]->fail()) {
                throw XGDException(ErrCode::FILE_READ, HERE());
            }

            index_infos_.push_back({ (index & 0x7FFFFFFF) << CCI::INDEX_ALIGNMENT, ((index & 0x80000000) > 0) });
        }
    }
}

void CCIReader::read_sector(const uint32_t sector, char* out_buffer) {
    int idx = (sector < part_1_sectors_) ? 0 : 1;
    size_t compressed_size = index_infos_[sector + 1].value - index_infos_[sector].value;

    if (index_infos_[sector].compressed || compressed_size < Xiso::SECTOR_SIZE) {
        uint8_t padding_len;

        in_files_[idx]->seekg(index_infos_[sector].value, std::ios::beg);
        in_files_[idx]->read(reinterpret_cast<char*>(&padding_len), sizeof(uint8_t));

        compressed_size = compressed_size - (1 + padding_len);

        std::vector<char> read_buffer(Xiso::SECTOR_SIZE);
        in_files_[idx]->read(read_buffer.data(), compressed_size);

        int decompressed_size = LZ4_decompress_safe(read_buffer.data(), out_buffer, static_cast<int>(compressed_size), Xiso::SECTOR_SIZE);
        if (decompressed_size < 0 || (decompressed_size != Xiso::SECTOR_SIZE)) {
            throw XGDException(ErrCode::MISC, HERE(), "LZ4_decompress_safe failed");
        }
    } else {
        in_files_[idx]->seekg(index_infos_[sector].value, std::ios::beg);
        in_files_[idx]->read(out_buffer, Xiso::SECTOR_SIZE);
    }
}

void CCIReader::read_bytes(const uint64_t offset, const size_t size, char* out_buffer) {
    uint32_t sectors_to_read = static_cast<uint32_t>(size / Xiso::SECTOR_SIZE) + ((size % Xiso::SECTOR_SIZE) ? 1 : 0);
    uint32_t start_sector = static_cast<uint32_t>(offset / Xiso::SECTOR_SIZE);
    size_t position_in_sector = offset % Xiso::SECTOR_SIZE;

    std::vector<char> buffer(sectors_to_read * Xiso::SECTOR_SIZE);

    for (uint32_t i = 0; i < sectors_to_read; ++i) {
        read_sector(start_sector + i, buffer.data() + (i * Xiso::SECTOR_SIZE));
    }

    std::memcpy(out_buffer, buffer.data() + position_in_sector, size);
}

void CCIReader::populate_directory_entries(bool exe_only) {
    if (!exe_only) {
        directory_entries_.clear();
    }

    uint64_t root_offset = Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN;
    uint32_t root_sector = static_cast<uint32_t>(root_offset / Xiso::SECTOR_SIZE);
    size_t position_in_sector = root_offset % Xiso::SECTOR_SIZE;

    std::vector<char> decode_buffer(Xiso::SECTOR_SIZE);
    read_sector(root_sector, decode_buffer.data());

    Xiso::DirectoryEntry root_entry;
    std::memcpy(&root_entry.header.start_sector, decode_buffer.data() + position_in_sector, sizeof(uint32_t));
    std::memcpy(&root_entry.header.file_size, decode_buffer.data() + position_in_sector + sizeof(uint32_t), sizeof(uint32_t));

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
        uint32_t current_sector = static_cast<uint32_t>(current_position / Xiso::SECTOR_SIZE);
        size_t position_in_sector = current_position % Xiso::SECTOR_SIZE;

        std::vector<char> decode_buffer(Xiso::SECTOR_SIZE * 2);
        read_sector(current_sector, decode_buffer.data());
        read_sector(current_sector + 1, decode_buffer.data() + Xiso::SECTOR_SIZE);

        Xiso::DirectoryEntry read_entry;
        std::memcpy(&read_entry.header, decode_buffer.data() + position_in_sector, sizeof(Xiso::DirectoryEntry::Header));
        read_entry.filename.assign(decode_buffer.data() + position_in_sector + sizeof(Xiso::DirectoryEntry::Header), std::min(read_entry.header.name_length, static_cast<uint8_t>(UINT8_MAX)));

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

    // Sort by path
    if (directory_entries_.size() > 1) {
        std::sort(directory_entries_.begin(), directory_entries_.end(), [](const Xiso::DirectoryEntry& a, const Xiso::DirectoryEntry& b) {
            return a.path < b.path;
        });
    } else if (directory_entries_.empty()) {
        throw XGDException(ErrCode::MISC, HERE(), "No directory entries found in XISO image");
    }
}

void CCIReader::populate_data_sectors() {
    data_sectors_.clear();

    Xiso::DirectoryEntry root_entry;
    read_bytes(Xiso::MAGIC_OFFSET + Xiso::MAGIC_DATA_LEN, sizeof(uint32_t) * 2, reinterpret_cast<char*>(&root_entry.header.start_sector));

    root_entry.offset = 0;
    root_entry.position = static_cast<uint64_t>(root_entry.header.start_sector) * Xiso::SECTOR_SIZE;
    root_entry.path = "";

    std::vector<Xiso::DirectoryEntry> unprocessed_entries;
    unprocessed_entries.push_back(root_entry);

    XGDLog() << "Getting data sectors..." << XGDLog::Endl;

    while (!unprocessed_entries.empty()) {
        Xiso::DirectoryEntry current_entry = unprocessed_entries.front();
        unprocessed_entries.erase(unprocessed_entries.begin());

        uint64_t current_position = current_entry.position + (current_entry.offset * sizeof(uint32_t));

        for (auto i = current_position >> 11; i < (current_position >> 11) + ((current_entry.header.file_size - (current_entry.offset * 4) + 2047) >> 11); i++) {
            data_sectors_.insert(static_cast<uint32_t>(i));
        }

        if ((current_entry.offset * 4) >= current_entry.header.file_size) {
            continue;
        }

        // uint64_t current_position = current_entry.position + (current_entry.offset * sizeof(uint32_t));
        // uint32_t current_sector = static_cast<uint32_t>(current_position / Xiso::SECTOR_SIZE);
        // size_t position_in_sector = current_position % Xiso::SECTOR_SIZE;

        // std::vector<char> decode_buffer(Xiso::SECTOR_SIZE * 2);
        // read_sector(current_sector, decode_buffer.data());
        // read_sector(current_sector + 1, decode_buffer.data() + Xiso::SECTOR_SIZE);

        Xiso::DirectoryEntry read_entry;
        // std::memcpy(&read_entry.header, decode_buffer.data() + position_in_sector, sizeof(Xiso::DirectoryEntry::Header));
        read_bytes(current_position, sizeof(Xiso::DirectoryEntry::Header), reinterpret_cast<char*>(&read_entry.header));

        std::vector<char> filename_buffer(std::min(static_cast<uint8_t>(UINT8_MAX), read_entry.header.name_length));
        read_bytes(current_position + sizeof(Xiso::DirectoryEntry::Header), read_entry.header.name_length, filename_buffer.data());
        read_entry.filename.assign(filename_buffer.data(), filename_buffer.size());

        if (read_entry.header.left_offset == Xiso::PAD_SHORT) {
            continue;
        }

        if (read_entry.header.left_offset != 0) {
            Xiso::DirectoryEntry unprocessed_left = current_entry;
            unprocessed_left.offset = static_cast<uint64_t>(read_entry.header.left_offset);
            unprocessed_entries.push_back(unprocessed_left);
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
                for (auto i = read_entry.header.start_sector; i < read_entry.header.start_sector + ((read_entry.header.file_size + Xiso::SECTOR_SIZE - 1) / Xiso::SECTOR_SIZE); i++) {
                    data_sectors_.insert(static_cast<uint32_t>(i));
                }
            }
        }

        if (read_entry.header.right_offset != 0) {
            Xiso::DirectoryEntry unprocessed_right = current_entry;
            unprocessed_right.offset = static_cast<uint64_t>(read_entry.header.right_offset);
            unprocessed_entries.push_back(unprocessed_right);
        }
    }
}

void CCIReader::get_security_sectors(const std::unordered_set<uint32_t>& data_sectors, std::unordered_set<uint32_t>& out_security_sectors, bool compare_mode) {
    bool flag = false;
    uint32_t start = 0;
    std::vector<char> sector_buffer(Xiso::SECTOR_SIZE);

    XGDLog() << "Getting security sectors..." << XGDLog::Endl;

    for (uint32_t sector_index = 0; sector_index <= total_sectors_; ++sector_index) {
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

        XGDLog().print_progress(sector_index, total_sectors_);
    }
}