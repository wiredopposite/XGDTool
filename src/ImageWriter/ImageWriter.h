#ifndef _IMAGE_WRITER_H_
#define _IMAGE_WRITER_H_

#include <cstdint>
#include <filesystem>
#include <memory>

#include "ImageReader/ImageReader.h"
#include "TitleHelper/TitleHelper.h"
#include "InputHelper/Types.h"

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
    std::unique_ptr<ImageWriter> create(const FileType out_file_type, 
                                        std::shared_ptr<ImageReader> image_reader, 
                                        TitleHelper& title_helper, 
                                        const OutputSettings& Output);

    std::unique_ptr<ImageWriter> create(const FileType out_file_type, 
                                        const std::filesystem::path& in_dir_path, 
                                        TitleHelper& title_helper, 
                                        const OutputSettings& settings);
};

#endif // _IMAGE_WRITER_H_