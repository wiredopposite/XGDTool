#include <cstring>

#include "XGDLog.h"
#include "Formats/Xiso.h"

Xiso::FileTime::FileTime() 
{
    double tmp;
    time_t now;

    if ((now = std::time(nullptr)) == -1)
    {
        XGDLog(Error) << "Warning: Failed to retrieve current time from OS, using default." << XGDLog::Endl;

        low = 0xd7d3e000;
        high = 0x01c55c11;
    } 
    else 
    {
        tmp = ((double) now + (369.0 * 365.25 * 24 * 60 * 60 - (3.0 * 24 * 60 * 60 + 6.0 * 60 * 60))) * 1.0e7;

        high = static_cast<uint32_t>(tmp * (1.0 / (4.0 * (double) (1 << 30))));
        low =  static_cast<uint32_t>(tmp - ((double) high) * 4.0 * (double) (1 << 30));
    }
    
    EndianUtils::little_32(high);
    EndianUtils::little_32(low);
}

Xiso::Header::Header(const uint32_t in_root_sector, const uint32_t in_root_size, const uint32_t in_total_sectors, const FileTime in_file_time)
    : root_sector(in_root_sector), root_size(in_root_size), file_time(in_file_time)
{
    std::memcpy(optimized_tag, XGD::OPTIMIZED_TAG, sizeof(optimized_tag));
    std::memcpy(magic1, Xiso::MAGIC_DATA, sizeof(magic1));
    std::memcpy(magic2, Xiso::MAGIC_DATA, sizeof(magic2));

    ecma119_header.vol_space_size_little = in_total_sectors;
    ecma119_header.vol_space_size_big = in_total_sectors;

    EndianUtils::big_32(ecma119_header.vol_space_size_big);
    EndianUtils::little_32(ecma119_header.vol_space_size_little);
    
    EndianUtils::little_32(root_sector);
    EndianUtils::little_32(root_size);

    std::memset(reserved1, 0, sizeof(reserved1));
    std::memset(reserved2, 0, sizeof(reserved2));
    std::memset(reserved3, 0, sizeof(reserved3));
    std::memset(reserved4, 0, sizeof(reserved4));

    std::memset(ecma119_header.reserved1, 0, sizeof(ecma119_header.reserved1));
    std::memset(ecma119_header.reserved2, 0, sizeof(ecma119_header.reserved2));
    std::memset(ecma119_header.reserved3, 0, sizeof(ecma119_header.reserved3));
    std::memset(ecma119_header.spaces, 0x20, sizeof(ecma119_header.spaces));
    std::memset(ecma119_header.sector_padding, 0, sizeof(ecma119_header.sector_padding));
}