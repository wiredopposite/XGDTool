#include <cstring>

#include "ImageReader/CSOReader/CSOReader.h"

CSOReader::CSOReader(const std::vector<std::filesystem::path>& in_cso_paths)
    : in_cso_paths_(in_cso_paths)
{
    part_1_size_ = std::filesystem::file_size(in_cso_paths_.front());

    for (const auto& path : in_cso_paths_) 
    {
        in_files_.push_back(std::make_unique<std::ifstream>(path, std::ios::binary));
        if (!in_files_.back()->is_open()) 
        {
            throw XGDException(ErrCode::FILE_OPEN, HERE(), path.string());
        }
    }

    verify_and_populate_index_infos();

    total_sectors_ = static_cast<uint32_t>(index_infos_.size()) - 1;

    LZ4F_errorCode_t lz4f_error = LZ4F_createDecompressionContext(&lz4f_dctx_, LZ4F_VERSION);
    if (LZ4F_isError(lz4f_error)) 
    {
        throw XGDException(ErrCode::MISC, HERE(), LZ4F_getErrorName(lz4f_error));
    }
}

CSOReader::~CSOReader()
{
    LZ4F_freeDecompressionContext(lz4f_dctx_);

    for (auto& file : in_files_) 
    {
        file->close();
    }
    in_files_.clear();
}

void CSOReader::verify_and_populate_index_infos()
{
    index_infos_.clear();

    in_files_[0]->seekg(0, std::ios::beg);

    CSO::Header header;

    in_files_[0]->read(reinterpret_cast<char*>(&header), sizeof(CSO::Header));
    if (in_files_[0]->fail()) 
    {
        throw XGDException(ErrCode::FILE_READ, HERE());
    }

    if ((std::memcmp(&header.magic, CSO::MAGIC, CSO::MAGIC_LEN) != 0) ||
        header.block_size != CSO::BLOCK_SIZE ||
        header.header_size != CSO::HEADER_SIZE ||
        header.version != CSO::VERSION ||
        header.index_alignment != CSO::INDEX_ALIGNMENT) 
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE());
    }

    index_infos_.reserve(static_cast<uint32_t>(header.uncompressed_size / Xiso::SECTOR_SIZE) + 1);

    for (uint32_t j = 0; j < (header.uncompressed_size / CSO::BLOCK_SIZE) + 1; ++j) 
    {
        uint32_t index;
        
        in_files_[0]->read(reinterpret_cast<char*>(&index), sizeof(uint32_t));
        if (in_files_[0]->fail()) 
        {
            throw XGDException(ErrCode::FILE_READ, HERE());
        }

        index_infos_.push_back({ (index & 0x7FFFFFFF) << CSO::INDEX_ALIGNMENT, ((index & 0x80000000) > 0) });
    }
}

void CSOReader::read_sector(const uint32_t sector, char* out_buffer)
{
    size_t read_len = index_infos_[sector + 1].value - index_infos_[sector].value;
    int file_idx = (index_infos_[sector].value > part_1_size_) ? 1 : 0;

    in_files_[file_idx]->seekg(index_infos_[sector].value, std::ios::beg);

    if (index_infos_[sector].compressed || read_len < Xiso::SECTOR_SIZE)
    {
        size_t compressed_size = sizeof(LZ4F_HEADER) + read_len + sizeof(LZ4F_FOOTER);
        size_t decompressed_size = Xiso::SECTOR_SIZE;

        std::vector<char> read_buffer(compressed_size, 0);
        std::memcpy(read_buffer.data(), LZ4F_HEADER, sizeof(LZ4F_HEADER));

        in_files_[file_idx]->read(read_buffer.data() + sizeof(LZ4F_HEADER), read_len);
        if (in_files_[file_idx]->fail()) 
        {
            throw XGDException(ErrCode::FILE_READ, HERE());
        }

        std::memcpy(read_buffer.data() + sizeof(LZ4F_HEADER) + read_len, LZ4F_FOOTER, sizeof(LZ4F_FOOTER));

        size_t lz4_decompressed_size = LZ4F_decompress(lz4f_dctx_, out_buffer, &decompressed_size, read_buffer.data(), &compressed_size, nullptr);
        if (LZ4F_isError(lz4_decompressed_size)) 
        {
            throw XGDException(ErrCode::MISC, HERE(), LZ4F_getErrorName(lz4_decompressed_size));
        }
    }
    else if (read_len != Xiso::SECTOR_SIZE)
    {
        throw XGDException(ErrCode::ISO_INVALID, HERE());
    }
    else
    {
        in_files_[file_idx]->read(out_buffer, Xiso::SECTOR_SIZE);
        if (in_files_[file_idx]->fail()) 
        {
            throw XGDException(ErrCode::FILE_READ, HERE());
        }
    }
}

void CSOReader::read_bytes(const uint64_t offset, const size_t size, char* out_buffer) 
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