#ifndef _IMAGE_EXTRACTOR_H_
#define _IMAGE_EXTRACTOR_H_

#include <cstdint>
#include <filesystem>

#include "SplitFStream/SplitFStream.h"

#include "XGD.h"
#include "ImageReader/ImageReader.h"

class ImageExtractor 
{
public:
    ImageExtractor(std::shared_ptr<ImageReader> image_reader, const bool allowed_media_patch);
    ~ImageExtractor() = default;

    void extract(const std::filesystem::path& out_dir_path);

private:
    std::shared_ptr<ImageReader> image_reader_{nullptr};  

    bool allowed_media_patch_{true};

    uint64_t prog_total_{0};
    uint64_t prog_processed_{0};

    void extract_file(const Xiso::DirectoryEntry& dir_entry);
    void extract_xbe_allowed_media_patch(const Xiso::DirectoryEntry& dir_entry);
};

#endif // _IMAGE_EXTRACTOR_H_