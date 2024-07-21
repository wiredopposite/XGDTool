#include <cstdint>
#include <string>
#include <vector>
#include <filesystem>
#include <memory>
#include <fstream>

#include <lz4frame.h>

#include "Formats/CSO.h"
#include "ImageReader/ImageReader.h"

class CSOReader : public ImageReader
{
public:
    CSOReader(const std::vector<std::filesystem::path>& in_cso_paths);
    ~CSOReader() override;

    void read_sector(const uint32_t sector, char* out_buffer) override;
    void read_bytes(const uint64_t offset, const size_t size, char* out_buffer) override;

    uint64_t image_offset() override { return 0; };
    uint32_t total_sectors() override { return total_sectors_; }

    std::string name() override { return in_cso_paths_.front().filename().string(); };

private:
    struct IndexInfo 
    {
        uint32_t value;
        bool compressed;
    };

    const uint8_t LZ4F_HEADER[7] = { 0x04, 0x22, 0x4D, 0x18, 0x60, 0x40, 0x82 };
    const uint8_t LZ4F_FOOTER[4] = { 0x00, 0x00, 0x00, 0x00 };
    
    LZ4F_dctx* lz4f_dctx_{nullptr};

    std::vector<std::filesystem::path> in_cso_paths_;
    std::vector<std::unique_ptr<std::ifstream>> in_files_;

    size_t part_1_size_{0};
    std::vector<IndexInfo> index_infos_;

    uint32_t total_sectors_{0};

    void verify_and_populate_index_infos();
};