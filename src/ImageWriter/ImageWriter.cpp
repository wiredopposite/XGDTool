#include "ImageWriter/CCIWriter/CCIWriter.h"
#include "ImageWriter/CSOWriter/CSOWriter.h"
#include "ImageWriter/GoDWriter/GoDWriter.h"
#include "ImageWriter/XisoWriter/XisoWriter.h"
#include "ImageWriter/ZARWriter/ZARWriter.h"
#include "ImageWriter/ImageWriter.h"

void ImageWriter::create_directory(const std::filesystem::path& dir_path) 
{
    if (!std::filesystem::exists(dir_path)) 
    {
        try 
        { 
            std::filesystem::create_directories(dir_path); 
        } 
        catch (const std::filesystem::filesystem_error& e) 
        {
            throw XGDException(ErrCode::FS_MKDIR, HERE(), e.what());
        }
    }
}

namespace WriterFactory 
{
    std::unique_ptr<ImageWriter> create(const FileType out_file_type, 
                                        std::shared_ptr<ImageReader> image_reader, 
                                        TitleHelper& title_helper, 
                                        const OutputSettings& settings) 
    {
        switch (out_file_type) 
        {
            case FileType::ISO:
                return std::make_unique<XisoWriter>(image_reader, settings.scrub_type, settings.split);
            case FileType::ZAR:
                return std::make_unique<ZARWriter>(image_reader);
            case FileType::GoD:
                return std::make_unique<GoDWriter>(image_reader, title_helper, settings.scrub_type);
            case FileType::CSO:
                return std::make_unique<CSOWriter>(image_reader, settings.scrub_type);
            case FileType::CCI:
                return std::make_unique<CCIWriter>(image_reader, settings.scrub_type);
            default:
                throw std::runtime_error("Unknown file type");
        }
    }

    std::unique_ptr<ImageWriter> create(FileType out_file_type, 
                                        const std::filesystem::path& in_dir_path, 
                                        TitleHelper& title_helper, 
                                        const OutputSettings& settings) 
    {
        switch (out_file_type) 
        {
            case FileType::ISO:
                return std::make_unique<XisoWriter>(in_dir_path, settings.split);
            case FileType::ZAR:
                return std::make_unique<ZARWriter>(in_dir_path);
            case FileType::GoD:
                return std::make_unique<GoDWriter>(in_dir_path, title_helper);
            case FileType::CSO:
                return std::make_unique<CSOWriter>(in_dir_path);
            case FileType::CCI:
                return std::make_unique<CCIWriter>(in_dir_path);
            default:
                throw std::runtime_error("Unknown file type");
        }
    }
}