#include <cstring>
#include <algorithm>
#include <numeric>

#include "XGD.h"
#include "ImageReader/XisoReader/XisoReader.h"

XisoReader::XisoReader(const std::vector<std::filesystem::path>& in_xiso_paths) 
    : in_xiso_paths_(in_xiso_paths) 
{
    in_file_ = split::ifstream(in_xiso_paths_);
    if (!in_file_.is_open()) 
    {
        throw XGDException(ErrCode::FILE_OPEN, HERE(), in_xiso_paths_.front().string() + ((in_xiso_paths_.size() > 1) ? (" and " + in_xiso_paths_.back().string()) : ""));
    }

    in_file_.seekg(0, std::ios::end);
    total_sectors_ = static_cast<uint32_t>(in_file_.tellg() / Xiso::SECTOR_SIZE);
    image_offset_ = get_image_offset(); 
}

XisoReader::~XisoReader() 
{
    if (in_file_.is_open()) 
    {
        in_file_.close();
    }
}

void XisoReader::read_sector(const uint32_t sector, char* out_buffer) 
{
    uint64_t position = static_cast<uint64_t>(sector) * static_cast<uint64_t>(Xiso::SECTOR_SIZE);
    in_file_.seekg(position, std::ios::beg);
    in_file_.read(out_buffer, Xiso::SECTOR_SIZE);

    if (in_file_.fail() || in_file_.gcount() != Xiso::SECTOR_SIZE) 
    {
        std::cerr << "Failed to read sector: " << sector 
                  << ", Bytes read: " << in_file_.gcount() 
                  << ", Expected bytes: " << Xiso::SECTOR_SIZE << std::endl;

        throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read sector from input file");
    }
}

void XisoReader::read_bytes(const uint64_t offset, const size_t size, char* out_buffer) 
{
    in_file_.seekg(offset, std::ios::beg);
    if (in_file_.fail() || in_file_.tellg() != offset) 
    {
        throw XGDException(ErrCode::FILE_SEEK, HERE(), "Failed to seek to offset in input file");
    }
    in_file_.read(out_buffer, size);
    if (in_file_.fail() || in_file_.gcount() != size) 
    {
        throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read bytes from input file");
    }
}

uint64_t XisoReader::get_image_offset() 
{
    XGDLog(Debug) << "Getting image offset..." << XGDLog::Endl;

    char buffer[Xiso::MAGIC_DATA_LEN];

    const std::vector<int> seek_offsets = 
    {
        0,
        Xiso::LSEEK_OFFSET_GLOBAL,
        Xiso::LSEEK_OFFSET_XGD3,
        Xiso::LSEEK_OFFSET_XGD1
    };

    for (int offset : seek_offsets) 
    {
        in_file_.seekg(Xiso::MAGIC_OFFSET + offset, std::ios::beg);
        in_file_.read(buffer, Xiso::MAGIC_DATA_LEN);

        if (in_file_.fail()) 
        {
            throw XGDException(ErrCode::FILE_READ, HERE(), "Failed to read magic data from input file");
        }

        if (std::memcmp(buffer, Xiso::MAGIC_DATA, Xiso::MAGIC_DATA_LEN) == 0) 
        {
            XGDLog(Debug) << "Found XISO magic data, image offset: " << offset << ", sector: " << (offset / Xiso::SECTOR_SIZE) << XGDLog::Endl;
            return offset;
        }
    }

    throw XGDException(ErrCode::MISC, HERE(), "Invalid XISO file, failed to find XISO magic data and get image offset");
    return 0;
}