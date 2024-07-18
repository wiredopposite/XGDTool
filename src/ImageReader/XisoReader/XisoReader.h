#ifndef _XISO_READER_H_
#define _XISO_READER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

#include "SplitFStream/SplitFStream.h"

#include "Formats/Xiso.h"
#include "ImageReader/ImageReader.h"

class XisoReader : public ImageReader {
public:
    XisoReader(const std::vector<std::filesystem::path>& in_xiso_paths);
    ~XisoReader() override;

    void read_sector(const uint32_t sector, char* out_buffer) override;
    void read_bytes(const uint64_t offset, const size_t size, char* out_buffer) override;

    std::vector<Xiso::DirectoryEntry>& directory_entries() override;
    Xiso::DirectoryEntry& executable_entry() override { return executable_entry_; };
    std::unordered_set<uint32_t>& data_sectors() override;
    
    uint32_t max_data_sector() override;
    uint64_t image_offset() override { return image_offset_; };
    uint32_t total_sectors() override { return total_sectors_; };
    uint64_t total_file_bytes() override;

    Platform platform() override { return platform_; };
    std::string name() override { return in_xiso_paths_.front().stem().string(); };

private:
    std::vector<std::filesystem::path> in_xiso_paths_;
    split::ifstream in_file_;

    std::vector<Xiso::DirectoryEntry> directory_entries_;
    Xiso::DirectoryEntry executable_entry_;
    std::unordered_set<uint32_t> data_sectors_;
    uint32_t max_data_sector_{0};
    uint64_t image_offset_{0};
    Platform platform_{Platform::UNKNOWN};
    uint32_t total_sectors_{0};
    uint64_t total_file_bytes_{0};

    uint64_t get_image_offset();
    void populate_directory_entries(bool exe_only);
    void populate_data_sectors();
    void get_security_sectors(const std::unordered_set<uint32_t>& data_sectors, std::unordered_set<uint32_t>& out_security_sectors);
};

#endif // _XISO_READER_H_