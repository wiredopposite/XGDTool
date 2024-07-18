#ifndef _IMAGE_WRITER_H_
#define _IMAGE_WRITER_H_

#include <cstdint>
#include <filesystem>
#include <memory>

#include "ImageReader/ImageReader.h"

enum class ScrubType { NONE, PARTIAL, FULL };

class ImageWriter {
public:
    virtual ~ImageWriter() = default;   

    std::vector<std::filesystem::path> virtual convert(const std::filesystem::path& out_filepath) = 0;

    void create_directory(const std::filesystem::path& dir_path);
};

namespace WriterFactory {
    std::unique_ptr<ImageWriter> create(FileType out_file_type, std::shared_ptr<ImageReader> image_reader, ScrubType scrub_type, bool split, bool allowed_media_patch);
    std::unique_ptr<ImageWriter> create(FileType out_file_type, const std::filesystem::path& in_dir_path, bool split, bool allowed_media_patch);
};

#endif // _IMAGE_WRITER_H_