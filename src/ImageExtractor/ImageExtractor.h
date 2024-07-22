#ifndef _IMAGE_EXTRACTOR_H_
#define _IMAGE_EXTRACTOR_H_

#include <cstdint>
#include <filesystem>

#include "SplitFStream/SplitFStream.h"

#include "XGD.h"
#include "Formats/Xiso.h"
#include "ImageReader/ImageReader.h"
#include "TitleHelper/TitleHelper.h"

class ImageExtractor 
{
public:
    ImageExtractor(ImageReader& image_reader, TitleHelper& title_helper, const bool allowed_media_patch, const bool rename_xbe);
    ~ImageExtractor() = default;

    void extract(const std::filesystem::path& out_dir_path);

private:
    ImageReader& image_reader_;  
    TitleHelper& title_helper_;

    bool allowed_media_patch_{true};
    bool rename_xbe_{false};

    uint64_t prog_total_{0};
    uint64_t prog_processed_{0};

    void extract_file(const Xiso::DirectoryEntry& dir_entry);
    void extract_file_xbe_patch(const Xiso::DirectoryEntry& dir_entry);
};

#endif // _IMAGE_EXTRACTOR_H_