#include "Utils/EndianUtils.h"

namespace EndianUtils {

inline bool is_big_endian() {
    union {
        uint32_t i;
        char c[4];
    } bint = {0x01020304};
    return bint.c[0] == 1;
}

inline void swap_16(uint16_t &value) { 
    value = (value << 8) | (value >> 8);
}

inline void swap_32(uint32_t &value) { 
    value = ((value << 24) & 0xFF000000) | ((value << 8 ) & 0x00FF0000) |
            ((value >> 8)  & 0x0000FF00) | ((value >> 24) & 0x000000FF) ;
}

void big_16(uint16_t &value) { 
    if (!is_big_endian()) {
        swap_16(value);
    }
}

void big_32(uint32_t &value) { 
    if (!is_big_endian()) {
        swap_32(value);
    }
}

void little_16(uint16_t &value) { 
    if (is_big_endian()) {
        swap_16(value);
    }
}

void little_32(uint32_t &value) { 
    if (is_big_endian()) {
        swap_32(value);
    }
}

void swap_endian(uint32_t &value) {
    value = ((value >> 24) & 0x000000FF) |
            ((value >> 8)  & 0x0000FF00) |
            ((value << 8)  & 0x00FF0000) |
            ((value << 24) & 0xFF000000);
}

void swap_endian(uint16_t &value) {
    value = ((value >> 8) & 0x00FF) |
            ((value << 8) & 0xFF00);
}

}; // 