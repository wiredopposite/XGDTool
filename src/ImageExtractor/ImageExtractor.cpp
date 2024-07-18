#include "ImageExtractor/ImageExtractor.h"

ImageExtractor::ImageExtractor(std::shared_ptr<ImageReader> image_reader, const bool allowed_media_patch)
    : image_reader_(image_reader), allowed_media_patch_(allowed_media_patch) {}

void ImageExtractor::extract(const std::filesystem::path& out_dir_path) {
    const std::vector<Xiso::DirectoryEntry>& dir_entries = image_reader_->directory_entries();
    
    prog_total_ = image_reader_->total_file_bytes();
    prog_processed_ = 0;

    if (!std::filesystem::exists(out_dir_path)) {
        std::filesystem::create_directories(out_dir_path);
    }

    auto cwd = std::filesystem::current_path();
    std::filesystem::current_path(out_dir_path);

    for (auto& dir_entry : dir_entries) {
        if (dir_entry.header.attributes & Xiso::ATTRIBUTE_DIRECTORY) {
            if (!dir_entry.path.empty() && !std::filesystem::exists(dir_entry.path)) {
                std::filesystem::create_directories(dir_entry.path);
            }
        } else {
            if (!dir_entry.path.parent_path().empty() && !std::filesystem::exists(dir_entry.path.parent_path())) {
                std::filesystem::create_directories(dir_entry.path.parent_path());
            }

            std::ofstream out_file(dir_entry.path, std::ios::binary);
            if (!out_file.is_open()) {
                throw XGDException(ErrCode::FILE_OPEN, HERE(), "Failed to open file for writing: " + dir_entry.path.string());
            }

            std::vector<char> buffer(Xiso::SECTOR_SIZE);
            uint32_t bytes_remaining = dir_entry.header.file_size;
            uint64_t read_position = static_cast<uint64_t>(dir_entry.header.start_sector) * Xiso::SECTOR_SIZE;

            while (bytes_remaining > 0) {
                uint32_t read_size = std::min(bytes_remaining, Xiso::SECTOR_SIZE);

                image_reader_->read_bytes(read_position, read_size, buffer.data());

                out_file.write(buffer.data(), read_size);
                if (!out_file) {
                    throw XGDException(ErrCode::FILE_WRITE, HERE(), "Failed to write to file: " + dir_entry.path.string());
                }

                bytes_remaining -= read_size;
                read_position += read_size;

                XGDLog().print_progress(prog_processed_ += read_size, prog_total_);
            }

            out_file.close();
        }
    }

    std::filesystem::current_path(cwd);
}