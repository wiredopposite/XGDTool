#ifndef _GOD_H_
#define _GOD_H_

#include <cstdint>

/* GoD file structure:
- master hashtable block
- sub hashtable block
- 204 data blocks
- sub hashtable block
- 204 data blocks
- sub hashtable block
- 204 data blocks 
...and so forth
Maximum of 203 sub hashtables per Data file */
namespace GoD {

    static constexpr uint32_t BLOCK_SIZE = 0x1000;
    static constexpr uint32_t BLOCKS_PER_PART = 41616;
    static constexpr uint32_t DATA_BLOCKS_PER_SHT = 204;
    // static constexpr uint32_t FREE_SECTOR = 0x24;
    static constexpr uint32_t SHT_PER_MHT = 203;
    static constexpr uint32_t DATA_BLOCKS_PER_PART = DATA_BLOCKS_PER_SHT * SHT_PER_MHT;
    // static constexpr uint64_t LIVE_HEADER_SIZE = 0xb000;

    struct Type {
        static constexpr uint32_t GAMES_ON_DEMAND = 0x7000;
        static constexpr uint32_t ORIGINAL_XBOX = 0x5000;
    };

};

#endif // _GOD_H_