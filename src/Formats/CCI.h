#ifndef _CCI_H_
#define _CCI_H_

#include <cstdint>
#include <vector>
#include <fstream>

namespace CCI {

    constexpr char     MAGIC[] = "CCIM";
    constexpr int      MAGIC_LEN = 4;
    const     uint32_t HEADER_SIZE = 32;
    const     uint32_t BLOCK_SIZE = 2048;
    const     uint8_t  VERSION = 1;
    const     uint8_t  INDEX_ALIGNMENT = 2;
    constexpr uint64_t SPLIT_OFFSET = 0xFF000000;

    struct IndexInfo {
        uint32_t value;
        bool compressed;
    };

    #pragma pack(push, 1)
    struct Header {
        char magic[4]               {'C', 'C', 'I', 'M'};
        uint32_t header_size        {CCI::HEADER_SIZE};
        uint64_t uncompressed_size{0};
        uint64_t index_offset{0};
        uint32_t block_size         {CCI::BLOCK_SIZE};
        uint8_t version             {CCI::VERSION};
        uint8_t index_alignment     {CCI::INDEX_ALIGNMENT};
        uint16_t reserved           {0};

        Header() = default;
        Header(uint64_t in_uncompressed_size, uint64_t in_index_offset) 
            : uncompressed_size(in_uncompressed_size), index_offset(in_index_offset) {};
    };
    #pragma pack(pop)
    static_assert(sizeof(Header) == CCI::HEADER_SIZE, "CCI Header size mismatch");

};

#endif // _CCI_H_