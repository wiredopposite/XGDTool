#ifndef _IMAGE_EXTRACTOR_H_
#define _IMAGE_EXTRACTOR_H_

#include <cstdint>
#include <filesystem>

#include "split_fstream.h"

#include "XGD.h"
#include "ImageReader.h"

class ImageExtractor {
public:
    ImageExtractor(std::shared_ptr<ImageReader> image_reader, const bool allowed_media_patch);
    ~ImageExtractor() {};

    void extract(const std::filesystem::path& out_dir_path);

private:
    std::shared_ptr<ImageReader> image_reader_{nullptr};  

    bool allowed_media_patch_{true};

    uint64_t prog_total_{0};
    uint64_t prog_processed_{0};
};

#endif // _IMAGE_EXTRACTOR_H_