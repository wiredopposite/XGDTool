#include "ImageWriter/CCIWriter/CCIWriter.h"
#include "ImageWriter/CSOWriter/CSOWriter.h"
#include "ImageWriter/GoDWriter/GoDWriter.h"
#include "ImageWriter/XisoWriter/XisoWriter.h"
#include "ImageWriter/ZARWriter/ZARWriter.h"
#include "ImageWriter/ImageWriter.h"

void ImageWriter::create_directory(const std::filesystem::path& dir_path) {
    if (!std::filesystem::exists(dir_path)) {
        try { 
            std::filesystem::create_directories(dir_path); 
        } catch (const std::filesystem::filesystem_error& e) {
            XGDException(ErrCode::FS_MKDIR, HERE(), e.what());
        }
    }
}

namespace WriterFactory {
    std::unique_ptr<ImageWriter> create(FileType out_file_type, std::shared_ptr<ImageReader> image_reader, ScrubType scrub_type, bool split, bool allowed_media_patch) {
        switch (out_file_type) {
            case FileType::ISO:
                return std::make_unique<XisoWriter>(image_reader, scrub_type, split, allowed_media_patch);
            case FileType::ZAR:
                return std::make_unique<ZARWriter>(image_reader);
            case FileType::GoD:
                return std::make_unique<GoDWriter>(image_reader, scrub_type, allowed_media_patch);
            case FileType::CSO:
                return std::make_unique<CSOWriter>(image_reader, scrub_type, allowed_media_patch);
            case FileType::CCI:
                return std::make_unique<CCIWriter>(image_reader, scrub_type, allowed_media_patch);
            default:
                throw std::runtime_error("Unknown file type");
        }
    }

    std::unique_ptr<ImageWriter> create(FileType out_file_type, const std::filesystem::path& in_dir_path, bool split, bool allowed_media_patch) {
        switch (out_file_type) {
            case FileType::ISO:
                return std::make_unique<XisoWriter>(in_dir_path, split, allowed_media_patch);
            case FileType::ZAR:
                return std::make_unique<ZARWriter>(in_dir_path);
            case FileType::GoD:
                return std::make_unique<GoDWriter>(in_dir_path, allowed_media_patch);
            case FileType::CSO:
                return std::make_unique<CSOWriter>(in_dir_path, allowed_media_patch);
            case FileType::CCI:
                return std::make_unique<CCIWriter>(in_dir_path, allowed_media_patch);
            default:
                throw std::runtime_error("Unknown file type");
        }
    }
}