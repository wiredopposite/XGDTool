#ifndef _CCI_READER_H_
#define _CCI_READER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <fstream>

#include "Formats/CCI.h"
#include "ImageReader/ImageReader.h"

class CCIReader : public ImageReader {
public:
    CCIReader(const std::vector<std::filesystem::path>& in_cci_paths);
    ~CCIReader() override;

    void read_sector(const uint32_t sector, char* out_buffer) override;
    void read_bytes(const uint64_t offset, const size_t size, char* out_buffer) override;

    const std::vector<Xiso::DirectoryEntry>& directory_entries() override;
    const Xiso::DirectoryEntry& executable_entry() override { return executable_entry_; }
    const std::unordered_set<uint32_t>& data_sectors() override;

    uint32_t max_data_sector() override;
    uint64_t image_offset() override { return 0; };
    uint32_t total_sectors() override { return total_sectors_; };
    uint64_t total_file_bytes() override;

    Platform platform() override { return Platform::OGX; };
    std::string name() override { return in_cci_paths_.front().stem().string(); };

private:
    std::vector<std::filesystem::path> in_cci_paths_;
    std::vector<std::unique_ptr<std::ifstream>> in_files_;

    std::vector<Xiso::DirectoryEntry> directory_entries_;
    Xiso::DirectoryEntry executable_entry_;
    std::unordered_set<uint32_t> data_sectors_;
    uint32_t max_data_sector_{0};
    uint32_t total_sectors_{0};
    uint64_t total_file_bytes_{0};

    uint32_t part_1_sectors_{0};
    std::vector<CCI::IndexInfo> index_infos_;

    void verify_and_populate_index_infos();
    void populate_directory_entries(bool exe_only);
    void populate_data_sectors();
    void get_security_sectors(const std::unordered_set<uint32_t>& data_sectors, std::unordered_set<uint32_t>& out_security_sectors, bool compare_mode);
};

#endif // _CCI_READER_H_