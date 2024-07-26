#include <cstring>
#include <algorithm>
#include <numeric>

#include <lz4hc.h>

#include "XGD.h"
#include "ImageReader/CCIReader/CCIReader.h"

CCIReader::CCIReader(const std::vector<std::filesystem::path>& in_cci_paths) 
    : in_cci_paths_(in_cci_paths) 
{
    for (const auto& path : in_cci_paths_) 
    {
        in_files_.push_back(std::make_unique<std::ifstream>(path, std::ios::binary));
        if (!in_files_.back()->is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), path.string());
        }
    }

    verify_and_populate_index_infos();

    for (const auto& index_info : index_infos_) 
    {
        total_sectors_ += static_cast<uint32_t>(index_info.size()) - 1;
    }
}

CCIReader::~CCIReader() 
{
    for (auto& in_file : in_files_) 
    {
        in_file->close();
    }
    in_files_.clear();
}

void CCIReader::verify_and_populate_index_infos() 
{
    index_infos_.resize(in_files_.size());   

    for (int i = 0; i < in_files_.size(); ++i) 
    {
        in_files_[i]->seekg(0, std::ios::beg);

        CCI::Header header;

        in_files_[i]->read(reinterpret_cast<char*>(&header), sizeof(CCI::Header));
        if (in_files_[i]->fail()) 
        {
            throw XGDException(ErrCode::FILE_READ, HERE());
        }

        if ((std::memcmp(&header.magic, CCI::MAGIC, CCI::MAGIC_LEN) != 0) ||
            header.block_size != CCI::BLOCK_SIZE ||
            header.header_size != CCI::HEADER_SIZE ||
            header.version != CCI::VERSION ||
            header.index_alignment != CCI::INDEX_ALIGNMENT) 
        {
            throw XGDException(ErrCode::ISO_INVALID, HERE());
        }
        
        index_infos_[i].reserve(static_cast<uint32_t>(header.uncompressed_size / CCI::BLOCK_SIZE) + 1);

        in_files_[i]->seekg(header.index_offset, std::ios::beg);

        for (uint32_t j = 0; j <= static_cast<uint32_t>(header.uncompressed_size / CCI::BLOCK_SIZE); ++j) 
        {
            uint32_t index;
            
            in_files_[i]->read(reinterpret_cast<char*>(&index), sizeof(uint32_t));
            if (in_files_[i]->fail()) 
            {
                throw XGDException(ErrCode::FILE_READ, HERE());
            }

            index_infos_[i].push_back({ (index & 0x7FFFFFFF) << CCI::INDEX_ALIGNMENT, ((index & 0x80000000) > 0) });
        }
    }
}

void CCIReader::read_sector(const uint32_t sector, char* out_buffer) 
{
    int idx = (sector > index_infos_[0].size() - 2) ? 1 : 0; //Final index in each file doesn't represent a sector

    if (idx > 0 && in_files_.size() < 2)
    {
        throw XGDException(ErrCode::MISC, HERE(), "Sector requested is out of bounds");
    }

    uint32_t sector_in_file = sector - (idx * (static_cast<uint32_t>(index_infos_[0].size()) - 1));
    size_t compressed_size = index_infos_[idx][sector_in_file + 1].value - index_infos_[idx][sector_in_file].value;

    if (index_infos_[idx][sector_in_file].compressed || compressed_size < Xiso::SECTOR_SIZE) 
    {
        uint8_t padding_len;

        in_files_[idx]->seekg(index_infos_[idx][sector_in_file].value, std::ios::beg);
        in_files_[idx]->read(reinterpret_cast<char*>(&padding_len), sizeof(uint8_t));

        compressed_size = compressed_size - (1 + padding_len);

        std::vector<char> read_buffer(Xiso::SECTOR_SIZE);
        in_files_[idx]->read(read_buffer.data(), compressed_size);

        int decompressed_size = LZ4_decompress_safe(read_buffer.data(), out_buffer, static_cast<int>(compressed_size), Xiso::SECTOR_SIZE);
        if (decompressed_size < 0 || (decompressed_size != Xiso::SECTOR_SIZE)) 
        {
            throw XGDException(ErrCode::MISC, HERE(), "LZ4_decompress_safe failed");
        }
    } 
    else 
    {
        in_files_[idx]->seekg(index_infos_[idx][sector_in_file].value, std::ios::beg);
        in_files_[idx]->read(out_buffer, Xiso::SECTOR_SIZE);
    }

    if (in_files_[idx]->fail()) 
    {
        throw XGDException(ErrCode::FILE_READ, HERE());
    }
}

void CCIReader::read_bytes(const uint64_t offset, const size_t size, char* out_buffer) 
{
    uint32_t sectors_to_read = static_cast<uint32_t>(size / Xiso::SECTOR_SIZE) + ((size % Xiso::SECTOR_SIZE) ? 1 : 0);
    uint32_t start_sector = static_cast<uint32_t>(offset / Xiso::SECTOR_SIZE);
    size_t position_in_sector = offset % Xiso::SECTOR_SIZE;

    std::vector<char> buffer(sectors_to_read * Xiso::SECTOR_SIZE);

    for (uint32_t i = 0; i < sectors_to_read; ++i) 
    {
        read_sector(start_sector + i, buffer.data() + (i * Xiso::SECTOR_SIZE));
    }

    std::memcpy(out_buffer, buffer.data() + position_in_sector, size);
}