#ifndef _GOD_READER_H_
#define _GOD_READER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

#include "Formats/GoD.h"
#include "ImageReader/ImageReader.h"

class GoDReader : public ImageReader 
{
public:
    GoDReader(const std::vector<std::filesystem::path>& in_god_directory);
    ~GoDReader() override;

    void read_sector(const uint32_t sector, char* out_buffer) override;
    void read_bytes(const uint64_t offset, const size_t size, char* out_buffer) override;

    uint64_t image_offset() override { return 0; };
    uint32_t total_sectors() override { return total_sectors_; }

    std::string name() override { return in_god_directory_.filename().string(); };

private:
    struct Remap 
    {
        uint64_t offset;
        uint32_t file_index;
    };

    std::filesystem::path in_god_directory_;
    std::vector<std::filesystem::path> in_god_data_paths_;
    std::vector<std::unique_ptr<std::ifstream>> in_files_;

    uint32_t total_sectors_{0};

    void populate_data_files(const std::filesystem::path& in_directory, int search_depth);

    Remap remap_sector(uint64_t xiso_sector);
    Remap remap_offset(uint64_t xiso_offset);
};

#endif // _GOD_READER_H_