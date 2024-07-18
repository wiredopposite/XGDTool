#ifndef _XISO_H_
#define _XISO_H_

#include <cstdint>
#include <string>
#include <filesystem>

#include "XGD.h"
#include "Common/EndianUtils.h"

namespace Xiso {

    static constexpr uint32_t SECTOR_SIZE  = 2048;
    static constexpr uint8_t  PAD_BYTE     = 0xFF;
    static constexpr uint16_t PAD_SHORT    = 0xFFFF;
    static constexpr uint32_t FILE_MODULUS = 0x10000;

    static constexpr uint32_t REDUMP_VIDEO_SECTORS = 0x30600;
    static constexpr uint32_t REDUMP_GAME_SECTORS  = 0x3A4D50;
    static constexpr uint32_t REDUMP_END_SECTOR    = 0x345B60;
    static constexpr uint32_t SECTOR_SHIFT         = 11;
    static constexpr uint64_t SPLIT_MARGIN = 0xFF000000; 

    static constexpr char     SYSTEM_UPDATE_DIR[] = "$SystemUpdate";

    static constexpr char     MAGIC_DATA[]      = "MICROSOFT*XBOX*MEDIA";
    static constexpr uint32_t MAGIC_DATA_LEN    = 20;
    static constexpr uint32_t MAGIC_OFFSET      = 0x10000;
    static constexpr uint32_t MAGIC_UNUSED_LEN  = 0x7c8;

    static constexpr uint32_t LSEEK_OFFSET_GLOBAL = 0x0FD90000;
    static constexpr uint32_t LSEEK_OFFSET_XGD3   = 0x02080000;
    static constexpr uint32_t LSEEK_OFFSET_XGD1   = 0x18300000;

    static constexpr char     MEDIA_ENABLE_TAG[]       = "\xe8\xca\xfd\xff\xff\x85\xc0\x7d";
    static constexpr size_t   MEDIA_ENABLE_TAG_LEN     = 8;
    static constexpr char     MEDIA_ENABLE_BYTE        = '\xeb';
    static constexpr uint32_t MEDIA_ENABLE_BYTE_OFFSET = 7;

    static constexpr uint32_t ROOT_DIRECTORY_SECTOR = 0x108;

    // static constexpr uint32_t FILENAME_OFFSET     = 14;
    // static constexpr uint32_t FILENAME_LEN_OFFSET = FILENAME_OFFSET - 1;
    static constexpr uint32_t FILENAME_MAX_CHARS  = 255;

    static constexpr uint8_t ATTRIBUTE_READ_ONLY = 0x01;
    static constexpr uint8_t ATTRIBUTE_HIDDEN    = 0x02;
    static constexpr uint8_t ATTRIBUTE_SYSTEM    = 0x04;
    static constexpr uint8_t ATTRIBUTE_DIRECTORY = 0x10;
    static constexpr uint8_t ATTRIBUTE_FILE      = 0x20;
    static constexpr uint8_t ATTRIBUTE_NORMAL    = 0x80;

    static constexpr uint32_t ECMA119_DATA_START        = 0x8000;
    static constexpr uint32_t ECMA119_VOL_SPACE_SIZE    = ECMA119_DATA_START + 80;
    static constexpr uint32_t ECMA119_VOL_SET_SIZE      = ECMA119_DATA_START + 120;
    static constexpr uint32_t ECMA119_VOL_SET_ID        = ECMA119_DATA_START + 190;
    static constexpr uint32_t ECMA119_VOL_CREATION_DATE = ECMA119_DATA_START + 813;

    struct DirectoryEntry {
        #pragma pack(push, 1)

        struct Header {
            uint16_t left_offset;
            uint16_t right_offset;
            uint32_t start_sector;
            uint32_t file_size;
            uint8_t attributes;
            uint8_t name_length;
        } header;

        #pragma pack(pop)
        static_assert(sizeof(DirectoryEntry::Header) == 14, "DirectoryEntry::Header size mismatch");

        std::string filename;
        
        // Custom fields
        uint64_t position;          // offset of entry relative to image offset
        uint64_t offset;            // left/right offset from parent node
        std::filesystem::path path; // relative to ISO root directory
    };

    #pragma pack(push, 1)

    struct FileTime {
        uint32_t low;
        uint32_t high;

        FileTime() {
            double tmp;
            time_t now;

            if ((now = std::time(nullptr)) == -1) {
                low = 0xd7d3e000;
                high = 0x01c55c11;
            } else {
                tmp = ((double) now + (369.0 * 365.25 * 24 * 60 * 60 - (3.0 * 24 * 60 * 60 + 6.0 * 60 * 60))) * 1.0e7;

                high = static_cast<uint32_t>(tmp * (1.0 / (4.0 * (double) (1 << 30))));
                low =  static_cast<uint32_t>(tmp - ((double) high) * 4.0 * (double) (1 << 30));
            }
            
            EndianUtils::little_32(high);
            EndianUtils::little_32(low);
        }
    };
    static_assert(sizeof(FileTime) == 8, "FileTime size mismatch");

    struct Header {
        char reserved1[XGD::OPTIMIZED_TAG_OFFSET];
        char optimized_tag[XGD::OPTIMIZED_TAG_LEN];
        char reserved2[Xiso::ECMA119_DATA_START - (sizeof(reserved1) + sizeof(optimized_tag))];

        struct Ecma119Header {
            char data_start[7]{ 0x01, 0x43, 0x44, 0x30, 0x30, 0x31, 0x01 };
            char reserved1[Xiso::ECMA119_VOL_SPACE_SIZE - Xiso::ECMA119_DATA_START - 7];
            uint32_t vol_space_size_little;
            uint32_t vol_space_size_big;
            char reserved2[Xiso::ECMA119_VOL_SET_SIZE - Xiso::ECMA119_VOL_SPACE_SIZE - 2 * sizeof(uint32_t)];
            char vol_set_size[12]{ 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x00, 0x08, 0x08, 0x00 };
            char reserved3[Xiso::ECMA119_VOL_SET_ID - Xiso::ECMA119_VOL_SET_SIZE - 12];
            char spaces[Xiso::ECMA119_VOL_CREATION_DATE - Xiso::ECMA119_VOL_SET_ID];
            char date1[17]{"0000000000000000"};
            char date2[17]{"0000000000000000"};
            char date3[17]{"0000000000000000"};
            char date4[17]{"0000000000000000"};
            char final_byte{0x01};
            char sector_padding[Xiso::SECTOR_SIZE - (sizeof(data_start) + sizeof(reserved1) + (2 * sizeof(uint32_t)) + sizeof(reserved2) + sizeof(vol_set_size) + sizeof(reserved3) + sizeof(spaces) + (4 * sizeof(date1)) + 1)];
            uint8_t sector_start[7]{ 0xFF, 0x43, 0x44, 0x30, 0x30, 0x31, 0x01 };
        } ecma119_header;

        char reserved3[Xiso::MAGIC_OFFSET - (Xiso::ECMA119_DATA_START + sizeof(Ecma119Header))];
        char magic1[20];
        uint32_t root_sector;
        uint32_t root_size;
        Xiso::FileTime file_time;
        char reserved4[Xiso::MAGIC_UNUSED_LEN];
        char magic2[20];

        Header(const uint32_t in_root_sector, const uint32_t in_root_size, const uint32_t in_total_sectors)
            : root_sector(in_root_sector), root_size(in_root_size) {

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
    };
    #pragma pack(pop)
    static_assert(sizeof(Header) == Xiso::MAGIC_OFFSET + Xiso::SECTOR_SIZE, "Xiso Header size mismatch");
    static_assert(offsetof(Header, ecma119_header) == Xiso::ECMA119_DATA_START, "ECMA start offset mismatch");
    static_assert(offsetof(Header, magic1) == Xiso::MAGIC_OFFSET, "Xiso Header magic1 offset mismatch");
    static_assert(sizeof(Xiso::Header::Ecma119Header) == Xiso::SECTOR_SIZE + 7, "Ecma119 Header size mismatch");

};

#endif // _XISO_H_