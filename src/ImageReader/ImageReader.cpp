#include "ImageReader/XisoReader/XisoReader.h"
#include "ImageReader/CCIReader/CCIReader.h"
#include "ImageReader/ZARReader/ZARReader.h"
#include "ImageReader/GoDReader/GoDReader.h"
#include "XGD.h"
#include "ImageReader.h"

std::shared_ptr<ImageReader> ReaderFactory::create(FileType in_file_type, const std::vector<std::filesystem::path>& paths) {
    switch (in_file_type) {
        case FileType::CCI:
            return std::make_shared<CCIReader>(paths);
        case FileType::ISO:
            return std::make_shared<XisoReader>(paths);
        case FileType::GoD:
            return std::make_shared<GoDReader>(paths);
        // case FileType::ZAR:
        //     return std::make_shared<ZARReader>(paths);
        default:
            throw XGDException(ErrCode::ISO_INVALID, HERE(), "Unsupported ImageReader file type");
    }
}