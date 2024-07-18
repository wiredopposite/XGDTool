#ifndef _IO_UTILS_H_
#define _IO_UTILS_H_

#include <filesystem>
#include <cstdint>
#include <fstream>

#include "SplitFStream/SplitFStream.h"

#pragma pack(push, 1)
struct FileTime {
    uint32_t low;
    uint32_t high;
};
#pragma pack(pop)
static_assert(sizeof(FileTime) == 8, "FileTime size mismatch");

namespace IOUtils {
    void pad_to_modulus(split::ofstream& out_file, uint32_t modulus, char pad_byte);
    void pad_to_modulus(std::ofstream& out_file, uint32_t modulus, char pad_byte);
    bool get_filetime(FileTime &file_time);
};

#endif // _IO_UTILS_H_