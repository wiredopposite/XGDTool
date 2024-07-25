#ifndef _CSO_H_
#define _CSO_H_

#include <cstdint>

namespace CSO {

    constexpr char     MAGIC[] = "CISO";
    constexpr int      MAGIC_LEN = 4;
    const     uint32_t HEADER_SIZE = 24;
    const     uint32_t BLOCK_SIZE = 2048;
    const     uint8_t  VERSION = 2;
    const     uint8_t  INDEX_ALIGNMENT = 2;
    constexpr uint64_t SPLIT_OFFSET = 0xFFBF6000;
    constexpr uint32_t FILE_MODULUS = 0x400;

    #pragma pack(push, 1)
    struct Header {
        char magic[4]               {'C', 'I', 'S', 'O'};
        uint32_t header_size        {CSO::HEADER_SIZE};
        uint64_t uncompressed_size;
        uint32_t block_size         {CSO::BLOCK_SIZE};
        uint8_t version             {CSO::VERSION};
        uint8_t index_alignment     {CSO::INDEX_ALIGNMENT};
        uint16_t reserved           {0};

        Header() = default;
        Header(uint64_t in_uncompressed_size) 
            : uncompressed_size(in_uncompressed_size) {}
    };
    #pragma pack(pop)
    static_assert(sizeof(Header) == 24, "CSO Header size mismatch");

};

#endif // _CSO_H_