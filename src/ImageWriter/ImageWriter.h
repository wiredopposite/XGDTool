#ifndef _IMAGE_WRITER_H_
#define _IMAGE_WRITER_H_

#include <cstdint>
#include <filesystem>
#include <vector>
#include <memory>
#include <atomic>

#include "ImageReader/ImageReader.h"
#include "TitleHelper/TitleHelper.h"
#include "InputHelper/Types.h"
#include "AvlTree/AvlIterator.h"

class ImageWriter 
{
public:
    virtual ~ImageWriter() = default;   

    static std::unique_ptr<ImageWriter> create_instance(std::shared_ptr<ImageReader> image_reader, TitleHelper& title_helper, const OutputSettings& out_settings); 
    static std::unique_ptr<ImageWriter> create_instance(const std::filesystem::path& in_dir_path, TitleHelper& title_helper, const OutputSettings& out_settings);

    virtual std::vector<std::filesystem::path> convert(const std::filesystem::path& out_filepath) = 0;

    void cancel_processing() { write_cancel_flag_ = true; }
    void pause_processing() { write_pause_flag_ = true; }
    void resume_processing() { write_pause_flag_ = false; }

protected:
    std::atomic<bool> write_cancel_flag_{false};
    std::atomic<bool> write_pause_flag_{false};

    void check_status_flags();
    Xiso::DirectoryEntry::Header get_directory_entry_header(const AvlTree::Node& node);
    size_t write_directory_to_buffer(const std::vector<AvlIterator::Entry>& avl_entries, const size_t start_index, std::vector<char>& entry_buffer);
    void create_directory(const std::filesystem::path& dir_path);
    uint32_t num_sectors(const uint64_t num_bytes);
};

#endif // _IMAGE_WRITER_H_