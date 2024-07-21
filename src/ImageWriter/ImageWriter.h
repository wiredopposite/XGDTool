#ifndef _IMAGE_WRITER_H_
#define _IMAGE_WRITER_H_

#include <cstdint>
#include <filesystem>
#include <memory>

#include "ImageReader/ImageReader.h"
#include "TitleHelper/TitleHelper.h"

enum class ScrubType { NONE, PARTIAL, FULL };

struct OutputSettings 
{
    ScrubType scrub_type{ScrubType::NONE};
    bool split{false};
    bool attach_xbe{false};
    bool allowed_media_patch{false};
    bool offline_mode{false};
    bool rename_xbe{false};
    LogLevel log_level{LogLevel::Normal};
};

class ImageWriter 
{
public:
    virtual ~ImageWriter() = default;   

    std::vector<std::filesystem::path> virtual convert(const std::filesystem::path& out_filepath) = 0;

protected:
    void create_directory(const std::filesystem::path& dir_path);
};

namespace WriterFactory 
{
    std::unique_ptr<ImageWriter> create(FileType out_file_type, 
                                        std::shared_ptr<ImageReader> image_reader, 
                                        std::unique_ptr<TitleHelper>& title_helper, 
                                        OutputSettings Output);

    std::unique_ptr<ImageWriter> create(FileType out_file_type, 
                                        const std::filesystem::path& in_dir_path, 
                                        std::unique_ptr<TitleHelper>& title_helper, 
                                        OutputSettings settings);
};

#endif // _IMAGE_WRITER_H_