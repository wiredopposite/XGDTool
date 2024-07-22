#ifndef _IMAGE_READER_H_
#define _IMAGE_READER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

#include "Formats/Xiso.h"
#include "InputHelper/Types.h"

/*  Each derived class implements its own override methods for reading the filetype it's responsible for,
    ImageReader's virtual read_ methods should all produce the same results no matter the derived class.
    Derived classes all take the same constructor params, a vector of file paths to accommodate split 
    ISO/CCI/CSO images, for GoD, provide its root directory. */
class ImageReader 
{
public:
    virtual ~ImageReader() = default;

    virtual void read_sector(const uint32_t sector, char* out_buffer) = 0;
    virtual void read_bytes(const uint64_t offset, const size_t size, char* out_buffer) = 0;

    virtual uint64_t image_offset() = 0;
    virtual uint32_t total_sectors() = 0;
    virtual std::string name() = 0;

    const std::vector<Xiso::DirectoryEntry>& directory_entries();
    const Xiso::DirectoryEntry& executable_entry();
    const std::unordered_set<uint32_t>& data_sectors();

    uint32_t max_data_sector();
    uint64_t total_file_bytes();
    Platform platform();

private:
    std::vector<Xiso::DirectoryEntry> directory_entries_;
    Xiso::DirectoryEntry executable_entry_;
    std::unordered_set<uint32_t> data_sectors_;

    uint32_t max_data_sector_{0};
    uint64_t total_file_bytes_{0};
    Platform platform_{Platform::UNKNOWN};

    void populate_directory_entries(bool exe_only);
    void populate_data_sectors();
    bool get_security_sectors(std::unordered_set<uint32_t>& out_security_sectors);
};

namespace ReaderFactory {
    std::shared_ptr<ImageReader> create(FileType in_file_type, const std::vector<std::filesystem::path>& in_paths);
}

#endif // _IMAGE_READER_H_