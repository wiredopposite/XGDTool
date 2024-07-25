#include <cstring>
#include <algorithm>
#include <numeric>

#include "XGD.h"
#include "Utils/StringUtils.h"
#include "ImageReader/GoDReader/GoDReader.h"

GoDReader::GoDReader(const std::vector<std::filesystem::path>& in_god_directory) 
    : in_god_directory_(in_god_directory.front()) 
{
    populate_data_files(in_god_directory_, 5);

    std::sort(in_god_data_paths_.begin(), in_god_data_paths_.end(), [](const auto& a, const auto& b) 
    {
        return a.string() < b.string();
    });

    auto last_blocks = std::filesystem::file_size(in_god_data_paths_.back()) / GoD::BLOCK_SIZE;
    auto last_hash_index = (last_blocks - 1) / (GoD::DATA_BLOCKS_PER_SHT + 1);
    auto last_data_blocks = last_blocks - (last_hash_index + 1);
    auto total_data_blocks = ((in_god_data_paths_.size() - 1) * GoD::DATA_BLOCKS_PER_PART) + last_data_blocks;

    total_sectors_ = static_cast<uint32_t>(total_data_blocks * (GoD::BLOCK_SIZE / Xiso::SECTOR_SIZE));

    for (const auto& data_path : in_god_data_paths_) 
    {
        in_files_.push_back(std::make_unique<std::ifstream>(data_path, std::ios::binary));
        if (!in_files_.back()->is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), data_path.string());
        }
    }

    XGDLog(Debug) << "GoD data files opened: " << in_files_.size() << "\n";
}

GoDReader::~GoDReader() 
{
    for (auto& in_file : in_files_) 
    {
        if (in_file->is_open()) 
        {
            in_file->close();
        }
    }
}

void GoDReader::populate_data_files(const std::filesystem::path& in_directory, int search_depth) 
{
    if (search_depth < 0) 
    {
        return;
    }
    for (const auto& entry : std::filesystem::directory_iterator(in_directory)) 
    {
        if (entry.is_regular_file() && StringUtils::case_insensitive_search(entry.path().string(), "Data")) 
        {
            in_god_data_paths_.push_back(entry.path());
        } 
        else if (entry.is_directory()) 
        {
            populate_data_files(entry.path(), search_depth - 1);
        }
    }
}

GoDReader::Remap GoDReader::remap_sector(uint64_t xiso_sector) 
{
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

GoDReader::Remap GoDReader::remap_offset(uint64_t xiso_offset) 
{
    Remap remapped = remap_sector(xiso_offset / Xiso::SECTOR_SIZE);
    remapped.offset += (xiso_offset % Xiso::SECTOR_SIZE);
    return { remapped.offset, remapped.file_index };
}

void GoDReader::read_sector(const uint32_t sector, char* out_buffer) 
{
    Remap remap = remap_sector(static_cast<uint64_t>(sector));
    in_files_[remap.file_index]->seekg(remap.offset, std::ios::beg);
    in_files_[remap.file_index]->read(out_buffer, Xiso::SECTOR_SIZE);
    if (in_files_[remap.file_index]->fail()) 
    {
        throw XGDException(ErrCode::FILE_READ, HERE());
    }
    XGDLog(Debug) << "Read sector " << sector << " from file " << remap.file_index << " at offset " << remap.offset << "\n";
}

void GoDReader::read_bytes(const uint64_t offset, const size_t size, char* out_buffer) 
{
    auto sectors_to_read = (size / Xiso::SECTOR_SIZE) + ((size % Xiso::SECTOR_SIZE) > 0 ? 1 : 0);
    auto start_sector = offset / Xiso::SECTOR_SIZE;
    auto start_offset = offset % Xiso::SECTOR_SIZE;

    std::vector<char> buffer(sectors_to_read * Xiso::SECTOR_SIZE);

    for (auto i = 0; i < sectors_to_read; i++) 
    {
        read_sector(static_cast<uint32_t>(start_sector + i), buffer.data() + (i * Xiso::SECTOR_SIZE));
    }

    std::memcpy(out_buffer, buffer.data() + start_offset, size);
}