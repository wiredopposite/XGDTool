#ifndef _IMAGE_READER_H_
#define _IMAGE_READER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

#include "Formats/Xiso.h"

enum class Platform { UNKNOWN, OGX, X360 };
enum class FileType { UNKNOWN, CCI, CSO, ISO, ZAR, DIR, GoD };

class ImageReader {
public:
    virtual ~ImageReader() {}

    virtual void read_sector(const uint32_t sector, char* out_buffer) = 0;
    virtual void read_bytes(const uint64_t offset, const size_t size, char* out_buffer) = 0;

    virtual const std::vector<Xiso::DirectoryEntry>& directory_entries() = 0;
    virtual const Xiso::DirectoryEntry& executable_entry() = 0;
    virtual const std::unordered_set<uint32_t>& data_sectors() = 0;

    virtual uint32_t max_data_sector() = 0;
    virtual uint64_t image_offset() = 0;
    virtual uint32_t total_sectors() = 0;
    virtual uint64_t total_file_bytes() = 0; // Files within dir table

    virtual Platform platform() = 0;
    virtual std::string name() = 0;
};

namespace ReaderFactory {
    std::shared_ptr<ImageReader> create(FileType in_file_type, const std::vector<std::filesystem::path>& in_paths);
}

#endif // _IMAGE_READER_H_