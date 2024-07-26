#ifndef _XISO_H_
#define _XISO_H_

#include <cstdint>
#include <string>
#include <filesystem>

#include "XGD.h"
#include "Utils/EndianUtils.h"

namespace Xiso {

    constexpr uint64_t SECTOR_SIZE  = 2048;
    constexpr uint8_t  PAD_BYTE     = 0xFF;
    constexpr uint16_t PAD_SHORT    = 0xFFFF;
    constexpr uint64_t FILE_MODULUS = 0x10000;

    constexpr uint64_t REDUMP_VIDEO_SECTORS = 0x30600;
    constexpr uint64_t REDUMP_TOTAL_SECTORS = 0x3A4D50;
    constexpr uint64_t REDUMP_GAME_SECTORS  = REDUMP_TOTAL_SECTORS - REDUMP_VIDEO_SECTORS;
    constexpr uint64_t SPLIT_MARGIN         = 0xFF000000; 

    constexpr char     SYSTEM_UPDATE_DIR[] = "$SystemUpdate";

    constexpr char     MAGIC_DATA[]      = "MICROSOFT*XBOX*MEDIA";
    constexpr uint64_t MAGIC_DATA_LEN    = 20;
    constexpr uint64_t MAGIC_OFFSET      = 0x10000;
    constexpr uint64_t MAGIC_UNUSED_LEN  = 0x7c8;

    constexpr uint64_t LSEEK_OFFSET_GLOBAL = 0x0FD90000;
    constexpr uint64_t LSEEK_OFFSET_XGD3   = 0x02080000;
    constexpr uint64_t LSEEK_OFFSET_XGD1   = 0x18300000;

    constexpr uint32_t ROOT_DIRECTORY_SECTOR = 0x108;

    // constexpr uint32_t FILENAME_MAX_CHARS  = 255;

    constexpr uint8_t ATTRIBUTE_READ_ONLY = 0x01;
    constexpr uint8_t ATTRIBUTE_HIDDEN    = 0x02;
    constexpr uint8_t ATTRIBUTE_SYSTEM    = 0x04;
    constexpr uint8_t ATTRIBUTE_DIRECTORY = 0x10;
    constexpr uint8_t ATTRIBUTE_FILE      = 0x20;
    constexpr uint8_t ATTRIBUTE_NORMAL    = 0x80;

    constexpr uint64_t ECMA119_DATA_START        = 0x8000;
    constexpr uint64_t ECMA119_VOL_SPACE_SIZE    = ECMA119_DATA_START + 80;
    constexpr uint64_t ECMA119_VOL_SET_SIZE      = ECMA119_DATA_START + 120;
    constexpr uint64_t ECMA119_VOL_SET_ID        = ECMA119_DATA_START + 190;
    constexpr uint64_t ECMA119_VOL_CREATION_DATE = ECMA119_DATA_START + 813;

    #pragma pack(push, 1)

    struct DirectoryEntry {
        struct Header {
            uint16_t left_offset;
            uint16_t right_offset;
            uint32_t start_sector;
            uint32_t file_size;
            uint8_t attributes;
            uint8_t name_length;
        } header;
        static_assert(sizeof(DirectoryEntry::Header) == 14, "DirectoryEntry::Header size mismatch");

        std::string filename;
        
        // Custom fields
        uint64_t position;          // offset of entry relative to image offset
        uint64_t offset;            // left/right offset from parent node
        std::filesystem::path path; // relative to ISO root directory
    };

    struct FileTime {
        uint32_t low;
        uint32_t high;

        FileTime();
    };
    static_assert(sizeof(FileTime) == 8, "FileTime size mismatch");

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
    };
    static_assert(sizeof(Xiso::Ecma119Header) == Xiso::SECTOR_SIZE + 7, "Ecma119 Header size mismatch");

    struct Header {
        char reserved1[XGD::OPTIMIZED_TAG_OFFSET];
        char optimized_tag[XGD::OPTIMIZED_TAG_LEN];
        char reserved2[Xiso::ECMA119_DATA_START - (sizeof(reserved1) + sizeof(optimized_tag))];

        Ecma119Header ecma119_header;

        char reserved3[Xiso::MAGIC_OFFSET - (Xiso::ECMA119_DATA_START + sizeof(Ecma119Header))];
        char magic1[20];
        uint32_t root_sector;
        uint32_t root_size;
        Xiso::FileTime file_time;
        char reserved4[Xiso::MAGIC_UNUSED_LEN];
        char magic2[20];

        Header(const uint32_t in_root_sector, const uint32_t in_root_size, const uint32_t in_total_sectors, const FileTime in_file_time);
    };
    static_assert(sizeof(Header) == Xiso::MAGIC_OFFSET + Xiso::SECTOR_SIZE, "Xiso Header size mismatch");
    static_assert(offsetof(Header, ecma119_header) == Xiso::ECMA119_DATA_START, "ECMA start offset mismatch");
    static_assert(offsetof(Header, magic1) == Xiso::MAGIC_OFFSET, "Xiso Header magic1 offset mismatch");

    #pragma pack(pop)

};

#endif // _XISO_H_