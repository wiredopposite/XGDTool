#ifndef _GOD_READER_H_
#define _GOD_READER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

#include "Formats/GoD.h"
#include "ImageReader/ImageReader.h"

class GoDReader : public ImageReader {
public:
    GoDReader(const std::vector<std::filesystem::path>& in_god_directory);
    ~GoDReader() override;

    void read_sector(const uint32_t sector, char* out_buffer) override;
    void read_bytes(const uint64_t offset, const size_t size, char* out_buffer) override;
    
    const std::vector<Xiso::DirectoryEntry>& directory_entries() override;
    const std::unordered_set<uint32_t>& data_sectors() override;
    const Xiso::DirectoryEntry& executable_entry() override { return executable_entry_; };

    uint32_t max_data_sector() override;
    uint64_t image_offset() override { return 0; };
    uint32_t total_sectors() override { return total_sectors_; }
    uint64_t total_file_bytes() override;

    Platform platform() override { return platform_; };
    std::string name() override { return in_god_directory_.filename().string(); };

private:
    struct Remap {
        uint64_t offset;
        uint32_t file_index;
    };

    std::filesystem::path in_god_directory_;
    std::vector<std::filesystem::path> in_god_data_paths_;
    std::vector<std::unique_ptr<std::ifstream>> in_files_;

    std::vector<Xiso::DirectoryEntry> directory_entries_;
    Xiso::DirectoryEntry executable_entry_;
    std::unordered_set<uint32_t> data_sectors_;
    uint32_t max_data_sector_{0};
    Platform platform_{Platform::UNKNOWN};
    uint32_t total_sectors_{0};
    uint64_t total_file_bytes_{0};

    void populate_data_files(const std::filesystem::path& in_directory, int search_depth);

    Remap remap_sector(uint64_t xiso_sector);
    Remap remap_offset(uint64_t xiso_offset);

    void populate_directory_entries(bool exe_only);
    void populate_data_sectors();
    void get_security_sectors(const std::unordered_set<uint32_t>& data_sectors, std::unordered_set<uint32_t>& out_security_sectors, bool compare_mode);
};

#endif // _GOD_READER_H_