#ifndef _XISO_READER_H_
#define _XISO_READER_H_

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_set>

#include "SplitFStream/SplitFStream.h"

#include "Formats/Xiso.h"
#include "ImageReader/ImageReader.h"

class XisoReader : public ImageReader 
{
public:
    XisoReader(const std::vector<std::filesystem::path>& in_xiso_paths);
    ~XisoReader() override;

    void read_sector(const uint32_t sector, char* out_buffer) override;
    void read_bytes(const uint64_t offset, const size_t size, char* out_buffer) override;

    uint64_t image_offset() override { return image_offset_; };
    uint32_t total_sectors() override { return total_sectors_; };

    std::string name() override { return in_xiso_paths_.front().stem().string(); };

private:
    std::vector<std::filesystem::path> in_xiso_paths_;
    split::ifstream in_file_;

    uint64_t image_offset_{0};
    uint32_t total_sectors_{0};

    uint64_t get_image_offset();
};

#endif // _XISO_READER_H_