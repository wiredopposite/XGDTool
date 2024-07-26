#ifndef _CCI_READER_H_
#define _CCI_READER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>
#include <memory>
#include <fstream>

#include "Formats/CCI.h"
#include "ImageReader/ImageReader.h"

class CCIReader : public ImageReader 
{
public:
    CCIReader(const std::vector<std::filesystem::path>& in_cci_paths);
    ~CCIReader() override;

    void read_sector(const uint32_t sector, char* out_buffer) override;
    void read_bytes(const uint64_t offset, const size_t size, char* out_buffer) override;

    uint64_t image_offset() override { return 0; };
    uint32_t total_sectors() override { return total_sectors_; };

    std::string name() override { return in_cci_paths_.front().stem().string(); };

private:
    std::vector<std::filesystem::path> in_cci_paths_;
    std::vector<std::unique_ptr<std::ifstream>> in_files_;

    uint32_t total_sectors_{0};
    std::vector<std::vector<CCI::IndexInfo>> index_infos_;

    void verify_and_populate_index_infos();
};

#endif // _CCI_READER_H_